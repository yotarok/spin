#include <spin/nnet/affine.hpp>
#include <viennacl/linalg/norm_2.hpp>

namespace spin {

  affine_delta::affine_delta(size_t nout, size_t nin) 
    : _noutput(nout), _ninput(nin) {
    _dweight = viennacl::zero_matrix<float>(_noutput, _ninput);
    _dbias = viennacl::zero_matrix<float>(_noutput, 1);
  }
    
  void affine_delta::add_regularizer(const_node_config_ptr conf,
                                     std::shared_ptr<const nnet_node> pnode) {
    auto p = dynamic_cast<const nnet_node_affine_transform*>(pnode.get());
    auto pconf = dynamic_cast<const affine_config*>(conf.get());
    if (p == 0) {
      throw std::runtime_error("companion type mistmatch (reg; affine expected)");
    }

    if (pconf->has_l2_regularizer() > 0) {
      _dweight += pconf->l2_regularizer() * p->weight();
    }
    if (pconf->has_bias_l2_regularizer() > 0) {
      _dbias += pconf->bias_l2_regularizer() * p->bias();
    }

  }

  affine_config::affine_config()
    : node_config(), _bias_l2reg(0), _maxnorm(HUGE_VALF) {
  }

  bool affine_config::has_bias_l2_regularizer() const {
    return _bias_l2reg > 0;
  }

  bool affine_config::has_maxnorm() const {
    return _maxnorm < BIG_VALF;
  }

  void affine_context::accumulate_delta(node_delta_ptr pdelta) {
    affine_delta* p = dynamic_cast<affine_delta*>(pdelta.get());
    if (p == 0) {
      throw std::runtime_error("companion type mistmatch (ac; affine expected)");
    }
    
    for (int n = 0; n < _dinput.nbatch(); ++ n) {
      nnet_submat dout = NNET_BATCH(_doutput, n);
      p->get_dweight() +=
        viennacl::linalg::prod(dout,
                               viennacl::trans(NNET_BATCH(_input, n)));
      p->get_dbias() +=
        viennacl::linalg::prod(dout,
                               ones(dout.size2(), 1));
    }
  }

  static const char * maxnorm_source =
    "__kernel void maxnorm("
    "          __global float * mat, __global float* bias, float maxnorm, "
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2) {"
    " for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    "  float norm2 = bias[i]; //0.0f; \n"
    "#pragma unroll 128\n"
    "  for (unsigned int j = 0; j < size2; ++ j) {"
    "    float v = mat[i + stride * j]; norm2 += v * v;"
    "  }"
    "  float norm = sqrt(norm2);"
    "  float scale = (norm < maxnorm) ? 1.0 : maxnorm / norm; \n"
    "#pragma unroll 128\n"
    "  for (unsigned int j = 0; j < size2; ++ j) {"
    "    mat[i + stride * j] *= scale;"
    "  } bias[i] *= scale;"
    " }"
    "}";

  nnet_node_affine_transform::nnet_node_affine_transform(const std::string& n)
    : nnet_node(n),
      _prog(viennacl::ocl::current_context().add_program(maxnorm_source,
                                                         "maxnorm")),
      _maxnorm_kernel(_prog.get_kernel("maxnorm")) {
  }
      
  void nnet_node_affine_transform::feed_forward(const_node_config_ptr pcfg,
                                                node_context_ptr pctx) const {
    affine_context* p = dynamic_cast<affine_context*>(pctx.get());
    if (p == 0) {
      throw std::runtime_error("companion type mistmatch (ff; affine expected)");
    }
    
    for (int n = 0; n < p->get_input().nbatch(); ++ n) {
      int D = _weight.size1();
      
      nnet_submat inp = NNET_BATCH(p->get_input(), n);
      int T = inp.size2();

      p->get_output().set_size(n, T);
      nnet_submat out = NNET_BATCH(p->get_output(), n); 
      viennacl::slice rows(0, 1, D), cols(0, 0, T);
      out = viennacl::linalg::prod(_weight, inp) +
        viennacl::project(_bias, rows, cols);

      if (false) { // this->name() == "aff6") {
        std::cout << this->name()  << std::endl;
        size_t Din = _weight.size2();
        std::cout << Din << " x " << T << std::endl;
        fmatrix debug_inp(Din, T);
        viennacl::copy(inp, debug_inp);
        std::cout << "INP: " << debug_inp.transpose().row(0) << std::endl;

        std::cout << D << " x " << T << std::endl;
        fmatrix debug_out(D, T);
        viennacl::copy(out, debug_out);
        std::cout << "OUT: " << debug_out.transpose().row(0) << std::endl;
      }
    }
  }
  
  void nnet_node_affine_transform::back_propagate(const_node_config_ptr pcfg,
                                                  node_context_ptr pctx) const {
    affine_context* p = dynamic_cast<affine_context*>(pctx.get());
    if (p == 0) {
      throw std::runtime_error("companion type mistmatch (bp; affine expected)");
    }
    
    for (int n = 0; n < p->get_diff_output().nbatch(); ++ n) {
      nnet_submat dout = NNET_BATCH(p->get_diff_output(), n);
      int T = dout.size2();
      p->get_diff_input().set_size(n, T);
      nnet_submat dinp = NNET_BATCH(p->get_diff_input(), n);
      dinp = viennacl::linalg::prod(viennacl::trans(_weight),
                                    dout);
    }
  }
  
  void nnet_node_affine_transform::update(const node_config& config,
                                          const node_delta& delta) {
    const affine_delta& d = dynamic_cast<const affine_delta&>(delta);
    const affine_config& c = dynamic_cast<const affine_config&>(config);
    if (c.has_maxnorm()) {
      int rows = _weight.size1(), cols = _weight.size2();
      _maxnorm_kernel.local_work_size(0, 16);
      _maxnorm_kernel.global_work_size(0, ((rows / 16) + 1) * 16);
      
      _weight -= config.learn_rate() * d.get_dweight();
      _bias -= config.learn_rate() * d.get_dbias();
      viennacl::ocl::enqueue(_maxnorm_kernel(_weight, _bias,
                                             cl_float(c.maxnorm()),
                                             cl_uint(_weight.internal_size1()),
                                             cl_uint(_weight.size1()),
                                             cl_uint(_weight.size2())));
    } else {
      _weight -= config.learn_rate() * d.get_dweight();
      _bias -= config.learn_rate() * d.get_dbias();
    }

#if 0
    fmatrix wmat(_weight.size1(), _weight.size2());
    viennacl::copy(_weight, wmat);
    std::cout << name() << " weight range = " << wmat.minCoeff() << ":" << wmat.maxCoeff() << std::endl;
    fmatrix bmat(_bias.size1(), _bias.size2());
    viennacl::copy(_bias, bmat);
    std::cout << name() << " bias range = " << bmat.minCoeff() << ":" << bmat.maxCoeff() << std::endl;
#endif
  }

  void nnet_node_affine_transform::
  initialize(const std::map<std::string, std::string>& ps) {
    int ninp = boost::lexical_cast<int>(get(ps, "ninputs"));
    int nout = boost::lexical_cast<int>(get(ps, "noutputs"));
    int seed = boost::lexical_cast<int>(get_or_else(ps, std::string("randseed"),
                                                    std::string("17")));
    float scale =
      boost::lexical_cast<float>(get_or_else(ps,
                                             std::string("randscale"),
                                             std::string("1.0")));
    fmatrix wmat(nout, ninp);
    boost::mt19937 rng(seed);
    //boost::normal_distribution<> nd(0.0, scale / std::sqrt((float)ninp));
    float stdrange = std::sqrt(6.0f) / std::sqrt((float)(ninp + nout));
    boost::random::uniform_real_distribution<float> nd(- scale * stdrange, scale * stdrange);
    
    for (int col = 0; col < ninp; ++ col) {
      for (int row = 0; row < nout; ++ row) {
        wmat(row, col) = nd(rng);
      }
    }
    fmatrix bmat = fmatrix::Zero(nout, 1); // TO DO: random init?
    viennacl::copy(wmat, _weight);
    viennacl::copy(bmat, _bias);
  }

}

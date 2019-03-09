#include <spin/nnet/dropout.hpp>

namespace spin {
    static const char * dropout_source =
    "__kernel void binarize("
    "          __global float * mat,"
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2,"
    "          float fraction) {"
    "  for (unsigned int j = get_global_id(1); j < size2; j += get_global_size(1)) {"
    "    for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    "    mat[i + stride * j] = (mat[i + stride * j] < fraction) ? 1.0f : 0.0f;"
    "    } "
    "  } "
    "} \n";

  dropout_config::dropout_config(uint32_t seed_offset)
    : node_config(), _training(false), _seed_offset(seed_offset) {
  }

  bool dropout_config::is_training() const {
    return _training;
  }

  uint32_t dropout_config::seed() const {
    return _seed;
  }
  
  bool dropout_config::set_update_parameter(const std::string& k,
                                            const std::string& v) {
    if (k == "training") {
      if (v == "true" || v == "1") {
        _training = true;
      } else {
        _training = false;
      }
    } else if (k == "seed") {
      _seed = boost::lexical_cast<uint32_t>(v) + _seed_offset;
    } else {
      return node_config::set_update_parameter(k, v);
    }
    
    return true;
  }                                     

  dropout_context::dropout_context(int maxnbatch, int maxbatchsize,
                                   int dim)
    : node_basic_context(maxnbatch, maxbatchsize, dim, dim),
      _mask(maxnbatch, maxbatchsize, dim), _rng(512),
      _rng_initialized(false) {
  }

  nnet_node_dropout::nnet_node_dropout(const std::string& n)
    : nnet_simple_activation(n),
      _prog(viennacl::ocl::current_context().add_program(dropout_source,
                                                         "dropout")),
      _binarize_kernel(_prog.get_kernel("binarize")) {
  }

  void nnet_node_dropout::feed_forward(const_node_config_ptr pcfg,
                                       node_context_ptr pctx) const {
    dropout_context& ctx = dynamic_cast<dropout_context&>(*pctx);
    const dropout_config& cfg = dynamic_cast<const dropout_config&>(*pcfg);

    for (int n = 0; n < pctx->get_input().nbatch(); ++ n) {
      if (cfg.is_training()) {
        if (! ctx.is_rng_initialized()) { ctx.initialize_rng(cfg.seed()); }

        nnet_submat inp = NNET_BATCH(ctx.get_input(), n);
        int D = inp.size1(), T = inp.size2();

        ctx.get_mask().set_size(n, T);
        nnet_submat mask = NNET_BATCH(ctx.get_mask(), n);
        ctx.get_rng().draw_uniform(&mask);

        _binarize_kernel.local_work_size(0, 16);
        _binarize_kernel.global_work_size(0, ((D / 16) + 1) * 16);
        _binarize_kernel.local_work_size(1, 1);
        _binarize_kernel.global_work_size(1, T);

        viennacl::ocl::enqueue(_binarize_kernel(mask,
                                                cl_uint(mask.internal_size1()),
                                                cl_uint(mask.size1()),
                                                cl_uint(mask.size2()),
                                                _fraction));
        

        pctx->get_output().set_size(n, T);
        nnet_submat out = NNET_BATCH(ctx.get_output(), n);
        out = viennacl::linalg::element_prod(inp, mask);
      } else {
        nnet_submat inp = NNET_BATCH(ctx.get_input(), n);
        int D = inp.size1(), T = inp.size2();
        pctx->get_output().set_size(n, T);
        nnet_submat out = NNET_BATCH(ctx.get_output(), n);
        out = _fraction * inp;
      }
    }
  }

  void nnet_node_dropout::back_propagate(const_node_config_ptr pcfg,
                                         node_context_ptr pctx) const {
    dropout_context& ctx = dynamic_cast<dropout_context&>(*pctx);
    const dropout_config& cfg = dynamic_cast<const dropout_config&>(*pcfg);

    for (int n = 0; n < pctx->get_input().nbatch(); ++ n) {
      if (cfg.is_training()) {
        nnet_submat dout = NNET_BATCH(pctx->get_diff_output(), n);
        int D = dout.size1(), T = dout.size2();
        pctx->get_diff_input().set_size(n, T);
        nnet_submat mask = NNET_BATCH(ctx.get_mask(), n);
        nnet_submat dinp = NNET_BATCH(pctx->get_diff_input(), n);
        dinp = viennacl::linalg::element_prod(dout, mask);
      }
      else {
        nnet_submat dout = NNET_BATCH(pctx->get_diff_output(), n);
        int D = dout.size1(), T = dout.size2();
        pctx->get_diff_input().set_size(n, T);
        nnet_submat dinp = NNET_BATCH(pctx->get_diff_input(), n);
        dinp = _fraction * dout;
      }
    }    
  }

  node_context_ptr nnet_node_dropout::create_context(int maxnbatch,
                                                     int maxbatchsize) const {
    return node_context_ptr(new dropout_context(maxnbatch, maxbatchsize,
                                                this->ndim()));
  }
  
  node_config_ptr nnet_node_dropout::create_config() const {
    std::hash<std::string> hashfunc;
    return node_config_ptr(new dropout_config(hashfunc(this->name())));
  }

  void nnet_node_dropout::read(const variant_t& src) {
    const variant_map& props = boost::get<variant_map>(src);
    nnet_simple_activation::read(src);
    _fraction = get_prop<double>(props, "fraction");
  }

  void nnet_node_dropout::write(variant_t* dest) const {
    nnet_simple_activation::write(dest);
    variant_map& props = boost::get<variant_map>(*dest);
    props["fraction"] = _fraction;
  }

  void
  nnet_node_dropout::initialize(const std::map<std::string, std::string>& ps) {
    nnet_simple_activation::initialize(ps);
    _fraction = boost::lexical_cast<float>(get(ps, "fraction"));
  }

}

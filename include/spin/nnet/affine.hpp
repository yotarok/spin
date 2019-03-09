#ifndef spin_nnet_affine_hpp_
#define spin_nnet_affine_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <viennacl/linalg/sum.hpp>
#include <viennacl/linalg/maxmin.hpp>
#include <gear/io/logging.hpp>

#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>
#include <spin/nnet/nnetmath.hpp>

namespace spin {
  class nnet_node_affine_transform;

  class affine_config : public node_config {
    float _bias_l2reg;
    float _maxnorm;
  public:
    affine_config();
    float bias_l2_regularizer() const { return _bias_l2reg; }
    bool has_bias_l2_regularizer() const;
    bool has_maxnorm() const;
    float maxnorm() const { return _maxnorm; }

    virtual bool set_update_parameter(const std::string& k,
                                      const std::string& v) {
      if (k == "maxnorm") {
        _maxnorm = boost::lexical_cast<float>(v);
      } else if (k == "bias_l2reg") {
        _bias_l2reg = boost::lexical_cast<float>(v);
      } else {
        return node_config::set_update_parameter(k, v);
      }
      return true;
    }
  };
  
  class affine_delta : public node_delta {
    size_t _noutput;
    size_t _ninput;
    nnet_matrix _dweight;
    nnet_matrix _dbias;
  public:
    affine_delta(size_t nout, size_t nin);
    virtual ~affine_delta() { }

    nnet_matrix& get_dweight() { return _dweight; }
    nnet_matrix& get_dbias() { return _dbias; }
    const nnet_matrix& get_dweight() const { return _dweight; }
    const nnet_matrix& get_dbias() const { return _dbias; }

    virtual void clear() {
      _dweight = viennacl::zero_matrix<float>(_noutput, _ninput);
      _dbias = viennacl::zero_matrix<float>(_noutput, 1);
    }
    
    virtual void scale(float r) {
      _dweight *= r;
      _dbias *= r;
    }

    virtual void add_regularizer(const_node_config_ptr conf,
                                 std::shared_ptr<const nnet_node> pnode);

  };
  
  class affine_context : public node_basic_context {
  public:
    affine_context(int maxnbatch, int maxbatchsize, int outdim, int indim)
      : node_basic_context(maxnbatch, maxbatchsize, outdim, indim) {
    }
    virtual ~affine_context() { }
    void accumulate_delta(node_delta_ptr pdelta);
  };
  
  class nnet_node_affine_transform : public nnet_node {
    nnet_matrix _weight;
    nnet_matrix _bias;

    viennacl::ocl::program& _prog;
    viennacl::ocl::kernel& _maxnorm_kernel;

  public:
    nnet_node_affine_transform(const std::string& n);
    
    const char* typetag() const { return "affine"; }

    node_context_ptr create_context(int maxnbatch, int maxbatchsize) const {
      return node_context_ptr(new affine_context(maxnbatch, maxbatchsize,
                                                 _weight.size1(), _weight.size2()));
    }
    node_delta_ptr create_delta() const {
      return node_delta_ptr(new affine_delta(_weight.size1(),
                                             _weight.size2()));
    }
    node_config_ptr create_config() const {
      return node_config_ptr(new affine_config);
    }

    const nnet_matrix& weight() const { return _weight; }
    const nnet_matrix& bias() const { return _bias; }

    nnet_matrix& weight() { return _weight; }
    nnet_matrix& bias() { return _bias; }


    void feed_forward(const_node_config_ptr pcfg,
                      node_context_ptr pctx) const;
    
    void back_propagate(const_node_config_ptr pcfg,
                        node_context_ptr pctx) const;

    virtual void update(const node_config& config,
                        const node_delta& delta);
    
    virtual void read(const variant_t& src) {
      const variant_map& props = boost::get<variant_map>(src);
      this->set_name(get_prop<std::string>(props, "nodename"));
      fmatrix wmat = get_prop<fmatrix>(props, "weight");
      viennacl::copy(wmat, _weight);
      fmatrix bmat = get_prop<fmatrix>(props, "bias");
      viennacl::copy(bmat, _bias);
    }

    virtual void write(variant_t* dest) const {
      *dest = variant_map();
      variant_map& props = boost::get<variant_map>(*dest);
      fmatrix wmat(_weight.size1(), _weight.size2());
      viennacl::copy(_weight, wmat);
      props["weight"] = wmat;
      fmatrix bmat(_bias.size1(), _bias.size2());
      viennacl::copy(_bias, bmat);
      props["bias"] = bmat;
      props["nodetype"] = std::string(this->typetag());
      props["nodename"] = this->name();
    }

    virtual void initialize(const std::map<std::string, std::string>& ps);
    virtual void dump_shape_info(std::ostream& os) {
      nnet_node::dump_shape_info(os);
      os << "   input : " << _weight.size2() << std::endl;
      os << "  output : " << _weight.size1() << std::endl;
    }

  };

}

#endif

#ifndef spin_nnet_node_hpp_
#define spin_nnet_node_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>

#include <boost/lexical_cast.hpp>
#include <spin/utils.hpp>
#include <gear/io/logging.hpp>
#include <spin/nnet/types.hpp>
#include <spin/nnet/signal.hpp>

namespace spin {
  
  class nnet_node;
  typedef std::shared_ptr<nnet_node> nnet_node_ptr;
  typedef std::shared_ptr<const nnet_node> nnet_node_cptr;

  /**
   * Class representing hyper parameters of nodes
   */
  class node_config {
    float _learn_rate;
    float _momentum;
    float _l2reg;
    
  public:
    node_config();
    
    virtual float learn_rate() const { return _learn_rate; }
    virtual float momentum() const { return _momentum; }
    virtual float l2_regularizer() const { return _l2reg; }

    virtual bool has_l2_regularizer() const;

    virtual bool set_update_parameter(const std::string& k, const std::string& v);
  };

  typedef std::shared_ptr<node_config> node_config_ptr;
  typedef std::shared_ptr<const node_config> const_node_config_ptr;

  
  class node_delta {
  public:
    node_delta() { }
   
    virtual ~node_delta() { }
    virtual void clear() = 0;
    virtual void scale(float r) = 0;
    virtual void add_regularizer(const_node_config_ptr conf,
                                 std::shared_ptr<const nnet_node> pnode) {
    }
    virtual void after_update(const_node_config_ptr conf) {
      this->scale(conf->momentum());
    }
  };
  // ^ Numbers that has same dimensionality with gradient vector
  //   i.e. objects used for represent gradients, updates, and so on
  
  typedef std::shared_ptr<node_delta> node_delta_ptr;
  typedef std::shared_ptr<const node_delta> node_delta_cptr;

  
  class node_delta_empty : public node_delta {
  public:
    virtual ~node_delta_empty() { }
    virtual void clear() { }
    virtual void scale(float) { }
  };

  
  class node_context {
  public:
    virtual ~node_context() { }
    virtual nnet_signal& get_input() = 0;
    virtual nnet_signal& get_output() = 0;
    virtual const nnet_signal& get_output() const = 0;
    
    virtual nnet_signal& get_diff_output() = 0;
    virtual nnet_signal& get_diff_input() = 0;

    virtual void clear() = 0;

    virtual void accumulate_delta(node_delta_ptr pdelta) = 0;
  };

  class node_basic_context : public node_context {
  protected:
    nnet_signal _input;
    nnet_signal _output;
    nnet_signal _dinput;
    nnet_signal _doutput;
  public:
    node_basic_context(int maxnbatch, int maxbatchsize, int outdim, int indim)
      : _input(maxnbatch, maxbatchsize, indim),
        _output(maxnbatch, maxbatchsize, outdim),
        _dinput(maxnbatch, maxbatchsize, indim),
        _doutput(maxnbatch, maxbatchsize, outdim) {
    }
    
    virtual ~node_basic_context() {
    }
    
    nnet_signal& get_input() {
      return _input;
    }

    nnet_signal& get_output() {
      return _output;
    }

    const nnet_signal& get_output() const {
      return _output;
    }

    nnet_signal& get_diff_output() {
      return _doutput;
    }

    nnet_signal& get_diff_input() {
      return _dinput;
    }

    void clear() {
      _input.clear();
      _output.clear();
      _dinput.clear();
      _doutput.clear();
    }
    
    virtual void accumulate_delta(node_delta_ptr pdelta) {
    }
  };
  
  typedef std::shared_ptr<node_context> node_context_ptr;
  typedef std::shared_ptr<const node_context> node_context_cptr;

  class nnet_node {
    std::string _name;

  protected:
    void set_name(const std::string& n) { _name = n; }
  public:
    nnet_node(const std::string& n) : _name(n) {
#ifdef ENABLE_NNET_PROFILE
      time_prepare_forward = 0;
      time_perform_forward = 0;
      time_prepare_backward = 0;
      time_perform_backward = 0;
      time_accumulate = 0;
      time_update = 0;
#endif
    }
    const std::string& name() const { return _name; }

    virtual void read(const variant_t& src) = 0;
    virtual void write(variant_t* dest) const = 0;
    
    virtual const char* typetag() const = 0;

    /// === Create companion objects ===
    
    virtual node_delta_ptr create_delta() const = 0;
    virtual node_context_ptr create_context(int maxnbatch, int maxbatchsize)
      const = 0;
    virtual node_config_ptr create_config() const = 0;
    
    virtual void feed_forward(const_node_config_ptr config,
                              node_context_ptr context) const = 0;
    virtual void back_propagate(const_node_config_ptr config,
                                node_context_ptr context) const = 0;

    virtual void initialize(const std::map<std::string, std::string>&) = 0;

    virtual void update(const node_config& config,
                        const node_delta& delta) {
    }

    virtual void dump_shape_info(std::ostream& os) {
      os << "    name : " << _name << std::endl;
      os << "    type : " << typetag() << std::endl;
    }

    static nnet_node_ptr create(const variant_t& src);
    static nnet_node_ptr create(const std::string& nodetype, const std::string& nodename);

#ifdef ENABLE_NNET_PROFILE
    double time_prepare_forward, time_perform_forward;
    double time_prepare_backward, time_perform_backward;
    double time_accumulate, time_update;
#endif
  };

  class nnet_simple_activation : public nnet_node {
    int _ndim;
  public:
    nnet_simple_activation(const std::string& n) : nnet_node(n), _ndim(-1) {
    }
    
    node_context_ptr create_context(int maxnbatch, int maxbatchsize) const {
      if (_ndim < 0) {
        ERROR("Set number of dimensions before creating context object");
        throw std::runtime_error("Set number of dimensions before creating context object");
      }
      return node_context_ptr(new node_basic_context(maxnbatch, maxbatchsize,
                                                     _ndim, _ndim));
    }
    node_delta_ptr create_delta() const {
      return node_delta_ptr(new node_delta_empty);
    }

    node_config_ptr create_config() const {
      return node_config_ptr(new node_config);
    }

    int ndim() const { return _ndim; }
    void set_ndim(int n) { _ndim = n; }
    
    virtual void read(const variant_t& src) {
      const variant_map& props = boost::get<variant_map>(src);
      this->set_name(get_prop<std::string>(props, "nodename"));
      _ndim = get_prop<int>(props, "ndim");
    }

    virtual void write(variant_t* dest) const {
      *dest = variant_map();
      variant_map& props = boost::get<variant_map>(*dest);
      props["ndim"] = _ndim;
      props["nodetype"] = std::string(this->typetag());
      props["nodename"] = this->name();
    }

    virtual void initialize(const std::map<std::string, std::string>& ps) {
      _ndim = boost::lexical_cast<int>(get(ps, "ndims"));
    }

    virtual void dump_shape_info(std::ostream& os) {
      nnet_node::dump_shape_info(os);
      os << "   # dim : " << _ndim << std::endl;
    }

  };

}


#endif

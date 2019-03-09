#ifndef spin_nnet_dropout_hpp_
#define spin_nnet_dropout_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/nnet/nnetmath.hpp>
#include <spin/nnet/random.hpp>

namespace spin {
  class dropout_config : public node_config {
    bool _training;
    uint32_t _seed;
    uint32_t _seed_offset;
  public:
    dropout_config(uint32_t seed_offset);
    bool is_training() const;
    uint32_t seed() const;
    virtual bool set_update_parameter(const std::string& k,
                                      const std::string& v);
  };
  
  class dropout_context : public node_basic_context {
    nnet_signal _mask;
    random_number_generator _rng;
    bool _rng_initialized;
  public:
    dropout_context(int maxnbatch, int maxbatchsize, int dim);
    virtual ~dropout_context() { }

    void reset_seed(uint32_t seed);
    
    nnet_signal& get_mask() { return _mask; }

    random_number_generator& get_rng() { return _rng; }
    bool is_rng_initialized() const { return _rng_initialized; }
    void initialize_rng(uint32_t seed) {
      _rng.reset_seed(seed);
      _rng_initialized = true;
    }
  };
  
  class nnet_node_dropout : public nnet_simple_activation {
    viennacl::ocl::program& _prog;
    viennacl::ocl::kernel& _binarize_kernel;
    float _fraction;
  public:
    nnet_node_dropout(const std::string& n);

    const char* typetag() const { return "dropout"; }
    
    void feed_forward(const_node_config_ptr pcfg,
                      node_context_ptr pctx) const;
    void back_propagate(const_node_config_ptr pcfg,
                        node_context_ptr pctx) const;

    virtual node_context_ptr create_context(int maxnbatch, int maxbatchsize)
      const;
    virtual node_config_ptr create_config() const;
    virtual void read(const variant_t& src);
    virtual void write(variant_t* dest) const;
    virtual void initialize(const std::map<std::string, std::string>& ps);
  };
}    

#endif

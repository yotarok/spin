#ifndef spin_nnet_relu_hpp_
#define spin_nnet_relu_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/nnet/nnetmath.hpp>

namespace spin {
  class nnet_node_relu : public nnet_simple_activation {
    viennacl::ocl::program& _prog;
    viennacl::ocl::kernel& _relu_kernel;
    viennacl::ocl::kernel& _drelu_kernel;
  public:
    nnet_node_relu(const std::string& n);

    const char* typetag() const { return "relu"; }
    
    void feed_forward(const_node_config_ptr pcfg,
                      node_context_ptr pctx) const;    
    void back_propagate(const_node_config_ptr pcfg,
                        node_context_ptr pctx) const;
  };
}    

#endif

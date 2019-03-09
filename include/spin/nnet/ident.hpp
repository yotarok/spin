#ifndef spin_nnet_ident_hpp_
#define spin_nnet_ident_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>

namespace spin {

  class nnet_node_ident : public nnet_simple_activation {
  public:
    nnet_node_ident(const std::string& n) : nnet_simple_activation(n) { }

    const char* typetag() const { return "ident"; }

    void feed_forward(const_node_config_ptr, node_context_ptr pctx) const {
      pctx->get_output().load(pctx->get_input());
    }
    
    void back_propagate(const_node_config_ptr, node_context_ptr pctx) const {
      pctx->get_diff_input().load(pctx->get_diff_output());
    }

  };

}    

#endif

#include <spin/nnet/node.hpp>

#include <spin/nnet/affine.hpp>
#include <spin/nnet/ident.hpp>
#include <spin/nnet/sigmoid.hpp>
#include <spin/nnet/relu.hpp>
#include <spin/nnet/dropout.hpp>

namespace spin {
  nnet_node_ptr nnet_node::create(const std::string& nodetype,
                                  const std::string& nodename) {
    nnet_node_ptr pnode;
    if (nodetype == "affine") {
      pnode.reset(new nnet_node_affine_transform(nodename));
    } else if (nodetype == "ident") {
      pnode.reset(new nnet_node_ident(nodename));
    } else if (nodetype == "sigmoid") {
      pnode.reset(new nnet_node_sigmoid(nodename));
    } else if (nodetype == "relu") {
      pnode.reset(new nnet_node_relu(nodename));
    } else if (nodetype == "dropout") {
      pnode.reset(new nnet_node_dropout(nodename));
    } else {
      throw std::runtime_error("Unknown node type");
    }
    return pnode;
  }

  nnet_node_ptr nnet_node::create(const variant_t& src) {
    const variant_map& props = boost::get<variant_map>(src);
    const std::string& nodetype = get_prop<std::string>(props, "nodetype");
    const std::string& nodename = get_prop<std::string>(props, "nodename");
    nnet_node_ptr pnode = nnet_node::create(nodetype, nodename);
    pnode->read(src);
    return pnode;
  }

  node_config::node_config() : _learn_rate(0), _momentum(0), _l2reg(0) {
  }
  
  bool node_config::set_update_parameter(const std::string& k,
                                         const std::string& v) {
    if (k == "learnrate") {
      _learn_rate = boost::lexical_cast<float>(v);
    } else if (k == "momentum") {
      _momentum = boost::lexical_cast<float>(v);
    } else if (k == "l2reg") {
      _l2reg = boost::lexical_cast<float>(v);
    } else {
      return false;
    }
    return true;
  }

  bool node_config::has_l2_regularizer() const {
    return _l2reg > 0;
  }


}

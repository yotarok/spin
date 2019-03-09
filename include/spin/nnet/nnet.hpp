#ifndef spin_nnet_nnet_hpp_
#define spin_nnet_nnet_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/nnet/node.hpp>
#include <gear/io/logging.hpp>
#include <spin/nnet/stream.hpp>

namespace spin { 
  class nnet_context;
  class nnet_delta;
  class nnet_config;
  
  class optimization_diverged : public std::runtime_error {
  public:
    optimization_diverged() : optimization_diverged("optimization diverged") {
    }
    optimization_diverged(const std::string& what) : std::runtime_error(what) {
    }
  };
  
  class nnet {
    std::vector<nnet_node_ptr> _nodes; // must be topologically sorted
    std::map<std::string, int> _name_to_loc;
    std::vector<std::vector<int> > _forward_links;
    std::vector<std::vector<int> > _reverse_links;

    void topological_sort_impl(std::vector<int>* reorder,
                               std::set<int>* visit,
                               int cur);
    void topological_sort();

  public:
    typedef std::vector<nnet_node_ptr>::const_iterator const_iterator;
    // Construct empty nnet
    nnet();

    // Construct by reading from variant
    nnet(const variant_t& src);

    void read(const variant_t& src);
    void write(variant_t* dest);

    int get_node_location(const std::string& name) const {
      auto it = _name_to_loc.find(name);
      if (it == _name_to_loc.end()) {
        ERROR("node %s not found", name.c_str());
      }
      return it->second;
    }

    void add_node(nnet_node_ptr p, const std::vector<std::string>& prevs);
    void remove_node(int loc);
    void remove_node(const std::string& name);

    const_iterator cbegin() const { return _nodes.cbegin(); }
    const_iterator cend() const { return _nodes.cend(); }

    size_t nnodes() const { return _nodes.size(); }

    std::shared_ptr<const nnet_node> node(int loc) const { return _nodes[loc]; }
    std::shared_ptr<const nnet_node> node(const std::string& k) const {
      auto it = _name_to_loc.find(k);
      if (it == _name_to_loc.end()) {
        throw std::runtime_error("key " + k + " is not found");
      }
      return _nodes[it->second];
    }
    std::shared_ptr<nnet_node> node(int loc) { return _nodes[loc]; }
    std::shared_ptr<nnet_node> node(const std::string& k) {
      auto it = _name_to_loc.find(k);
      if (it == _name_to_loc.end()) {
        throw std::runtime_error("key " + k + " is not found");
      }
      return _nodes[it->second];
    }

    void feed_forward(const nnet_config& config, nnet_context* context);
    void back_propagate(const nnet_config& config, nnet_context* context);

    void update(const nnet_config& config, const nnet_delta& delta);

    void dump_shape_info(std::ostream& os);
  };
  
  /**
   * Class representing context of neural network forward and backward pass
   */
  class nnet_context {
    const nnet& _parameter;
    std::vector<node_context_ptr> _node_contexts;

    std::vector<bool> _has_forward_stream;
    std::vector<bool> _has_backward_stream;
  public:
    nnet_context(const nnet& nnet, int maxnbatch, int maxbatchsize);

    node_context_ptr get(int loc) { return _node_contexts[loc]; }
    node_context_cptr get(int loc) const { return _node_contexts[loc]; }
    node_context_ptr get(const std::string& k) {
      return get(_parameter.get_node_location(k));
    }
    node_context_cptr get(const std::string& k) const {
      return get(_parameter.get_node_location(k));
    }

    void clear();

    void before_forward(const std::vector<stream_ptr>& streams,
                        const std::vector<corpus_entry>& batch);
    void before_backward(const std::vector<stream_ptr>& streams,
                         const std::vector<corpus_entry>& batch);
    
    bool has_forward_stream(int loc) const {
      return _has_forward_stream[loc];
    }
    bool has_backward_stream(int loc) const {
      return _has_backward_stream[loc];
    }
    void set_forward_stream_flag(int loc, bool b) {
      // TO DO: Bit ugly...
      //    This is prepared for supportdirect data loading in nnet_scorer
      _has_forward_stream[loc] = b;
    }

    node_context_ptr node(int loc) { return _node_contexts[loc]; }

    void accumulate_delta(const nnet_config& config, nnet_delta* pdelta);
  };

  class nnet_config {
    std::vector<std::string> _node_names;
    std::vector<node_config_ptr> _node_confs;
  public:
    nnet_config(const nnet& nnet);
    void load_option(const std::string& s);

    const_node_config_ptr get(int loc) const {
      return _node_confs[loc];
    }
  };
 
  class nnet_delta {
    nnet& _parameter;
    std::vector<node_delta_ptr> _node_deltas;
  public:
    nnet_delta(nnet& nnet);

    node_delta_ptr get(int loc) { return _node_deltas[loc]; }
    node_delta_cptr get(int loc) const { return _node_deltas[loc]; }
    node_delta_ptr get(const std::string& k) {
      return get(_parameter.get_node_location(k));
    }
    node_delta_cptr get(const std::string& k) const {
      return get(_parameter.get_node_location(k));
    }


    void add_regularizer(const nnet_config& config, const nnet& net) {
      for (int loc = 0; loc < _node_deltas.size(); ++ loc)
        _node_deltas[loc]->add_regularizer(config.get(loc), net.node(loc));
    }

    // Do momentum
    void after_update(const nnet_config& config) {
      for (int loc = 0; loc < _node_deltas.size(); ++ loc) {
        _node_deltas[loc]->after_update(config.get(loc));
      }
    }

    void clear() {
      for (auto it = _node_deltas.begin(), last = _node_deltas.end();
           it != last; ++ it) (*it)->clear();
    }

    void scale(float r) {
      for (auto it = _node_deltas.begin(), last = _node_deltas.end();
           it != last; ++ it) (*it)->scale(r);
    }
  };
}


#endif

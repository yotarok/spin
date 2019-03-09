#include <spin/nnet/nnet.hpp>
#include <spin/variant.hpp>

#include <gear/io/logging.hpp>
#include <boost/xpressive/xpressive.hpp>

namespace spin {
  nnet::nnet() {
  }

  nnet::nnet(const variant_t& src) {
    read(src);
  }

  void nnet::topological_sort_impl(std::vector<int>* reorder,
                                   std::set<int>* visit,
                                   int cur) {
    visit->insert(cur);
    for (int i = 0; i < _reverse_links[cur].size(); ++ i) {
      topological_sort_impl(reorder, visit, _reverse_links[cur][i]);
    }
    reorder->push_back(cur);
  }
  
  void nnet::topological_sort() {
    std::set<int> outputs;
    for (int i = 0; i < _forward_links.size(); ++ i) {
      if (_forward_links[i].size() == 0)
        outputs.insert(i);
    }

    std::set<int> visit;
    std::vector<int> reorder;
    for (auto it = outputs.cbegin(), last = outputs.cend(); it != last; ++ it) {
      topological_sort_impl(& reorder, & visit, *it);
    }
    std::vector<int> oid2nid(reorder.size(), -1);
    for (int j = 0; j < reorder.size(); ++ j) {
      oid2nid[reorder[j]] = j;
    }

    std::vector<nnet_node_ptr> new_nodes;
    std::vector<std::vector<int> > new_fwd, new_bwd;
    _name_to_loc.clear();
    // apply reorder
    for (int nid = 0; nid < reorder.size(); ++ nid) {
      int oid = reorder[nid];
      new_nodes.push_back(_nodes[oid]);
      _name_to_loc[_nodes[oid]->name()] = nid;
      
      new_fwd.push_back(std::vector<int>());
      new_bwd.push_back(std::vector<int>());

      for (int j = 0; j < _forward_links[oid].size(); ++ j)
        new_fwd[nid].push_back(oid2nid[_forward_links[oid][j]]);

      for (int j = 0; j < _reverse_links[oid].size(); ++ j)
        new_bwd[nid].push_back(oid2nid[_reverse_links[oid][j]]);
    }

    _nodes = new_nodes;
    _forward_links = new_fwd;
    _reverse_links = new_bwd;
  }
  
  void nnet::read(const variant_t& src) {
    variant_vector nodesrc; 
    try {
      nodesrc = get_prop<variant_vector>(src, "nodes");
    } catch(std::exception& e) {
      //ERROR("Ignore error: %s", e.what());
    }
    // const variant_vector& nodesrc = get_prop<variant_vector>(src, "nodes");
    // ^ TO DO: Not an ideal solution, this is for overcoming the fact that 
    // LibYAML emits "~" for empty array
    
    int loc = 0;
    for(auto it = nodesrc.cbegin(), last = nodesrc.cend();
        it != last; ++ it, ++ loc) {
      nnet_node_ptr pnode = nnet_node::create(*it);
      _nodes.push_back(pnode);
      _name_to_loc.insert(std::make_pair(pnode->name(), loc));
    }

    bool sorted = true;
    _forward_links.assign(_nodes.size(), std::vector<int>());
    _reverse_links.assign(_nodes.size(), std::vector<int>());

    //const variant_vector& linksrc = get_prop<variant_vector>(src, "links");
    variant_vector linksrc;
    try {
      linksrc = get_prop<variant_vector>(src, "links");
    } catch(...) {
    }
    // ^ TO DO: Not an ideal solution, this is for overcoming the fact that 
    // LibYAML emits "~" for empty array
    
    for (auto it = linksrc.cbegin(), last = linksrc.cend();
         it != last; ++ it) {
      int from = _name_to_loc[get_prop<std::string>(*it, "from")];
      int to = _name_to_loc[get_prop<std::string>(*it, "to")];

      _forward_links[from].push_back(to);
      _reverse_links[to].push_back(from);
      if (from > to) {
        sorted = false;
      } else if (from == to) {
        throw std::runtime_error("Self-loop is not supported");
      }
    }

    if (! sorted) {
      INFO("NNet is not topologically sorted, sort.");
      topological_sort();
    }
  }

  void nnet::write(variant_t* dest) {
    *dest = variant_map();
    variant_map& destmap = boost::get<variant_map>(*dest);

    destmap["nodes"] = variant_vector();
    variant_vector& nodes =
      boost::get<variant_vector>(destmap["nodes"]);

    for (auto it = _nodes.cbegin(), last = _nodes.cend();
         it != last; ++ it) {
      if ((*it)) { // after node removal this might be null
        nodes.push_back(variant_t());
        (*it)->write(&(nodes[nodes.size() - 1]));
      }
    }

    destmap["links"] = variant_vector();
    variant_vector& links =
      boost::get<variant_vector>(destmap["links"]);

    for (int n = 0; n < _nodes.size(); ++ n) {
      for (auto it = _forward_links[n].cbegin(), last = _forward_links[n].cend();
           it != last; ++ it) {
        links.push_back(variant_map());
        variant_map& added =
          boost::get<variant_map>(links[links.size() - 1]);

        added["from"] = _nodes[n]->name();
        added["to"] = _nodes[*it]->name();
      }
    }
  }
  
  void nnet::add_node(nnet_node_ptr p, const std::vector<std::string>& prevs) {
    int loc = _nodes.size();
    _nodes.push_back(p);
    _name_to_loc[p->name()] = loc;
    _forward_links.push_back(std::vector<int>());
    _reverse_links.push_back(std::vector<int>());
    for (auto it = prevs.cbegin(), last = prevs.cend(); it != last; ++ it) {
      auto pit = _name_to_loc.find(*it);
      if (pit == _name_to_loc.end()) {
        throw std::runtime_error("Previous node " + *it + " not found");
      }
      int ploc = pit->second;
      _forward_links[ploc].push_back(loc);
      _reverse_links[loc].push_back(ploc);
    }
  }

  void nnet::remove_node(int loc) {
    _nodes[loc] = nnet_node_ptr();
    _forward_links[loc].clear();
    _reverse_links[loc].clear();

    for (int i = 0; i < _nodes.size(); ++ i) {
      std::vector<int> nlink;
      for (auto it = _forward_links[i].cbegin(),
             last = _forward_links[i].cend(); it != last; ++ it) {
        if (*it != loc) nlink.push_back(*it);
      }
      _forward_links[i] = nlink;
    }
    
    for (int i = 0; i < _nodes.size(); ++ i) {
      std::vector<int> nlink;
      for (auto it = _reverse_links[i].cbegin(),
             last = _reverse_links[i].cend(); it != last; ++ it) {
        if (*it != loc) nlink.push_back(*it);
      }
      _reverse_links[i] = nlink;
    }

  }
  
  void nnet::remove_node(const std::string& name) {
    auto it = _name_to_loc.find(name);
    if (it == _name_to_loc.end()) {
      throw std::runtime_error("node " + name + " not found");
    } else {
      remove_node(it->second);
    }
  }

  void nnet::feed_forward(const nnet_config& config,
                          nnet_context* context) {
    
    for (int loc = 0; loc < _nodes.size(); ++ loc) {
      if (context->has_forward_stream(loc)) {
      } else {
        node_context_ptr pctx = context->node(loc);
        int offset = 0;
#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        double timer_start = get_wall_time();
#endif
        for (int n = 0; n < pctx->get_input().nbatch(); ++ n) {
          for (auto it = _reverse_links[loc].cbegin(),
                 last = _reverse_links[loc].cend(); it != last; ++ it) {
            nnet_submat prevout =
              NNET_BATCH(context->node(*it)->get_output(), n);
            int D = prevout.size1(), T = prevout.size2();
            pctx->get_input().set_size(n, T);
            viennacl::project(NNET_BATCH(pctx->get_input(), n),
                              viennacl::range(offset, offset + D),
                              viennacl::range(0, T)) = prevout;
            offset += D;
          }
        }

#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        double timer_mid = get_wall_time();
#endif
        _nodes[loc]->feed_forward(config.get(loc), pctx);
#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        _nodes[loc]->time_prepare_forward += timer_mid - timer_start;
        _nodes[loc]->time_perform_forward += get_wall_time() - timer_mid;
#endif
      }
    }
  }
  
  void nnet::back_propagate(const nnet_config& config,
                            nnet_context* context) {
    for (int loc = _nodes.size() - 1; loc >= 0; -- loc) {
      if (context->has_backward_stream(loc)) {
      } else {
#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        double timer_start = get_wall_time();
#endif
        node_context_ptr pctx = context->node(loc);
        int offset = 0;
        for (int n = 0; n < pctx->get_diff_output().nbatch(); ++ n) {
          for (auto it = _forward_links[loc].cbegin(),
                 last = _forward_links[loc].cend(); it != last; ++ it) {
            nnet_submat prevdin = 
              NNET_BATCH(context->node(*it)->get_diff_input(), n);
            int D = prevdin.size1(), T = prevdin.size2();
            pctx->get_diff_output().set_size(n, T);
            viennacl::project(NNET_BATCH(pctx->get_diff_output(), n),
                              viennacl::range(offset, offset + D),
                              viennacl::range(0, T)) = prevdin;
            offset += D;
          }
        }

#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        double timer_mid = get_wall_time();
#endif
        _nodes[loc]->back_propagate(config.get(loc), pctx);

#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        _nodes[loc]->time_prepare_backward += timer_mid - timer_start;
        _nodes[loc]->time_perform_backward += get_wall_time() - timer_mid;
#endif
      }
    }
  }

  void nnet::update(const nnet_config& config,
                    const nnet_delta& delta) {
    for (int loc = 0; loc < _nodes.size(); ++ loc) {
#ifdef ENABLE_NNET_PROFILE
      viennacl::backend::finish();
      double timer_start = get_wall_time();
#endif
      _nodes[loc]->update(*config.get(loc), *delta.get(loc));

#ifdef ENABLE_NNET_PROFILE
      viennacl::backend::finish();
      _nodes[loc]->time_update += get_wall_time() - timer_start;
#endif
    }
  }

  void nnet::dump_shape_info(std::ostream& os) {
    os << " NNET SHAPE INFO " << std::endl
       << " # nodes: " << _nodes.size() << std::endl;
    for (int loc = 0; loc < _nodes.size(); ++ loc) {
      os << "Location " << loc << std::endl;
      _nodes[loc]->dump_shape_info(os);
      os << "Connecting to [";
      for (auto cit = _forward_links[loc].cbegin(),
             clast = _forward_links[loc].cend(); cit != clast; ++ cit) {
        os << *cit << ", ";
      }
      os << "] and is connected from [";
      for (auto cit = _reverse_links[loc].cbegin(),
             clast = _reverse_links[loc].cend(); cit != clast; ++ cit) {
        os << *cit << ", ";
      }
      os << "]" << std::endl;
    }
  }

  nnet_context::nnet_context(const nnet& nnet, int maxnbatch, int maxbatchsize)
    : _parameter(nnet),
      _has_forward_stream(nnet.nnodes(), false),
      _has_backward_stream(nnet.nnodes(), false) {
    for (auto it = nnet.cbegin(), last = nnet.cend(); it != last; ++ it) {
      _node_contexts.push_back((*it)->create_context(maxnbatch, maxbatchsize));
    }
  }

  void nnet_context::clear() {
    _has_forward_stream.assign(_parameter.nnodes(), false);
    _has_backward_stream.assign(_parameter.nnodes(), false);
    for (auto it = _node_contexts.begin(), last = _node_contexts.end();
         it != last; ++ it) {
      (*it)->clear();
    }
  }

  void nnet_context::before_forward(const std::vector<stream_ptr>& streams,
                                    const std::vector<corpus_entry>& batch) {
    
    for (auto it = streams.cbegin(), last = streams.cend(); it != last; ++ it) {
      int loc = _parameter.get_node_location((*it)->target_component());
      
      if ((*it)->before_forward(_node_contexts[loc], batch)) {
        _has_forward_stream[loc] = true;
      }
    }
  }

  void nnet_context::before_backward(const std::vector<stream_ptr>& streams,
                                     const std::vector<corpus_entry>& batch) {
    
    for (auto it = streams.cbegin(), last = streams.cend(); it != last; ++ it) {
      int loc = _parameter.get_node_location((*it)->target_component());
      
      if ((*it)->before_backward(_node_contexts[loc], batch)) {
        _has_backward_stream[loc] = true;
      }
    }
  }

  void nnet_context::accumulate_delta(const nnet_config& config,
                                      nnet_delta* pdelta) {
    for (int loc = 0; loc < _node_contexts.size(); ++ loc) {
#ifdef ENABLE_NNET_PROFILE
        viennacl::backend::finish();
        double timer_start = get_wall_time();
#endif
      _node_contexts[loc]->accumulate_delta(pdelta->get(loc));
#ifdef ENABLE_NNET_PROFILE
      viennacl::backend::finish();
      // TO DO: Avoid using const_cast!!
      const_cast<nnet_node*>(_parameter.node(loc).get())->time_accumulate += get_wall_time() - timer_start;
#endif
    }
  }

  nnet_config::nnet_config(const nnet& nnet) {
    for (int loc = 0; loc < nnet.nnodes(); ++ loc) {
      _node_names.push_back(nnet.node(loc)->name());
      _node_confs.push_back(nnet.node(loc)->create_config());
    }
  }

  void nnet_config::load_option(const std::string& s) {
    size_t eq = s.find('=');
    if (eq == std::string::npos) {
      throw std::runtime_error("Equal not found:"
                               "Update option must be NAME@TARGET=VALUE");
    }
    std::string k = s.substr(0, eq), v = s.substr(eq + 1);

    size_t atmark = k.find('@');
    std::string prop, target;
    if (atmark == std::string::npos) {
      //throw std::runtime_error("Atmark not found:"
      //"Update option must be NAME@TARGET=VALUE");
      prop = k;
      target = ".*";
    } else {
      prop = k.substr(0, atmark);
      target = k.substr(atmark + 1);
    }
    
    if (target == "_") target = ".*";
    
    boost::xpressive::sregex target_rex =
      boost::xpressive::sregex::compile(target.c_str());
    boost::xpressive::smatch what;
    for (int loc = 0; loc < _node_names.size(); ++ loc) {
      bool match =
        boost::xpressive::regex_match(_node_names[loc],
                                      what, target_rex);
      if (match) {
        bool success = _node_confs[loc]->set_update_parameter(prop, v);
        if (! success) {
          INFO("Config %s is not supported in %s", prop.c_str(),
               _node_names[loc].c_str());
        }
      }
    }
  }

  nnet_delta::nnet_delta(nnet& nnet) : _parameter(nnet) {
    for (auto it = nnet.cbegin(), last = nnet.cend(); it != last; ++ it) {
      _node_deltas.push_back((*it)->create_delta());
    }
  }


}

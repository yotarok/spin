#include <spin/types.hpp>
#include <spin/hmm/treestat.hpp>
#include <spin/io/yaml.hpp>
#include <boost/algorithm/string/split.hpp>
#include <gear/io/logging.hpp>
#include <spin/fst/linear.hpp>


namespace spin {

  void tree_statistics::accumulate(const HMM_context& ctx, const fmatrix& features) {
    auto it = _data.find(ctx);
    if (it == _data.end()) {
      it = _data.insert(std::make_pair(ctx, tree_Gaussian_statistics())).first;
      it->second.zero = 0.0;
      it->second.first = dvector::Zero(features.rows());
      it->second.second = dvector::Zero(features.rows());
    }

    it->second.zero += features.cols();
    it->second.first += features.cast<double>().rowwise().sum();
    it->second.second += features.cast<double>().array().square().matrix().rowwise().sum();
  }

  void tree_statistics::accumulate(const tree_statistics& other) {
    if (other._left_context != _left_context ||
        other._right_context != _right_context) {
      ERROR("Context size mismatch: Left: (%d vs %d), Right: (%d vs %d)",
            other._left_context, _left_context,
            other._right_context, _right_context);
      throw std::runtime_error("Context size mismatch");
    }

    for (auto otit = other._data.cbegin(), otlast = other._data.cend();
         otit != otlast; ++ otit) {
      auto thisit = _data.find(otit->first);
      if (thisit == _data.end()) {
        _data.insert(*otit); // just copy
      } else {
        thisit->second.zero += otit->second.zero;
        thisit->second.first += otit->second.first;
        thisit->second.second += otit->second.second;
      }
    }
  }

  void tree_statistics::read(const variant_t& src) {
    const variant_map& props = boost::get<variant_map>(src);

    const variant_vector& lens = get_prop<variant_vector>(props, "contextLengths");
    _left_context = boost::get<int>(lens[0]);
    _right_context = boost::get<int>(lens[1]);

    const variant_map& stats = get_prop<variant_map>(props, "stats");
    for (auto it = stats.cbegin(), last = stats.cend(); it != last; ++ it) {
      const variant_map& stat = boost::get<variant_map>(it->second);
      tree_Gaussian_statistics d;
      d.zero = get_prop<double>(stat, "zero");
      d.first = get_prop<dmatrix>(stat, "first");
      d.second = get_prop<dmatrix>(stat, "second");
      HMM_context ctx;
      ctx.parse_string(it->first);
      _data.insert(std::make_pair(ctx, d));
    }
  }
  
  void tree_statistics::write(variant_t* dest) {
    *dest = variant_map();
    variant_map& props = boost::get<variant_map>(*dest);

    props["contextLengths"] = variant_vector();
    boost::get<variant_vector>(props["contextLengths"]).push_back(_left_context);
    boost::get<variant_vector>(props["contextLengths"]).push_back(_right_context);
    
    props["stats"] = variant_map();
    variant_map& stats = boost::get<variant_map>(props["stats"]);
    for (auto it = _data.cbegin(), last = _data.cend(); it != last; ++ it) {
      variant_map stat;
      stat["zero"] = it->second.zero;
      stat["first"] = dmatrix(it->second.first);
      stat["second"] = dmatrix(it->second.second);
      
      stats.insert(std::make_pair(it->first.to_string(), stat));
    }
    props["stats"] = stats;
    *dest = props;
  }

  void tree_statistics::compute_membership(const context_decision_tree& tree,
                                           tree_membership* pmem) const {
    for (auto it = data().cbegin(), last = data().cend();
         it != last; ++ it) {
      const context_decision_tree_node* cur = tree.root();

      while (! cur->is_leaf) {
        bool f = cur->question->question(it->first);
        if (f) {
          cur = cur->is_true;
        }
        else {
          cur = cur->is_false;
        }
        if (! cur) {
          throw std::runtime_error("Fallen into undefined state");
        }
      }
      tree_context_stat sp(it->first, &(it->second));
      //pmem->insert(std::make_pair(cur, sp));
      (*pmem)[cur].push_back(sp);
    }
  }


 
  void get_alignment_context(const vector_fst& align, int leftctx, int rightctx,
                             std::vector<HMM_context>* pctxlist) {
    // Assume state and phone are synchronized, linearity, and topological sorted
    

    auto palign = align.GetFst<LatticeArc>();
    int nstate = 0;

    std::vector<int> ilabs;
    ExtractString(align, true, &ilabs);

    std::vector<std::string> phones;
    std::vector<int> phoneidx;
    std::vector<int> phoneloc;
    int pi = 0;
    for (int i = 0; i < ilabs.size(); ++ i) {
      std::string isym = align.InputSymbols()->Find(ilabs[i]);
      if (ilabs[i] == 0) {
        phoneidx.push_back(-1);
        phoneloc.push_back(-1);
      } else if (isym.substr(0, 2) == "#]") {
        phones.push_back(isym.substr(2));
        pi += 1;
        phoneidx.push_back(-1);
        phoneloc.push_back(-1);
      } else if (isym[0] == '#') {
        phoneidx.push_back(-1);
        phoneloc.push_back(-1);
      } else {
        std::vector<std::string> vals;
        boost::split(vals, isym, boost::is_any_of(";"));

        if (vals.size() != 3) {
          ERROR("Input symbol doesn't contain location information: %s", isym.c_str());
          throw std::runtime_error("Input symbol doesn't contain location information");
        }

        int loc = -1;
        try {
          loc = boost::lexical_cast<int>(vals[2]);
        } catch (boost::bad_lexical_cast) {
          ERROR("Input symbol parse error: %s", isym.c_str());
          throw std::runtime_error("Input symbol parse error");
        }
        
        phoneidx.push_back(pi);
        phoneloc.push_back(loc);
      }
    }

    for (int i = 0; i < ilabs.size(); ++ i) {
      int pcur = phoneidx[i];

      HMM_context ctx;
      for (int tau = -leftctx; tau <= rightctx; ++ tau) {
        std::string p = (pcur + tau < 0 || pcur + tau >= phones.size())
          ? "" : phones[pcur + tau];
        ctx.label.push_back(p);
      }
      ctx.loc = phoneloc[i];
      pctxlist->push_back(ctx);
    }   
    
  }



}

#ifndef spin_hmm_treestat_hpp_
#define spin_hmm_treestat_hpp_

#include <spin/types.hpp>
#include <boost/utility.hpp>
#include <spin/io/yaml.hpp>
#include <spin/fstext/latticearc.hpp>
#include <spin/utils.hpp>
#include <spin/hmm/tree.hpp>

#include <boost/tuple/tuple.hpp>

// routines for getting tree statistics
namespace spin {
  struct tree_Gaussian_statistics {
    double zero;
    dvector first;
    dvector second;

    tree_Gaussian_statistics() : zero(0), first(), second() {
    }

    void accumulate(const tree_Gaussian_statistics& oth) {
      zero += oth.zero;
      if (first.rows() == 0) first = dvector::Zero(oth.first.rows());
      first += oth.first;
      if (second.rows() == 0) second = dvector::Zero(oth.second.rows());
      second += oth.second;
    }

    double data_loglikelihood() {
      // TO DO: naive computation...
      double D = static_cast<double>(first.rows());
      dvector mean = first / zero;
      dvector var = second / zero - mean.array().square().matrix();
      return - zero / 2 * (D * (M_LOG2PI - 1) + var.array().log().sum());
    }
  };

  typedef
  boost::tuple<HMM_context, const tree_Gaussian_statistics*> tree_context_stat;

  typedef
  std::vector<tree_context_stat> tree_context_stat_list;
  
  typedef
  std::map<const context_decision_tree_node*, tree_context_stat_list> tree_membership;

  class tree_statistics {
    std::map<HMM_context, tree_Gaussian_statistics> _data;
    int _left_context, _right_context;
  public:
    tree_statistics() {}
    tree_statistics(const variant_t& src) { read(src); }
    void accumulate(const HMM_context& ctx, const fmatrix& features);
    void accumulate(const tree_statistics& other);
    void read(const variant_t& src);
    void write(variant_t* dest);

    int left_context_length() const { return _left_context; }
    int right_context_length() const { return _right_context; }
    void set_left_context_length(int n) { _left_context = n; }
    void set_right_context_length(int n) { _right_context = n; }

    const std::map<HMM_context, tree_Gaussian_statistics>& data() const {
      return _data;
    }

    void compute_membership(const context_decision_tree& tree,
                            tree_membership* pmem) const;

  };
  
  void get_alignment_context(const vector_fst& align, int leftctx, int rightctx,
                             std::vector<HMM_context>* pctxlist);



}

#endif

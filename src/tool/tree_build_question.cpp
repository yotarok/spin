#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/corpus/corpus.hpp>
#include <spin/io/variant.hpp>
#include <spin/fst/linear.hpp>
#include <spin/hmm/tree.hpp>
#include <spin/hmm/treestat.hpp>
#include <fst/script/topsort.h>
#include <queue>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, stats,
                   ("s", "stats", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  struct node {
    node() : labels(), stats(), left(0), right(0) { }
    std::vector<std::string> labels;
    tree_Gaussian_statistics stats;
    node* left;
    node* right;

    std::string label_str() const {
      std::string ret;
      for (auto it = labels.cbegin(), last = labels.cend(); it != last; ++ it) {
        ret += *it;
        if (boost::next(it) != last) ret += ",";
      }
      return ret;
    }
  };

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t tree_src;
    load_variant(&tree_src, arg.input.getValue());
    context_decision_tree tree(tree_src);

    variant_t stats_src;
    load_variant(&stats_src, arg.stats.getValue());
    tree_statistics treestat(stats_src);

    size_t dim = 0;
    std::map<std::string, node*> leaves;
    for (auto it = treestat.data().cbegin(), last = treestat.data().cend();
         it != last; ++ it) {
      std::string p = it->first.label[treestat.left_context_length()];
      auto lit = leaves.find(p);
      if (dim == 0) {
        dim = it->second.first.rows();
        INFO("Feature dimension = %d", dim);
      }

      if (lit == leaves.end()) {
        node* nnode = new node;
        nnode->labels.push_back(p);
        lit = leaves.insert(std::make_pair(p, nnode)).first;
      }
      lit->second->stats.accumulate(it->second);
    }

    std::set<node*> roots;
    for (auto it = leaves.cbegin(), last = leaves.cend(); it != last; ++ it) {
      roots.insert(it->second);
    }

    while (roots.size() > 1) {
      std::set<node*>::const_iterator minlit, minrit;
      tree_Gaussian_statistics minstat;
      double minloss = HUGE_VAL;
      for (auto lit = roots.cbegin(), last = roots.cend(); lit != last; ++ lit) {
        for (auto rit = boost::next(lit); rit != last; ++ rit) {
          double ll_split = (*lit)->stats.data_loglikelihood()
            + (*rit)->stats.data_loglikelihood();
          tree_Gaussian_statistics merge = (*lit)->stats;
          merge.accumulate((*rit)->stats);
          double ll_merge = merge.data_loglikelihood();
          double loss = ll_split - ll_merge;
          if (loss < minloss) {
            minloss = loss;
            minstat = merge;
            minlit = lit;
            minrit = rit;
          }
        }
      }

      INFO("[%s] and [%s] merged, ll = (%f, %f) merged_ll = %f",
           (*minlit)->label_str().c_str(),
           (*minrit)->label_str().c_str(),
           (*minlit)->stats.data_loglikelihood(),
           (*minrit)->stats.data_loglikelihood(),
           minstat.data_loglikelihood());

      node* nnode = new node;
      nnode->labels = (*minlit)->labels;
      nnode->labels.insert(nnode->labels.end(),
                           (*minrit)->labels.begin(), (*minrit)->labels.end());
      nnode->stats = minstat;
      nnode->left = *minlit;
      nnode->right = *minrit;

      roots.erase(minlit);
      roots.erase(minrit);
      roots.insert(nnode);
    }

    std::set<std::set<std::string> > predefined_categories;
    for (auto it = tree.categories().cbegin(), last = tree.categories().cend();
         it != last; ++ it) {
      predefined_categories.insert(it->second);
    }
    std::queue<node*> que;
    int qid = 0;
    // skip root node that includes all phones
    que.push((*roots.begin())->left);
    que.push((*roots.begin())->right);
    while (que.size() > 0) {
      node* n = que.front();
      que.pop();

      std::set<std::string> cat(n->labels.begin(), n->labels.end());
      if (predefined_categories.find(cat) != predefined_categories.end()) {
        continue; // avoid duplicate categories
      }
      std::string qidstr = (boost::format("GenQ[%d]") % qid).str();
      ++ qid;
      tree.categories().insert(std::make_pair(qidstr, cat));
      if (n->left) que.push(n->left);
      if (n->right) que.push(n->right);
    }
    
    // memory release intentionally omitted
    variant_t output_src;
    tree.write(&output_src);
    INFO("Emitting...");
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacPCDT");
    
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Tree question builder",
                          argc, argv, spin::tool_main);
}

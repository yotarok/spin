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
#include <spin/hmm/tree.hpp>
#include <spin/hmm/treestat.hpp>
#include <queue>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, stats,
                   ("s", "stats", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<int>, maxstate,
                   ("", "maxstate", "", false, 5000, "N")),
                  (TCLAP::ValueArg<int>, minocc,
                   ("", "minocc", "", false, 50, "N")),
                  (TCLAP::ValueArg<float>, delta,
                   ("", "delta", "", false, 1.0, "delta")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  
  typedef
  std::priority_queue<boost::tuple<double, const context_decision_tree_node*,
                                   tree_question_ptr> > tree_split_queue;


  double examine_question(tree_question_ptr pq,
                          const tree_context_stat_list& statlist, int minocc) {
    tree_Gaussian_statistics all, is_true, is_false;
    //int tc = 0, fc = 0;
    for (auto it = statlist.cbegin(), last = statlist.cend();
         it != last; ++ it) {
      all.accumulate(*(it->get<1>()));
      if (pq->question(it->get<0>())) {
        is_true.accumulate(*(it->get<1>()));
      } else {
        is_false.accumulate(*(it->get<1>()));
      }
    }
    if (is_true.zero < minocc || is_false.zero < minocc)
      return 0;
    double gain = is_true.data_loglikelihood() + is_false.data_loglikelihood()
      - all.data_loglikelihood();
    
    //std::cout << is_true.data_loglikelihood() << " + " << is_false.data_loglikelihood() << " - " << all.data_loglikelihood() << " = " << gain << std::endl;
    //std::cout << tc << " " << is_true.zero << " : " << fc << std::endl;
    
    return gain;
  }
  
  
  tree_question_ptr
  find_best_question(const context_decision_tree_node* pnode,
                     const tree_context_stat_list& statlist,
                     int minocc,
                     double* pgain) {
    double maxgain = 0.0;
    tree_question_ptr maxq;
    
    const context_decision_tree& tree = pnode->tree;

    // context_categorical_question
    for (int ctx = - tree.left_context_length();
         ctx <= tree.right_context_length(); ++ ctx) {
      for (auto cit = tree.categories().begin(), clast = tree.categories().end();
           cit != clast; ++ cit) {
        tree_question_ptr q =
          tree_question_ptr(new context_question(pnode->tree, cit->first, ctx));
        double gain = examine_question(q, statlist, minocc);
        if (gain > maxgain) {
          maxgain = gain;
          maxq = q;
        }
      }
    }

    for (int loc = 0; loc < 3; ++ loc) {
      tree_question_ptr q =
        tree_question_ptr(new location_question(pnode->tree, loc));
      double gain = examine_question(q, statlist, minocc);
      if (gain > maxgain) {
        maxgain = gain;
        maxq = q;
      }
    }

    *pgain = maxgain;
    return maxq;
  }

  void reassign_state_numbers(context_decision_tree* ptree) {
    std::queue<context_decision_tree_node*> que;
    std::set<int> used_ids;
    que.push(ptree->root());
    while (! que.empty()) {
      context_decision_tree_node* p = que.front();
      que.pop();
      if (p->is_leaf) {
        if (p->value > 0) used_ids.insert(p->value);
      } else {
        if (p->is_true) que.push(p->is_true);
        if (p->is_false) que.push(p->is_false);
      }
    }

    que = std::queue<context_decision_tree_node*>(); // clear
    que.push(ptree->root());
    int nid = 0;
    while (used_ids.find(nid) != used_ids.end()) ++ nid;

    while (! que.empty()) {
      context_decision_tree_node* p = que.front();
      que.pop();
      if (p->is_leaf) {
        if (p->value < 0) {
          p->value = nid;
          ++ nid;
          while (used_ids.find(nid) != used_ids.end()) ++ nid;
        }
      } else {
        if (p->is_true) que.push(p->is_true);
        if (p->is_false) que.push(p->is_false);
      }
    }
  }

  
  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t tree_src;
    load_variant(&tree_src, arg.input.getValue());
    context_decision_tree tree(tree_src);

    variant_t stats_src;
    load_variant(&stats_src, arg.stats.getValue());
    tree_statistics treestat(stats_src);

    tree.set_left_context_length(treestat.left_context_length());
    tree.set_right_context_length(treestat.right_context_length());
    
    tree_membership membership;
    treestat.compute_membership(tree, &membership);

    int id = 0;
    for (auto it = membership.begin(), last = membership.end();
         it != last; ++ it){
      std::cout << "Node " << id++ << " has " << it->second.size() << " stats" << std::endl;
    }
    
    tree_split_queue que;
    int nnosplit = 0;
    
    for (auto it = membership.begin(), last = membership.end();
         it != last; ++ it){
      double gain;
      if (! it->first->nosplit) {
        tree_question_ptr q = find_best_question(it->first, it->second, arg.minocc.getValue(), &gain);

        if (gain > arg.delta.getValue()) {
          que.push(boost::make_tuple(gain, it->first, q));
        }
      } else {
        ++ nnosplit;
      }
    }

    while (membership.size() < (arg.maxstate.getValue() - nnosplit) &&
           que.size() > 0) {
      boost::tuple<double, const context_decision_tree_node*,
                   tree_question_ptr> front = que.top();
      context_decision_tree_node* node =
        const_cast<context_decision_tree_node*>(front.get<1>());
      // ^ want to avoid const_cast, need to refactor
      double gain = front.get<0>();
      tree_question_ptr pq = front.get<2>();
      que.pop();
      if (gain < arg.delta.getValue()) continue;

      INFO("Split and gain %f", gain);
      
      // prepare new node and membership
      context_decision_tree_node* truenode = new context_decision_tree_node(-1, tree);
      context_decision_tree_node* falsenode = new context_decision_tree_node(-1, tree);
      node->is_leaf = false;
      node->nosplit = false;
      node->question = pq;
      node->is_true = truenode;
      node->is_false = falsenode;

      int nframes_true = 0, nframes_false = 0;
      for (auto it = membership[node].cbegin(), last = membership[node].cend();
           it != last; ++ it) {
        if (pq->question(it->get<0>())) {
          nframes_true += static_cast<int>(it->get<1>()->zero);
          membership[truenode].push_back(*it);
        } else {
          nframes_false += static_cast<int>(it->get<1>()->zero);
          membership[falsenode].push_back(*it);
        }
      }
      INFO("# frames = (%d, %d)", nframes_true, nframes_false);      
      membership.erase(node);      

      // find best_q for these
      double truegain, falsegain;
      tree_question_ptr trueq =
        find_best_question(truenode, membership[truenode], arg.minocc.getValue(), &truegain);
      tree_question_ptr falseq =
        find_best_question(falsenode, membership[falsenode], arg.minocc.getValue(), &falsegain);

      que.push(boost::make_tuple(truegain, truenode, trueq));
      que.push(boost::make_tuple(falsegain, falsenode, falseq));
    }
    INFO("Final # of leaves = %d", membership.size());

    reassign_state_numbers(&tree);

    
    variant_t output_src;
    tree.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacPCDT");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Tree splitter",
                          argc, argv, spin::tool_main);
}

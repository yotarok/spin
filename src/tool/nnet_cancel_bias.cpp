#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>

#include <spin/types.hpp>
#include <spin/nnet/nnet.hpp>
#include <spin/nnet/cache.hpp>
#include <spin/nnet/stream.hpp>
#include <spin/nnet/affine.hpp>
#include <spin/hmm/tree.hpp>
#include <spin/hmm/treestat.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>


#include <boost/random.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, tree, 
                   ("t", "tree", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, stats, 
                   ("s", "treestat", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, input,
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::ValueArg<std::string>, target,
                   ("c", "target", "", true, "", "COMPONENT"))
                  );


  int tool_main(Arg& arg, int argc, char* argv[]) {   
    INFO("Loading tree");
    variant_t tree_src;
    load_variant(&tree_src, arg.tree.getValue());
    context_decision_tree tree(tree_src);

    INFO("Loading initial parameter");
    variant_t input_src;
    load_variant(&input_src, arg.input.getValue());
    nnet nnet(input_src);

    variant_t treestat_src;
    load_variant(&treestat_src, arg.stats.getValue());
    tree_statistics treestat(treestat_src);
    
    std::vector<double> counts(tree.nstates(), 0.0);
    double denom = 0.0;
    // ^ denominator is actually not necessary for computing decoding score.
    // it's required only for computing posterior,
    // and it's cheap enough to compute.

    for (auto it = treestat.data().cbegin(), last = treestat.data().cend();
         it != last; ++ it) {
      int st = tree.resolve(it->first);
      counts[st] += it->second.zero;
      denom += it->second.zero;
    }
    double logdenom = std::log(denom);

    auto pnode = nnet.node(arg.target.getValue());
    if (! pnode) {
      ERROR("Target %s is not found", arg.target.getValue().c_str());
      return -1;
    }

    if (pnode->typetag() == std::string("affine")) {
      auto paff = std::dynamic_pointer_cast<nnet_node_affine_transform>(pnode);
      fmatrix bias(paff->bias().size1(), paff->bias().size2());
      viennacl::copy(paff->bias(), bias);
      for (int k = 0; k< counts.size(); ++ k) {
        double logp = std::log(counts[k]) - logdenom;
        std::cerr << "P(s = " << k << ") = " << std::exp(logp) << std::endl;
        bias(k, 0) -= logp;
      }
      viennacl::copy(bias, paff->bias());
    } else {
      ERROR("Target type %s is not supported", pnode->typetag());
      return -1;
    }

    INFO("Finished, writing output...");
    variant_t output_src;
    nnet.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinNnet");

    
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet initializer",
                          argc, argv, spin::tool_main);
}


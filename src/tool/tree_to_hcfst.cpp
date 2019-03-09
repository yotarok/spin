#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/io/variant.hpp>
#include <spin/io/fst.hpp>
#include <spin/hmm/tree.hpp>

#include <boost/algorithm/string.hpp>
namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, tree, 
                   ("t", "tree", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::ValueArg<int>, nstates,
                   ("", "nstates", "", false, 3, "N")),
                  (TCLAP::ValueArg<std::string>, disamb,
                   ("", "disamb", "", false, "", "S1,S2,..."))
                  );
  // as a first attempt, 
  // I will use zero transp and fixed num of states

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    variant_t tree_src;
    load_variant(&tree_src, arg.tree.getValue());
    context_decision_tree tree(tree_src);

    std::vector<std::string> disamb;
    {
      std::vector<std::string> disamb_raw;
      boost::split(disamb_raw, arg.disamb.getValue(), boost::is_any_of(","));
      for (auto it = disamb_raw.cbegin(), last = disamb_raw.cend();
           it != last; ++ it) {
        if (it->size() > 0) disamb.push_back(*it);
      }
    }
    
    fst::script::VectorFstClass hcfst = tree.create_HC_FST(arg.nstates.getValue(),
                                                           disamb);
    std::ofstream ofs(arg.output.getValue());
    if (arg.write_text.getValue()) {
      print_fst(std::cout, hcfst);
    } else {
      fst::FstWriteOptions opts;
      hcfst.Write(ofs, opts);
    }
    
    INFO("DONE");    
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Copy corpus (mainly intended for converting "
                          "corpus to/from binary format)",
                          argc, argv, spin::tool_main);
}


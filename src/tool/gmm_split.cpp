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
#include <spin/fscorer/diaggmm.hpp>
#include <spin/hmm/tree.hpp>

#include <boost/random.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, gmm,
                   ("", "gmm", "GMMs used for computing the initial statistics",
                    true, "", "FILE")),
                  (TCLAP::ValueArg<float>, delta,
                   ("", "delta", "Deviation factor",
                    false, 0.2, "delta")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t param_src;
    load_variant(&param_src, arg.gmm.getValue());
    diagonal_GMM_parameter gmm(param_src);
    
    size_t S = gmm.nstates();
    for (int s = 0; s < S; ++ s) {
      gmm.split_all_components(s, arg.delta.getValue());
    }

    variant_t output_src;
    gmm.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGMM");
    return 0;
  }
  

}

int main(int argc, char* argv[]) {
  return gear::wrap_main("GMM splitter",
                          argc, argv, spin::tool_main);
}


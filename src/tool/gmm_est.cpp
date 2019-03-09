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

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, stats,
                   ("s", "stats", "", true, "", "FILE")),
                  (TCLAP::ValueArg<float>, var_minocc,
                   //("", "var-minocc", "", false, 0.5, "OCC")),
                   ("", "var-minocc", "", false, 0.001, "OCC")),
                  (TCLAP::ValueArg<float>, var_floor,
                   ("", "var-floor", "", false, 0.001, "VAR")),
                   //("", "var-floor", "", false, 0.1, "VAR")),
                   //("", "var-floor", "", false, 0.01, "VAR")),
                   //("", "var-floor", "", false, 0.001, "VAR")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );
  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t param_src;
    load_variant(&param_src, arg.input.getValue());
    diagonal_GMM_parameter param(param_src);

    variant_t stats_src;
    load_variant(&stats_src, arg.stats.getValue());
    diagonal_GMM_statistics stats(stats_src);

    param.reestimation(stats,
                       arg.var_minocc.getValue(), arg.var_floor.getValue());

    int nframe = static_cast<int>(stats.get_zero().sum());
    INFO("# frames = %d", nframe);
    INFO("Log likelihood / # frames = %f", stats.get_loglikelihood() / nframe);

    variant_t output_src;
    param.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGMM");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Super GMM estimator",
                          argc, argv, spin::tool_main);
}

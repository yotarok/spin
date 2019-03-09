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
                  (TCLAP::ValueArg<std::string>, stats, 
                   ("s", "stats", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );
  
  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t stats_src;
    load_variant(&stats_src, arg.stats.getValue());
    diagonal_GMM_statistics stats(stats_src);

    diagonal_GMM_statistics::statscalar zeroth = stats.get_zero().sum();
    diagonal_GMM_statistics::statvector
      first = stats.get_first().rowwise().sum(),
      second = stats.get_second().rowwise().sum();

    diagonal_GMM_statistics::statvector mean = first / zeroth;
    diagonal_GMM_statistics::statvector
      var = second / zeroth - mean.array().square().matrix();
    
    fvector
      bias = - 1.0 * mean.cast<float>(),
      scale = var.array().sqrt().inverse().matrix().cast<float>();

    // As same as in Gear, spin uses "((weight * x) + bias) .* scale" for
    // feature transformation where ".*" denotes element-wise multiplication
    variant_map trans;
    trans["bias"] = fmatrix(bias);
    trans["scale"] = fmatrix(scale);

    write_variant(trans, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinAfTr");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Global CMVN affine transformation estimator",
                          argc, argv, spin::tool_main);
}


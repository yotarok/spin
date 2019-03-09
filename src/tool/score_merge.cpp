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

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::UnlabeledMultiArg<std::string>, scores,
                   ("scores", "Statistics.", true, "stats.."))
                  );
  int tool_main(Arg& arg, int argc, char* argv[]) {
    INFO("Loading %s...", arg.scores.getValue()[0].c_str());
    variant_t scores_src;
    load_variant(&scores_src, arg.scores.getValue()[0]);

    variant_map score = boost::get<variant_map>(scores_src);

    for (int n = 1; n < arg.scores.getValue().size(); ++ n) {
      INFO("Loading %s...", arg.scores.getValue()[n].c_str());
      variant_t scores_src;
      load_variant(&scores_src, arg.scores.getValue()[n]);

      variant_map&  otherscore = boost::get<variant_map>(scores_src);

      const char* intprops[] = {
        "seg_total", "seg_error", "token_total", "token_sub_err",
        "token_del_err", "token_ins_err", "frame_total", "decode_msec"
      };
      for (int j = 0; j < sizeof(intprops) / sizeof(intprops[0]); ++ j) {
        score[intprops[j]] =
          boost::get<int>(score[intprops[j]]) +
          boost::get<int>(otherscore[intprops[j]]);
      }
    }

    write_variant(score, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinEval");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Decode score merger",
                          argc, argv, spin::tool_main);
}


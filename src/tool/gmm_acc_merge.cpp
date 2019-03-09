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
                  (TCLAP::UnlabeledMultiArg<std::string>, stats,
                   ("stats", "Statistics.", true, "stats.."))
                  );
  int tool_main(Arg& arg, int argc, char* argv[]) {
    INFO("Loading %s...", arg.stats.getValue()[0].c_str());
    variant_t stats_src;
    load_variant(&stats_src, arg.stats.getValue()[0]);
    diagonal_GMM_statistics stats(stats_src);

    for (int n = 1; n < arg.stats.getValue().size(); ++ n) {
      INFO("Loading %s...", arg.stats.getValue()[n].c_str());
      variant_t stats_src;
      load_variant(&stats_src, arg.stats.getValue()[n]);
      diagonal_GMM_statistics otherstats(stats_src);

      stats.accumulate(otherstats);
    }
    variant_t stat_src;
    stats.write(&stat_src);
    write_variant(stat_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGST");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("GMM staistics merger",
                          argc, argv, spin::tool_main);
}


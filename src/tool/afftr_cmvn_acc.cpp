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
                  (TCLAP::ValueArg<std::string>, features, 
                   ("f", "features", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
   
    boost::shared_ptr<diagonal_GMM_statistics> pstat;
    typedef diagonal_GMM_statistics::statscalar statscalar;
    typedef diagonal_GMM_statistics::statvector statvector;
    for (int n = 0 ; ! fit->done() ; fit->next(), ++ n) {
      corpus_entry input_feat = fit->value();
      INFO("Processing %s...", fit->get_key().c_str());

      fmatrix feats = boost::get<fmatrix>(input_feat["feature"]);
      int D = feats.rows();
      if (! pstat) {
        pstat.reset(new diagonal_GMM_statistics(D, 1, 1, 1, 1));
      }
      pstat->get_zero()(0, 0) += feats.cols();
      statvector first(pstat->get_first().col(0));
      first += feats.rowwise().sum().cast<statscalar>();
      pstat->get_first().col(0) = first;
      statvector second(pstat->get_second().col(0));
      second += feats.array().square().rowwise().sum().matrix().cast<statscalar>();
      pstat->get_second().col(0) = second;
    }

    variant_t stat_src;
    pstat->write(&stat_src);
    write_variant(stat_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGST");

    return 0;

  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Global Gaussian statistics accumulator",
                          argc, argv, spin::tool_main);
}


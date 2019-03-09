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
                  (TCLAP::ValueArg<std::string>, alignment, 
                   ("a", "alignment", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, features, 
                   ("f", "features", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::ValueArg<std::string>, gmmdef,
                   ("g", "gmmdef", "GMM definition", true, "", "FILE"))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    corpus_iterator_ptr ait = make_corpus_iterator(arg.alignment.getValue());
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("alignment", ait,
                                                "features", fit);
    variant_t param_src;
    load_variant(&param_src, arg.gmmdef.getValue());
   
    diagonal_GMM_parameter param(param_src);
    diagonal_GMM_statistics stat(param);
    
    for (int n = 0 ; ! zit->done() ; zit->next(), ++ n) {
      corpus_entry input_al = zit->value(0);
      corpus_entry input_feat = zit->value(1);
      INFO("Processing %s...", zit->get_key().c_str());
      
      vector_fst alignment = 
        boost::get<vector_fst>(input_al["alignment"]);

      if (! CheckLinearity(alignment)) {
        WARN("%s is not linear, skip.", zit->get_key().c_str());
        continue;
      }

      fmatrix feats = boost::get<fmatrix>(input_feat["feature"]);
      const fst::Fst<LatticeArc>* palign = alignment.GetFst<LatticeArc>();

      for (fst::StateIterator<fst::Fst<LatticeArc> > stit(*palign);
           ! stit.Done(); stit.Next()) {
        for (fst::ArcIterator<fst::Fst<LatticeArc> > ait(*palign, stit.Value());
             ! ait.Done(); ait.Next()) {
          LatticeArc arc = ait.Value();
          int stt = arc.weight.Time().Start(), ent = arc.weight.Time().End();
          stt = std::max(stt, 0);
          ent = std::min(ent, (int) feats.cols());

          if (arc.ilabel == 0) continue;
          std::vector<std::string> stids;
          std::string isym = palign->InputSymbols()->Find(arc.ilabel);
          boost::split(stids, isym, boost::is_any_of(";"));

          if (stids.size() == 0 || stids[0].size() == 0 || stids[0][0] != 'S') {
            if (stt < ent) {
              throw std::runtime_error("Invalid lattice input label " + stids[0]);
            } else {
              continue;
            }
          }

          int s = boost::lexical_cast<int>(stids[0].substr(1));
          //std::cout << "Acc to " << s << std::endl;
          if (ent > stt) {
            stat.accumulate(s, feats.block(0, stt, feats.rows(), ent - stt),
                            param);
          }
        }
      }
      
    }

    variant_t stat_src;
    stat.write(&stat_src);
    write_variant(stat_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGST");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("GMM accumulator",
                          argc, argv, spin::tool_main);
}


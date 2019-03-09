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
#include <spin/hmm/treestat.hpp>
#include <fst/script/topsort.h>

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
                  (TCLAP::ValueArg<int>, left,
                   ("l", "left", "Left-context size", true, 1, "N")),
                  (TCLAP::ValueArg<int>, right,
                   ("r", "right", "Right-context size", true, 1, "N"))
                  );
  

  int tool_main(Arg& arg, int argc, char* argv[]) {
    corpus_iterator_ptr ait = make_corpus_iterator(arg.alignment.getValue());
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("alignment", ait,
                                                "features", fit);

    tree_statistics treestat;

    treestat.set_left_context_length(arg.left.getValue());
    treestat.set_right_context_length(arg.right.getValue());
    
    for (int n = 0 ; ! zit->done() ; zit->next(), ++ n) {
      corpus_entry input_al = zit->value(0);
      corpus_entry input_feat = zit->value(1);
      INFO("Processing %s...", zit->get_key().c_str());

      vector_fst alignment = 
        boost::get<vector_fst>(input_al["alignment"]);
      fmatrix feats = boost::get<fmatrix>(input_feat["feature"]);


      if (! CheckLinearity(alignment)) {
        WARN("%s is not linear, skip.", zit->get_key().c_str());
        continue;
      }
      if (! fst::script::TopSort(&alignment)) {
        throw std::runtime_error("Topological sort failed");
      }

      std::vector<HMM_context> ctxlist;
      auto palign = alignment.GetFst<LatticeArc>();

      get_alignment_context(alignment, arg.left.getValue(), arg.right.getValue(), &ctxlist);

      std::vector<LatticeWeight> weights;
      ExtractWeights(*palign, &weights);
      assert(weights.size() == ctxlist.size());
      for (int n = 0; n < ctxlist.size(); ++ n) {
        
        int stt = weights[n].Time().Start(), ent = weights[n].Time().End();

        stt = std::max(stt, 0);
        ent = std::min(ent, (int) feats.cols());
        if (ent > stt) {
          treestat.accumulate(ctxlist[n], feats.block(0, stt, feats.rows(), ent - stt));
        }
      }
    }
    INFO("Writing statistics...");

    variant_t stat_src;
    treestat.write(&stat_src);
    write_variant(stat_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacTRST");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Tree staistics accumulator",
                          argc, argv, spin::tool_main);
}

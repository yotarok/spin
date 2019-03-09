#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/corpus/corpus.hpp>
#include <spin/io/fst.hpp>
#include <spin/fst/text_compose.hpp>
#include <fst/script/connect.h>
#include <spin/fst/linear.hpp>

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, inputtag,
                   ("I", "inputtag", "", false, "alignment", "TAG")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, outputtag,
                   ("O", "outputtag", "", false, "state", "TAG")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());

    for ( ; ! cit->done() ; cit->next()) {
      INFO("Processing %s", cit->get_key().c_str());
      corpus_entry input = cit->value();
      corpus_entry output;
      copy_sticky_tags(&output, input);

      vector_fst alignment = 
        boost::get<vector_fst>(input[arg.inputtag.getValue()]);

      if (! CheckLinearity(alignment)) {
        WARN("%s is not linear, skip.", cit->get_key().c_str());
        continue;
      }

      const fst::Fst<LatticeArc>* palign = alignment.GetFst<LatticeArc>();

      int length = 0;
      for (fst::StateIterator<fst::Fst<LatticeArc> > stit(*palign);
           ! stit.Done(); stit.Next()) {
        for (fst::ArcIterator<fst::Fst<LatticeArc> > ait(*palign, stit.Value());
             ! ait.Done(); ait.Next()) {
          LatticeArc arc = ait.Value();
          if (length < arc.weight.Time().End()) length = arc.weight.Time().End();
        }
      }
      intmatrix stid = intmatrix::Constant(1, length, -1);

      for (fst::StateIterator<fst::Fst<LatticeArc> > stit(*palign);
           ! stit.Done(); stit.Next()) {
        for (fst::ArcIterator<fst::Fst<LatticeArc> > ait(*palign, stit.Value());
             ! ait.Done(); ait.Next()) {
          LatticeArc arc = ait.Value();
          if (arc.ilabel == 0) continue;
          if (arc.weight.Time().Start() >= arc.weight.Time().End()) continue;

          std::vector<std::string> comps;
          std::string isym = palign->InputSymbols()->Find(arc.ilabel);
          boost::split(comps, isym, boost::is_any_of(";"));
          if (comps.size() == 0 || comps[0].size() == 0 || comps[0][0] != 'S') {
            throw std::runtime_error("Invalid lattice input label " + comps[0]);
          }

          int s = boost::lexical_cast<int>(comps[0].substr(1));
          for (int tau = arc.weight.Time().Start();
               tau < arc.weight.Time().End(); ++ tau) {
            stid(tau, 0) = s;
          }
        }
      }
    
      if (stid.minCoeff() < 0) {
        throw std::runtime_error("Alignment doesn't cover whole utterance");
      }

      output[arg.outputtag.getValue()] = stid;
      writer->write(output);
    }

    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Convert (graph-style) alignment to "
                         "integer matrix of state ids",
                          argc, argv, spin::tool_main);
}


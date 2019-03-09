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
#include <spin/io/yaml.hpp>
#include <spin/fst/linear.hpp>
#include <fst/script/randgen.h>


namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, stategraph, 
                   ("s", "stategraph", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, features, 
                   ("f", "features", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, outputtag,
                   ("O", "outputtag", "", false, "alignment", "TAG")),
                  (TCLAP::ValueArg<int>, randseed,
                   ("", "randseed", "", false, 43, "seed")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );
  
  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr sgit = make_corpus_iterator(arg.stategraph.getValue());
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("stategraph", sgit,
                                                "features", fit);
    
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());
    
    for (int n = 0 ; ! zit->done() ; zit->next(), ++ n) {
      corpus_entry input_sg = zit->value(0), input_feat = zit->value(1);
      INFO("Processing %s...", zit->get_key().c_str());
      corpus_entry output;
      copy_sticky_tags(&output, input_sg);

      fst::script::VectorFstClass stategraph = 
        boost::get<fst::script::VectorFstClass>(input_sg["stategraph"]);
      if (! CheckLinearity(stategraph)) {
        INFO("%s is not linear, perform random sample", zit->get_key().c_str());
        fst::script::VectorFstClass rstategraph(stategraph);
        fst::RandGenOptions<fst::script::RandArcSelection>
          ropt(fst::script::FAST_LOG_PROB_ARC_SELECTOR);
        
        fst::script::RandGen(stategraph, &rstategraph,
                             arg.randseed.getValue() + n, ropt);
        stategraph = rstategraph;
      }

      std::vector<int> isyms, osyms;
      ExtractString(stategraph, false, &osyms);
      ExtractString(stategraph, true, &isyms);
      int nstates = 0;

      for (int n = 0; n < isyms.size(); ++ n) {
        if (stategraph.InputSymbols()->Find(isyms[n])[0] == 'S') {
          nstates += 1;
        }
      }

      Lattice alignment;
      int flen = boost::get<fmatrix>(input_feat["feature"]).cols();

      //std::cout << "# frames = " << flen;
      float fflen = static_cast<float>(flen);
      float fsyms = static_cast<float>(isyms.size());
      Lattice lattice;
      
      int stst = lattice.AddState();
      lattice.SetStart(stst);
      fst::SymbolTable itable, otable;
      itable.AddSymbol("<eps>", 0);
      otable.AddSymbol("<eps>", 0);
      int prevst = stst;

      int off = 0;
      for (int n = 0; n < isyms.size(); ++ n) {
        int nst = lattice.AddState();
        LatticeArc arc;
        arc.ilabel = prepare_label(&itable,
                                   stategraph.InputSymbols()->Find(isyms[n]));
        arc.olabel = prepare_label(&otable,
                                   stategraph.OutputSymbols()->Find(osyms[n]));

        int beg = static_cast<int>(off * fflen / fsyms);
        int end = beg;
        if (stategraph.InputSymbols()->Find(isyms[n])[0] == 'S') {
          end = static_cast<int>((off + 1) * fflen / fsyms);
          off += 1;
        } 
        arc.weight = LatticeWeight(0.0, 0.0, TimingWeight<int>(beg, end));
        arc.nextstate = nst;
        lattice.AddArc(prevst, arc);

        prevst = nst;
      }
      lattice.SetFinal(prevst, LatticeWeight::One());
      lattice.SetInputSymbols(&itable);
      lattice.SetOutputSymbols(&otable);

      output["alignment"] = fst::script::VectorFstClass(lattice);
      writer->write(output);
    }
    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Super alignment generator",
                          argc, argv, spin::tool_main);
}


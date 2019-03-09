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

#include <fst/map.h>
#include <fst/script/map.h>
#include <fst/script/project.h>
#include <fst/script/shortest-path.h>
#include <fst/script/rmepsilon.h>
#include <fst/script/arcsort.h>
#include <fst/script/topsort.h>

#include <spin/io/variant.hpp>
#include <spin/fst/text_compose.hpp>

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, ref, 
                   ("r", "ref", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, reftag, 
                   ("R", "reftag", "", false, "wordgraph", "TAG")),
                  (TCLAP::ValueArg<std::string>, hyp,
                   ("", "hyp", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, hyptag,
                   ("H", "hyptag", "", false, "decoded", "TAG")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, stat_output,
                   ("", "stat-output", "", false, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  inline vector_fst convert_to_stdfst(vector_fst f) {
    if (f.ArcType() == "standard") {
      return f;
    }
    else if (f.ArcType() == "lattice") {
      fst::MutableFst<LatticeArc>* impl = f.GetMutableFst<LatticeArc>();
      fst::VectorFst<fst::StdArc> dest;
      fst::WeightConvertMapper<LatticeArc, fst::StdArc> func;
      fst::ArcMap(*impl, &dest, &func);
      return vector_fst(dest);
      //fst_t* fst = fst::script::Map(f, fst::script::TO_STD_MAPPER, 0.0,
      //fst::script::WeightClass("lattice", "(0,0,[0:0])"));
      //return vector_fst(*fst);
    } else {
      FATAL("Unknown FST type");
    }
  }

  inline vector_fst make_matcher(vector_fst acceptor, bool is_ref,
                                 float subcost = 1.0,
                                 float delcost = 1.0,
                                 float inscost = 1.0) {
    if (acceptor.InputSymbols()->LabeledCheckSum() !=
        acceptor.OutputSymbols()->LabeledCheckSum()) {
      throw std::runtime_error("Input and output symbol tables do not match, do projection before making matcher");
    }
    
    fst::VectorFst<fst::StdArc> matcher;
    fst::SymbolTable augsyms(*(acceptor.InputSymbols()));
    int sublab = augsyms.AddSymbol("SUB");
    int dellab = augsyms.AddSymbol("DEL");
    int inslab = augsyms.AddSymbol("INS");
    
    
    if (is_ref) {
      matcher.SetInputSymbols(acceptor.InputSymbols());
      matcher.SetOutputSymbols(&augsyms);
    }
    else { 
      matcher.SetOutputSymbols(acceptor.InputSymbols());
      matcher.SetInputSymbols(&augsyms);
    }

    int the_state = matcher.AddState();
    matcher.SetStart(the_state);
    matcher.SetFinal(the_state, fst::TropicalWeight::One());

    // make correct/substitution arcs
    for (fst::SymbolTableIterator stit(*acceptor.InputSymbols()); ! stit.Done();
         stit.Next()) {
      if (stit.Value() == 0) continue;
      fst::StdArc corarc(stit.Value(), stit.Value(),
                         fst::TropicalWeight::One(), the_state);
      matcher.AddArc(the_state, corarc);

      fst::StdArc subarc(stit.Value(), sublab,
                         fst::TropicalWeight(subcost/2), the_state);
      if (! is_ref) std::swap(subarc.ilabel, subarc.olabel);
      matcher.AddArc(the_state, subarc);

      if (is_ref) {
        fst::StdArc delarc(stit.Value(), dellab,
                           fst::TropicalWeight(delcost/2), the_state);
        matcher.AddArc(the_state, delarc);
      } else {
        fst::StdArc insarc(inslab, stit.Value(),
                           fst::TropicalWeight(inscost/2), the_state);
        matcher.AddArc(the_state, insarc);
      }
    }
    if (is_ref) {
      fst::StdArc insarc(0, inslab, fst::TropicalWeight(inscost/2), the_state);
      matcher.AddArc(the_state, insarc);
    } else {
      fst::StdArc delarc(dellab, 0, fst::TropicalWeight(delcost/2), the_state);
      matcher.AddArc(the_state, delarc);
    }

    
    return vector_fst(matcher);
  }
  
  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr refit = make_corpus_iterator(arg.ref.getValue());
    corpus_iterator_ptr hypit = make_corpus_iterator(arg.hyp.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("ref", refit,
                                                "hyp", hypit);

    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());


    int total_num_frames = 0;
    int total_decode_msec = 0;
    int seg_numref = 0, seg_err = 0;
    int all_numref = 0, all_sub = 0, all_ins = 0, all_del = 0;
    
    for (int n = 0 ; ! zit->done() ; zit->next(), ++ n) {
      corpus_entry input_ref = zit->value(0), input_hyp = zit->value(1);
      INFO("Processing %s...", zit->get_key().c_str());
      corpus_entry output;
      copy_sticky_tags(&output, input_ref);

      vector_fst ref = 
        boost::get<vector_fst>(input_ref[arg.reftag.getValue()]);
      vector_fst hyp = 
        boost::get<vector_fst>(input_hyp[arg.hyptag.getValue()]);

      ref = convert_to_stdfst(ref);
      hyp = convert_to_stdfst(hyp);

      // project
      fst::script::Project(&ref, fst::PROJECT_INPUT);
      fst::script::Project(&hyp, fst::PROJECT_OUTPUT);
      fst::script::RmEpsilon(&ref, true);
      fst::script::RmEpsilon(&hyp, true);

      fst::RmWeightMapper<fst::StdArc> rmweight;
      fst::Map(ref.GetMutableFst<fst::StdArc>(), rmweight);
      fst::Map(hyp.GetMutableFst<fst::StdArc>(), rmweight);

      try {
        int decms = boost::get<int>(input_hyp["+decode_msec"]);
        int numfr = boost::get<int>(input_hyp["+num_frames"]);
        total_num_frames += numfr;
        total_decode_msec += decms;
      } catch(boost::bad_get) {
        WARN("Cannot read decoding time statistics");
      }
      
      // make matchers
      vector_fst ref_match = make_matcher(ref, true);
      vector_fst hyp_match = make_matcher(hyp, false);

      vector_fst augref = vector_fst(fst_textual_compose(ref, ref_match));
      vector_fst aughyp = vector_fst(fst_textual_compose(hyp_match, hyp));

      fst::script::ArcSort(&augref, fst::script::OLABEL_SORT);
      fst::script::ArcSort(&aughyp, fst::script::ILABEL_SORT);

      fst_t align = fst_textual_compose(augref, aughyp);

      vector_fst shortest_align("standard");

      fst::script::ShortestPath(align, &shortest_align, 1, false, false,
                                fst::script::WeightClass::Zero("tropical"));

      // Not really necessary, but for readability of text format
      fst::script::TopSort(&shortest_align);

      int numref = 0, sub = 0, ins = 0, del = 0;
      std::vector<int> ilabels, olabels;
      ExtractString(*shortest_align.GetFst<fst::StdArc>(), true, &ilabels);
      ExtractString(*shortest_align.GetFst<fst::StdArc>(), false, &olabels);

      for (int i = 0; i < ilabels.size(); ++ i) {
        if (ilabels[i] != 0) { numref += 1; }

        if (ilabels[i] == 0 && olabels[i] == 0) continue;
        else if (ilabels[i] != 0 && olabels[i] == 0) {
          del += 1;
        } else if (ilabels[i] == 0 && olabels[i] != 0) {
          ins += 1;
        } else if (shortest_align.InputSymbols()->Find(ilabels[i]) ==
                   shortest_align.OutputSymbols()->Find(olabels[i])) {
        } else {
          sub += 1;
        }
      }
      all_numref += numref;
      all_sub += sub;
      all_del += del;
      all_ins += ins;
      seg_numref += 1;
      seg_err += (sub == 0 && del == 0 && ins == 0) ? 0 : 1;
      
      output["alignment"] = shortest_align;
      output["num_ref"] = numref;
      output["num_sub"] = sub;
      output["num_del"] = del;
      output["num_ins"] = ins;
      writer->write(output);
    }
    
    std::cerr << "# segment processed (1) = " << seg_numref
              << ", # segment w error (2) = " << seg_err
              << ", (1) / (2) =  "
              << static_cast<float>(seg_err) / seg_numref << std::endl;
    std::cerr << "N = " << all_numref << ", S = " << all_sub
              << ", D = " << all_del << ", I = " << all_ins << std::endl;
    std::cerr << "(S + D + I) / N = "
              << (static_cast<float>(all_sub + all_del + all_ins) / all_numref)
              << std::endl;
    if (total_decode_msec != 0) {
      std::cerr << "Average FPS = " <<
        static_cast<double>(total_num_frames) / 
        static_cast<double>(total_decode_msec) * 1000.0
                << std::endl;
    }

    if (arg.stat_output.isSet()) {
      variant_map stat;
      stat["seg_total"] = static_cast<int>(seg_numref);
      stat["seg_error"] = static_cast<int>(seg_err);
      stat["token_total"] = static_cast<int>(all_numref);
      stat["token_sub_err"] = static_cast<int>(all_sub);
      stat["token_del_err"] = static_cast<int>(all_del);
      stat["token_ins_err"] = static_cast<int>(all_ins);
      stat["frame_total"] = static_cast<int>(total_num_frames);
      stat["decode_msec"] = static_cast<int>(total_decode_msec);
      write_variant(stat, arg.stat_output.getValue(),
                    arg.write_text.isSet(), "SpinEval");
    }
    return 0;
  }
  
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("FST alignment for ASR evaluation",
                          argc, argv, spin::tool_main);
}

  

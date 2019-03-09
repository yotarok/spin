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
#include <fst/script/convert.h>
#include <spin/io/variant.hpp>
#include <spin/fscorer/diaggmm.hpp>
#include <spin/decode/decoder.hpp>
#include <spin/utils.hpp>
#include <spin/flow/flowutils.hpp>
#ifdef SPIN_WITH_NNET
#  include <spin/fscorer/nnet_scorer.hpp>
#  include <spin/nnet/nnet.hpp>
#endif

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
                  (TCLAP::ValueArg<std::string>, scorer,
                   ("m", "scorer", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, flow, 
                   ("", "flow", "", false, "", "FILE")),
                  (TCLAP::ValueArg<float>, acscale,
                   ("", "acscale", "", false, 0.1, "SCALE")),
                  (TCLAP::ValueArg<float>, beam,
                   ("", "beam", "", false, 8000.0, "BEAM")),
                  (TCLAP::ValueArg<int>, maxactive,
                   ("", "maxactive", "", false, 30000, "N")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );
  
  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr sgit = make_corpus_iterator(arg.stategraph.getValue());
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("stategraph", sgit,
                                                "features", fit);

    variant_t scorer_src;
    std::string scorer_type;
    std::shared_ptr<frame_scorer> pscorer;
    load_variant(&scorer_src, &scorer_type, arg.scorer.getValue());
    if (scorer_type == "StacDGMM" || scorer_type == "SpinDGMM") {
      diagonal_GMM_parameter_ptr param(new diagonal_GMM_parameter(scorer_src));
      pscorer.reset(new diagonal_GMM_scorer(param));
    }
#ifdef SPIN_WITH_NNET
    else if (scorer_type == "SpinNnet") {
      std::shared_ptr<nnet> param(new nnet(scorer_src));
      pscorer.reset(new nnet_scorer(param));
    }
#endif

    gear::flow_ptr flow;
    if (arg.flow.isSet()) {
      flow = gear::parse_flow(arg.flow.getValue());
      add_source_sink_inplace<float>(flow);
    }

    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());

    for (int n = 0 ; ! zit->done() ; zit->next(), ++ n) {
      corpus_entry input_sg = zit->value(0), input_feat = zit->value(1);
      INFO("Processing %s...", zit->get_key().c_str());
      corpus_entry output;
      copy_sticky_tags(&output, input_sg);

      apply_matrix_flow_inplace<float>(&input_feat["feature"], flow);
      fmatrix feats = boost::get<fmatrix>(input_feat["feature"]);

      fst::MutableFst<fst::StdArc>* pnet =
        boost::get<vector_fst>(input_sg["stategraph"])
        .GetMutableFst<fst::StdArc>();

      double timer_start = get_wall_time();

      decoder decoder(*pnet, pscorer.get());
      decoder.set_max_active(arg.maxactive.getValue());
      decoder.set_beam_width(arg.beam.getValue());
      decoder.set_acoustic_scale(arg.acscale.getValue());
      //decoder.parse_state_symbols();
      
      decoder.push_init();
      bool decoded = true;
      try {
        decoder.push_input(feats);
        if (! decoder.push_final()) {
          INFO("Cannot reach to a final state");
          decoded = false;
        } 

      } catch(no_hypothesis& hyp) {
        INFO("Cannot reach to a final state");
        decoded = false;
      }

      if (decoded) {
        Lattice lattice;
        float finalw = decoder.extract_lattice(&lattice, 1);
        double timer_end = get_wall_time();
        double dur_sec = (timer_end - timer_start);
        INFO("Final weight = %f, FPS = %f", finalw, feats.cols() / dur_sec);

        
        output["alignment"] = fst::script::VectorFstClass(lattice);
        writer->write(output);
      }
    }
    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Super alignment generator",
                          argc, argv, spin::tool_main);
}


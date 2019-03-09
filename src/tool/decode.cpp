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
#ifdef SPIN_WITH_NNET
#  include <spin/fscorer/nnet_scorer.hpp>
#  include <spin/nnet/nnet.hpp>
#endif
#include <spin/decode/decoder.hpp>
#include <spin/utils.hpp>
#include <spin/flow/flowutils.hpp>


namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, graph, 
                   ("g", "graph", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, features, 
                   ("f", "features", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, outputtag,
                   ("O", "outputtag", "", false, "decoded", "TAG")),
                  (TCLAP::ValueArg<std::string>, scorer,
                   ("m", "scorer", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, flow, 
                   ("", "flow", "", false, "", "FILE")),
                  (TCLAP::ValueArg<float>, acscale,
                   ("", "acscale", "", false, 0.2, "SCALE")),
                  (TCLAP::ValueArg<float>, beam,
                   ("", "beam", "", false, 15.0, "BEAM")),
                  (TCLAP::ValueArg<int>, maxactive,
                   ("", "maxactive", "", false, 8000, "N")),
                  (TCLAP::ValueArg<int>, maxbranch,
                   ("", "maxbranch", "", false, 1, "M")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  
  int tool_main(arg_type& arg, int argc, char* argv[]) {
    char* gpuid = ::getenv("SPIN_GPUID");
    std::vector<viennacl::ocl::device> devices = viennacl::ocl::platform().devices();
    int device_id = 0;
    if (gpuid) {
      try {
	device_id = boost::lexical_cast<int>(gpuid);
      } catch(...) {
      }
      if (device_id >= devices.size()) device_id = 0;
      std::cerr << "Device Info:" << std::endl << devices[device_id].info() << std::endl;
    }
    viennacl::ocl::setup_context(0, devices[device_id]);
    viennacl::ocl::switch_context(0);
    viennacl::ocl::current_context().switch_device(devices[device_id]);

    viennacl::ocl::current_context().build_options("-cl-mad-enable -cl-unsafe-math-optimizations -cl-fast-relaxed-math -cl-no-signed-zeros -cl-single-precision-constant");

    
    corpus_iterator_ptr cit = make_corpus_iterator(arg.features.getValue());

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

    vector_fst_ptr decodegraph = read_fst_file(arg.graph.getValue());

    fst::MutableFst<fst::StdArc>* pnet = decodegraph->GetMutableFst<fst::StdArc>();
    //parse_state_symbols(pnet, scorer.nstates());

    decoder decoder(*pnet, pscorer.get());
    decoder.set_max_active(arg.maxactive.getValue());
    decoder.set_beam_width(arg.beam.getValue());
    decoder.set_acoustic_scale(arg.acscale.getValue());
    decoder.set_max_branch(arg.maxbranch.getValue());
    
    for (int n = 0 ; ! cit->done() ; cit->next(), ++ n) {
      corpus_entry input = cit->value();
      INFO("Processing %s...", cit->get_key().c_str());

     
      corpus_entry output;
      copy_sticky_tags(&output, input);

      apply_matrix_flow_inplace<float>(&input["feature"], flow);
      fmatrix feats = boost::get<fmatrix>(input["feature"]);

      double timer_start = get_wall_time();
      
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
        float finalw = decoder.extract_lattice(&lattice, arg.maxbranch.getValue());
        double timer_end = get_wall_time();
        double dur_sec = (timer_end - timer_start);
        INFO("Final weight = %f, FPS = %f", finalw, feats.cols() / dur_sec);

        fst::VectorFst<fst::StdArc> stdlattice;
        ArcMap(lattice, &stdlattice,
               fst::WeightConvertMapper<LatticeArc, fst::StdArc>());
        stdlattice.Write("testfst.bin");

        output["+num_frames"] = static_cast<int>(feats.cols());
        output["+decode_msec"] = static_cast<int>(dur_sec * 1000.0);
        output[arg.outputtag.getValue()] = fst::script::VectorFstClass(lattice);
        writer->write(output);
      }
    }
    return 0;
  }
  
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("decoder",
                          argc, argv, spin::tool_main);
}


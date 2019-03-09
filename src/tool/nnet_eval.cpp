#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>
#include <spin/io/variant.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/nnet/nnet.hpp>
#include <spin/nnet/stream.hpp>
#include <spin/nnet/io.hpp>

#include <boost/random.hpp>
#include <boost/lexical_cast.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::MultiArg<std::string>, specs, 
                   ("S", "streamspec", "", true, "TYPE:CORPUS:TAG:NODE:FLOWFILE")),
                  (TCLAP::ValueArg<std::string>, input,
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );


  int tool_main(Arg& arg, int argc, char* argv[]) {
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
    
    INFO("Loading initial parameter");
    variant_t input_src;
    load_variant(&input_src, arg.input.getValue());
    nnet nnet(input_src);

    nnet.dump_shape_info(std::cerr);
    
    INFO("Loading corpora");
    std::vector<stream_specifier> specs;
    for (auto it = arg.specs.getValue().cbegin(),
           last = arg.specs.getValue().cend(); it != last; ++ it) {
      specs.push_back(stream_specifier(*it));
    }
        

    INFO("Setting up streams");
    std::vector<stream_ptr> streams; 
    std::vector<gear::flow_ptr> flows;
    for (auto it = specs.cbegin(), last = specs.cend(); it != last; ++ it) {
      streams.push_back(stream::create(*it));
      gear::flow_ptr pflow = (it->flow_path.size() == 0) ?
        gear::flow_ptr() : gear::parse_flow(it->flow_path);
      flows.push_back(pflow);
    }

    nnet_input_data nn_input_data(streams, flows);
    nnet_output_writer nn_output_writer(streams);

    nnet_context context(nnet, 1, 4096);
    nnet_config config(nnet);
    
    INFO("Start feed-forward");
    while(! nn_input_data.done()) {
      std::vector<corpus_entry> batch(1, corpus_entry());
      nn_input_data.pull_next_sequence(&batch[0]);

      INFO("Processing %s...",
           boost::get<std::string>(batch[0]["+key"]).c_str());


      context.before_forward(streams, batch);
      nnet.feed_forward(config, &context);

      context.before_backward(streams, batch);

      // Output
      corpus_entry metainfo;
      nn_output_writer.write_sequence(context, batch[0]);
    }

    INFO("Finished, writing output...");
    variant_map output;
    for (auto it = streams.begin(), last = streams.end(); it != last; ++ it) {
      variant_t stream_stat;
      (*it)->write_statistics(&stream_stat);
      output[(*it)->target_component()] = stream_stat;
    }
    write_variant(output, arg.output.getValue(),
                  arg.write_text.isSet(), "StacNNSS");

    INFO("FIN");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet initializer",
                          argc, argv, spin::tool_main);
}


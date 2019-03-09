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
#include <spin/nnet/cache.hpp>
#include <spin/nnet/stream.hpp>

#include <boost/random.hpp>
#include <boost/lexical_cast.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::MultiArg<std::string>, specs, 
                   ("S", "streamspec", "", true, "TYPE:CORPUS:TAG:NODE:FLOWFILE")),
                  (TCLAP::ValueArg<size_t>, batchsize, 
                   ("B", "batchsize", "", false, 128, "N")),
                  (TCLAP::ValueArg<size_t>, cachesize, 
                   ("C", "cachesize", "", false, 524288, "N")),
                  (TCLAP::ValueArg<float>, reportfreq, 
                   ("", "reportfreq", "", false, 15.0, "SECOND")),
                  (TCLAP::MultiArg<std::string>, updateparams,
                   ("U", "updateparam", "", false, "KEY=VALUE")),
                  (TCLAP::ValueArg<int>, seed, 
                   ("", "seed", "", false, 0x5EED, "M")),
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

    INFO ("Setting up randomization cache");
    random_frame_cache cache(nn_input_data, streams,
                             arg.batchsize.getValue(),
                             arg.cachesize.getValue(), arg.seed.getValue());
    cache.initialize();
    
    INFO("Start training");
    nnet_delta delta(nnet);

    nnet_config config(nnet);
    for (auto it = arg.updateparams.getValue().cbegin(),
           last = arg.updateparams.getValue().cend(); it != last; ++ it) {
      config.load_option(*it);
    }
    config.load_option("training=1");

    float total_loss = 0.0f;
    float loss_since_last_report = 0.0f;
    int nbatch = 0;
    nnet_context context(nnet, 1, arg.batchsize.getValue());
    std::vector<corpus_entry> batch(1, corpus_entry());

    double last_report_time = get_wall_time();
    int last_report_batch = 0;
    while (! cache.done()) {
      cache.retrieve(&batch[0]);

      context.before_forward(streams, batch);
      nnet.feed_forward(config, &context);

      try {
        context.before_backward(streams, batch);
      } catch(optimization_diverged&) {
        INFO("Diverged... Write intermediate result to ./dump.bin");
        variant_t output_src;
        nnet.write(&output_src);
        write_variant(output_src, "./dump.bin", false, "SpinNnet");
        throw;
      }
      nnet.back_propagate(config, &context);

      float cur_loss = 0.0;
      for (auto it = streams.begin(), last = streams.end(); it != last; ++ it) {
        cur_loss += (*it)->loss();
        (*it)->clear_loss();
      }
      total_loss += cur_loss;
      loss_since_last_report += cur_loss;

      context.accumulate_delta(config, &delta);
      delta.add_regularizer(config, nnet);
      nnet.update(config, delta);
      ++ nbatch;      

      double now = get_wall_time();
      if (now > last_report_time + arg.reportfreq.getValue()) {
        float dur = now - last_report_time;
        last_report_time = now;

        int processed_batch = nbatch - last_report_batch;
        int processed_frame = processed_batch * arg.batchsize.getValue();
        int total_frame = nbatch * arg.batchsize.getValue();
        std::cerr << " -- " << std::endl;
        std::cerr << "            Frames ingested (total) = " << total_frame << std::endl;
        std::cerr << "Frames ingested (from prev. report) = " << processed_frame << std::endl;
        std::cerr << "                         Loss/frame = " << loss_since_last_report / processed_frame << std::endl;
        std::cerr << "                                FPS = " << static_cast<float>(processed_frame) / dur << std::endl;

        loss_since_last_report = 0.0;
        last_report_batch = nbatch;
      }
      delta.after_update(config);

      cache.next();
    }

    INFO("Finished, writing output...");
    variant_t output_src;
    nnet.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinNnet");

#ifdef ENABLE_NNET_PROFILE
    for (int loc = 0; loc < nnet.nnodes(); ++ loc) {
      std::cout << "      Node Name: " << nnet.node(loc)->name() << std::endl
                << " Prepare forward: " << nnet.node(loc)->time_prepare_forward << std::endl
                << " Perform forward: " << nnet.node(loc)->time_perform_forward << std::endl
                << "Prepare backward: " << nnet.node(loc)->time_prepare_backward << std::endl
                << "Perform backward: " << nnet.node(loc)->time_perform_backward << std::endl
                << "      Accumulate: " << nnet.node(loc)->time_accumulate << std::endl
                << "          Update: " << nnet.node(loc)->time_update << std::endl;

    }
#endif

    INFO("FIN");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet initializer",
                          argc, argv, spin::tool_main);
}


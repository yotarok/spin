#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/corpus/corpus.hpp>
#include <spin/corpus/yaml.hpp>
#include <spin/corpus/msgpack.hpp>
#include <spin/io/variant.hpp>

#include <gear/flow/preset.hpp>
#include <gear/utils.hpp>

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, inputtag,
                   ("I", "inputtag", "", true, "", "TAG")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, outputtag,
                   ("O", "outputtag", "", true, "", "TAG")),
                  (TCLAP::ValueArg<std::string>, flow, 
                   ("f", "flow", "rspec", false, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, preset,
                   ("p", "preset", "preset name", false, "", "NAME")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    gear::flow_ptr flow;
    if (arg.flow.isSet()) {
      flow = gear::parse_flow(arg.flow.getValue());
    }
    else if (arg.preset.isSet()) {
      flow = gear::eval_preset_name(arg.preset.getValue());
    }
    else {
      throw std::runtime_error("Set flow yaml or preset name");
    }

    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());
    // TO DO: Currently MIMO flow is not supported

    auto src = new gear::vector_source_node<float>("source", "");
    auto sink = new gear::vector_sink_node<float>("sink", "");
    flow->add(src);
    flow->add(sink);
    flow->connect("source", "", "_input", "");
    flow->connect("_output", "", "sink", "");

    for ( ; ! cit->done() ; cit->next()) {
      INFO("Processing %s", cit->get_key().c_str());
      corpus_entry input = cit->value();
      corpus_entry output;
      copy_sticky_tags(&output, input);

      fmatrix inp;
      //= boost::get<fmatrix>(input[arg.inputtag.getValue()]);
      read_fmatrix(&inp, input[arg.inputtag.getValue()]);
      src->set_meta_data(inp.rows(), 0, "");

      src->append_data(inp);
      
      src->feed_BOS();
      src->feed_data_all();
      src->feed_EOS();

      output[arg.outputtag.getValue()] = sink->get_output();

      
      writer->write(output);
    }
    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Copy corpus (mainly intended for converting "
                         "corpus to/from binary format)",
                         argc, argv, spin::tool_main);
}


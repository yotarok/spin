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

#include <boost/random.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::UnlabeledMultiArg<std::string>, names,
                   ("names", "Node names to be deleted", true, "names.."))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t input_src;
    load_variant(&input_src, arg.input.getValue());
    nnet nn(input_src);

    // TO DO: So far, there's no sanity check of specified names
    //        Maybe, it should be verified that these are on the edge of NN

    for (auto it = arg.names.getValue().cbegin(),
           last = arg.names.getValue().cend(); it != last; ++ it) {
      INFO("Deleting %s...", it->c_str());
      nn.remove_node(*it);
    }

    INFO("Deletion completed, writing...");
    variant_t output_src;
    nn.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinNnet");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet remove node",
                          argc, argv, spin::tool_main);
}


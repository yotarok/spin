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
                  (TCLAP::ValueArg<std::string>, name,
                   ("n", "name", "", true, "", "NAME")),
                  (TCLAP::ValueArg<std::string>, type,
                   ("t", "type", "", true, "", "NAME")),
                  (TCLAP::MultiArg<std::string>, prevs,
                   ("p", "prev", "", false, "NAME")),
                  (TCLAP::MultiArg<std::string>, initparams,
                   ("c", "init", "", false, "KEY=VALUE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t input_src;
    load_variant(&input_src, arg.input.getValue());
    nnet nn(input_src);

    std::map<std::string, std::string> initparams;
    for (int n = 0; n < arg.initparams.getValue().size(); ++ n) {
      const std::string s = arg.initparams.getValue()[n];
      size_t eq = s.find('=');
      if (eq == std::string::npos) {
        throw std::runtime_error("initialization parameter must have a form"
                                 " as NAME=VALUE");
      }
      std::string k = s.substr(0, eq), v = s.substr(eq + 1);
      initparams.insert(std::make_pair(k, v));
    }
    
    nnet_node_ptr node = nnet_node::create(arg.type.getValue(),
                                           arg.name.getValue());
    node->initialize(initparams);

    nn.add_node(node, arg.prevs.getValue());

    variant_t output_src;
    nn.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinNnet");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet add ident",
                          argc, argv, spin::tool_main);
}


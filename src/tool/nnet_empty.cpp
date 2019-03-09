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
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    nnet empty;
    variant_t output_src;
    empty.write(&output_src);
    write_variant(output_src, arg.output.getValue(),
                  arg.write_text.isSet(), "SpinNnet");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet initializer",
                          argc, argv, spin::tool_main);
}


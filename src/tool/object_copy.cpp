#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/corpus/corpus.hpp>
#include <spin/io/variant.hpp>
#include <spin/fst/linear.hpp>
#include <spin/fscorer/diaggmm.hpp>
#include <spin/hmm/treestat.hpp>
#include <fst/script/topsort.h>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );
  

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t src;
    load_variant(&src, arg.input.getValue());
    write_variant(src, arg.output.getValue(),
                  arg.write_text.isSet(), "");

    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Copy variant object",
                          argc, argv, spin::tool_main);
}

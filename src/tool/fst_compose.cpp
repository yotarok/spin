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
#include <spin/fst/text_compose.hpp>
#include <fst/script/connect.h>

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, left, 
                   ("l", "left", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, right, 
                   ("r", "right", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, noconnect,
                   ("", "noconnect", "")),
                  (TCLAP::SwitchArg, lookahead,
                   ("", "lookahead", "")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    vector_fst_ptr left = read_fst_file(arg.left.getValue());
    vector_fst_ptr right = read_fst_file(arg.right.getValue());
    vector_fst output(fst_textual_compose(*left, *right, arg.lookahead.isSet()));
    if (! arg.noconnect.isSet())
      fst::script::Connect(&output);
    
    if (arg.write_text.getValue()) {
      std::ofstream ofs(arg.output.getValue());
      print_fst(ofs, output);
    } else {
      std::ofstream ofs(arg.output.getValue());
      fst::FstWriteOptions opts;
      output.Write(ofs, opts);
    }
                      
    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Compose FST to corpus object",
                          argc, argv, spin::tool_main);
}


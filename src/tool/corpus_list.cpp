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

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "TEXT")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    std::ofstream ofs(arg.output.getValue().c_str());
    for ( ; ! cit->done() ; cit->next()) {
      ofs << cit->get_key() << std::endl;
    }
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("List keys in corpus (mainly intended for shuffling/ "
                          "head-tail extraction)",
                          argc, argv, spin::tool_main);
}


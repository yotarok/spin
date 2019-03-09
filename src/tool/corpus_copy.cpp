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
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());
    for ( ; ! cit->done() ; cit->next()) {
      INFO("Processing %s", cit->get_key().c_str());
      corpus_entry ent = cit->value();
      writer->write(ent);
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


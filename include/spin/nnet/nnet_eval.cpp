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

#include <boost/random.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::MultiArg<std::string>, specs, 
                   ("S", "streamspec", "", true, "TYPE:CORPUS:TAG:COMP:FLOWFILE")),
                  (TCLAP::ValueArg<std::string>, input,
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    corpus_iterator_ptr ait = make_corpus_iterator(arg.alignment.getValue());
    corpus_iterator_ptr fit = make_corpus_iterator(arg.features.getValue());
    zipped_corpus_iterator_ptr zit = zip_corpus("alignment", ait,
                                                "features", fit);
    zit->import_key(1, "feature", "feature");
    zit->import_key(0, "state", "state");

    for (; ! zit.done(); zit.next()) {
      
    }

    INFO("FIN");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("nnet initializer",
                          argc, argv, spin::tool_main);
}


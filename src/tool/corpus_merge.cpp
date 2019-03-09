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
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", "")),
                  (TCLAP::UnlabeledMultiArg<std::string>, input,
                   ("inputs", "Input corpora", true, "corpus.."))
                  );

  bool all_done(std::vector<corpus_iterator_ptr>& cits) {
    bool ret = true;
    for (auto cit : cits) {
      if (! cit->done()) {
        ret = false;
        break;
      }
    }
    return ret;
  }

  void extract_first_key(std::vector<corpus_iterator_ptr>& cits,
                         std::string* dest) {
    bool is_first = true;
    for (auto cit : cits) {
      if (cit->done()) continue;
      if (is_first) {
        *dest = cit->get_key();
        is_first = false;
        continue;
      }
      std::string k = cit->get_key();
      if (*dest < k) *dest = k;
    }
  }

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());
    
    std::vector<corpus_iterator_ptr> cits;
    for (int n = 0; n < arg.input.getValue().size(); ++ n) {
      corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue()[n]);
      cits.push_back(cit);
    }

    while (! all_done(cits)) {
      std::string curkey;
      extract_first_key(cits, &curkey);
      INFO("Writing %s", curkey.c_str());

      corpus_entry ent;
      for (auto cit : cits) {
        if (cit->done()) continue;
        if (cit->get_key() != curkey) continue;
        const corpus_entry& src = cit->value();
        for (const auto& pair : src) {
          if (ent.find(pair.first) != ent.end()) continue;

          ent.insert(pair);
        }
        cit->next();
      }
      writer->write(ent);

    }
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Merge corpus files",
                          argc, argv, spin::tool_main);
}


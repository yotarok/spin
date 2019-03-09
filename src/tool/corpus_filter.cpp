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
                  (TCLAP::ValueArg<std::string>, filter, 
                   ("f", "filter", "", true, "", "TEXT")),
                  (TCLAP::ValueArg<std::string>, source, 
                   ("s", "source", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.source.getValue());
    std::map<std::string, corpus_pos_t> pos;
    for ( ; ! cit->done() ; cit->next()) {
      pos[cit->get_key()] = cit->pos();
      std::cout << cit->get_key() << " => " << cit->pos() << std::endl;
    }

    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());

    std::ifstream ifs(arg.filter.getValue().c_str());
    std::string line;
    while(std::getline(ifs, line)) {
      boost::algorithm::trim(line);
      if (line.size() == 0) continue;
      std::vector<std::string> fields;
      boost::algorithm::split(fields, line, boost::is_any_of(" \t"));
      if (fields.size() == 1) {
        fields.push_back(fields[0]);
      }

      auto posit = pos.find(fields[1]);
      if (posit == pos.end()) {
        WARN("%s is not found in the source", fields[1].c_str());
        continue;
      }

      corpus_iterator_ptr seekit = make_corpus_iterator(arg.source.getValue(),
                                                        posit->second);
      corpus_entry ent = seekit->value();
      ent["+key"] = fields[0];
      writer->write(ent);
      
    }
    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Re-order corpus according to given key file",
                          argc, argv, spin::tool_main);
}


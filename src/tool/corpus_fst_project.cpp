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
#include <fst/script/project.h>
#include <fst/script/rmepsilon.h>
#include <fst/script/map.h>

namespace spin {
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, inputtag,
                   ("I", "inputtag", "", true, "", "TAG")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, outputtag,
                   ("O", "outputtag", "", true, "", "TAG")),
                  (TCLAP::SwitchArg, proj_out,
                   ("", "proj-out", "")),
                  (TCLAP::SwitchArg, to_graph,
                   ("", "to-graph",
                    "Convert lattice/alignment to word/phone graph")),
                  (TCLAP::SwitchArg, rmeps,
                   ("", "rmeps", "")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());

    for ( ; ! cit->done() ; cit->next()) {
      INFO("Processing %s", cit->get_key().c_str());
      corpus_entry input = cit->value();
      corpus_entry output;
      copy_sticky_tags(&output, input);

      vector_fst inputfst = 
        boost::get<vector_fst>(input[arg.inputtag.getValue()]);
      if (arg.to_graph.isSet()) {
        inputfst = vector_fst(*fst::script::Map(inputfst,
                                                fst::script::TO_STD_MAPPER,
                                                0.0,
                                                fst::script::WeightClass()));
      }

      fst::script::Project(&inputfst,
                           (arg.proj_out.isSet() ?
                            fst::PROJECT_OUTPUT : fst::PROJECT_INPUT));

      if (arg.rmeps.isSet()) {
        fst::script::RmEpsilon(&inputfst, true);
      }

      output[arg.outputtag.getValue()] = inputfst;
      writer->write(output);
    }

    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Compose FST to corpus object",
                          argc, argv, spin::tool_main);
}


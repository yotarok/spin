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
#include <fst/script/convert.h>
#include <spin/fst/linear.hpp>

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
                  (TCLAP::ValueArg<std::string>, fst,
                   ("f", "fst", "", true, "", "FILE")),
                  (TCLAP::SwitchArg, rightcompose,
                   ("r", "rightcompose", "")),
                  (TCLAP::SwitchArg, noconnect,
                   ("", "noconnect", "")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());
    corpus_writer_ptr writer = make_corpus_writer(arg.output.getValue(),
                                                  ! arg.write_text.getValue());

    std::ifstream ifs(arg.fst.getValue());
    vector_fst_ptr pfst;
    if (fst::IsFstHeader(ifs, arg.fst.getValue())) {
      INFO("Loading binary FST");
      fst::script::FstClass* p = 
        fst::script::FstClass::Read(ifs, arg.fst.getValue());
      pfst.reset(new vector_fst(*p));
      delete p; // is it really okay?
    } else {
      INFO("Loading text FST");
      pfst.reset(new vector_fst(make_fst(ifs)));
    }

    for ( ; ! cit->done() ; cit->next()) {
      INFO("Processing %s", cit->get_key().c_str());
      corpus_entry input = cit->value();
      corpus_entry output;
      copy_sticky_tags(&output, input);

      vector_fst inputfst = 
        boost::get<vector_fst>(input[arg.inputtag.getValue()]);

      if (inputfst.ArcType() == "lattice" && pfst->ArcType() == "standard") {
        INFO("Attempting to compose standard FST to lattice FST");
        INFO("upconvert standard FST to lattice");
        fst::VectorFst<LatticeArc>* pconvfst = new fst::VectorFst<LatticeArc>;
        fst::ArcMap(*pfst->GetFst<fst::StdArc>(), pconvfst,
                    fst::WeightConvertMapper<fst::StdArc, LatticeArc>());
        pfst.reset(new vector_fst(*pconvfst));
        pconvfst->Write("testfst.bin");
      }

      vector_fst outputfst =
        (arg.rightcompose.isSet())
        ? vector_fst(fst_textual_compose(inputfst, *pfst))
        : vector_fst(fst_textual_compose(*pfst, inputfst));

      if (! arg.noconnect.isSet())
        fst::script::Connect(&outputfst);
      output[arg.outputtag.getValue()] = outputfst;
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


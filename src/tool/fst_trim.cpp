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
#include <fst/script/relabel.h>

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
    vector_fst_ptr input = read_fst_file(arg.input.getValue());

    const fst::SymbolTable* pisymtab = input->InputSymbols();
    fst::SymbolTable newisymtab;
    newisymtab.AddSymbol("<eps>", 0);
    
    std::vector<std::pair<int64, int64> > empty;
    std::vector<std::pair<int64, int64> > pairs;

    for (fst::SymbolTableIterator symit(*pisymtab);
         ! symit.Done(); symit.Next()) {
      std::string sym = symit.Symbol();
      int64 lab = symit.Value();
      if (lab == 0 || sym[0] == '#') {
        pairs.push_back(std::make_pair(lab, 0));
      } else if (sym[0] == 'S') {
        std::string nsym = sym.substr(0, sym.find(';'));
        int64 nlab = prepare_label(&newisymtab, nsym);
        pairs.push_back(std::make_pair(lab, nlab));
      } else {
        WARN("Uninterpretable symbol: %s", sym.c_str());
        pairs.push_back(std::make_pair(lab, lab));
      }
    }

    fst::script::Relabel(&(*input), pairs, empty);

    if (arg.write_text.getValue()) {
      std::ofstream ofs(arg.output.getValue());
      print_fst(ofs, *input);
    } else {
      std::ofstream ofs(arg.output.getValue());
      fst::FstWriteOptions opts;
      input->Write(ofs, opts);
    }

    INFO("DONE");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Compose FST to corpus object",
                          argc, argv, spin::tool_main);
}

  

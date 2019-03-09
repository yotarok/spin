#include <spin/types.hpp>
#include <spin/io/fst.hpp>
#include <fst/fst.h>
#include <fst/vector-fst.h>
#include <fst/const-fst.h>
#include <fst/script/fstscript.h>

#include <gear/io/logging.hpp>

namespace spin {
  fst::script::VectorFstClass make_fst(std::istream& is) {
    // TO DO: This should be modified to use fst::script::...
    // However, it's not straight forward to use this so far due to current 
    // limitation of OpenFST that doesn't support add_symbols in 
    // script::CompileFst

    std::string header;
    std::string type;
    int64 oldpos = is.tellg();
    std::getline(is, header);
    if (header[0] == '#') {
      std::vector<std::string> vals;
      boost::algorithm::split(vals, header, boost::is_any_of(" \t"), 
                              boost::token_compress_on);
      type = vals[1];
    } else {
      throw std::runtime_error("spin doesn't support headerless text FST format");
    }

    if (type == "lattice") {
      fst::VectorFst<LatticeArc> fst;
      load_fst(&fst, is);
      fst::script::VectorFstClass ret(fst);
      return ret;
    }
    else if (type == "standard") {
      fst::VectorFst<fst::StdArc> fst;
      load_fst(&fst, is);
      return fst::script::VectorFstClass(fst);
    }
    throw std::runtime_error("unknown fst type [" + type + "]");
  }


 void print_fst(std::ostream& os, const fst::script::FstClass& fst) {
    std::string type = fst.ArcType();
    os << "#FSTHeader " << type << std::endl;
    // TO DO: This should be modified to use fst::script::...
    if (type == "lattice") {
      const fst::Fst<LatticeArc>* pfst = fst.GetFst<LatticeArc>();
      print_fst(os, *pfst);
      return;
    }
    else if (type == "standard") {
      const fst::Fst<fst::StdArc>* pfst = fst.GetFst<fst::StdArc>();
      print_fst(os, *pfst);
      return;
    }
    throw std::runtime_error("unknown fst type [print_fst:" + type + "]");
  }

  int prepare_label(fst::SymbolTable* psymtab, const std::string& sym) {
    if (sym.size() == 0) {
      ERROR("Empty FST symbol is not allowed");
      throw std::runtime_error("Empty FST symbol is not allowed");
    }
    int ret = psymtab->Find(sym);
    if (ret == fst::SymbolTable::kNoSymbol) {
      ret = psymtab->AddSymbol(sym);
    }
    return ret;
  }

  fst::script::VectorFstClass read_fst(std::istream& is) {
    fst::script::FstClass* p =
      fst::script::FstClass::Read(is, "<unspecified>");
    fst::script::VectorFstClass ret(*p);
    delete p; // is it okay?
    return ret;
  }

  vector_fst_ptr read_fst_file(const std::string& path) {
    std::ifstream ifs(path);
    vector_fst_ptr pfst;
    if (fst::IsFstHeader(ifs, path)) {
      fst::script::FstClass* p = 
        fst::script::FstClass::Read(ifs, path);
      pfst.reset(new vector_fst(*p));
      delete p; // is it really okay?
    } else {
      pfst.reset(new vector_fst(make_fst(ifs)));
    }
    return pfst;
  }

}

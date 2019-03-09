#include <spin/fst/text_compose.hpp>
#include <gear/io/logging.hpp>


#include <fst/script/relabel.h>
#include <fst/script/arcsort.h>
#include <fst/script/compose.h>
#include <fst/script/convert.h>
#include <fst/script/relabel.h>

#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


namespace spin {
  fst::script::FstClass
  fst_textual_compose(fst::script::FstClass left,
                      fst::script::FstClass right,
                      bool lookahead) {
    if (left.ArcType() != right.ArcType()) {
      throw std::runtime_error("compostition failed: arc type mismatch");
    }
    if (left.OutputSymbols() == 0 && right.InputSymbols() == 0) {
      // No symbol tables, do numeric match
      fst::script::VectorFstClass ret(left.ArcType());
      fst::script::Compose(left, right, &ret, fst::AUTO_FILTER);
      return ret;
    }
    if (left.OutputSymbols()->LabeledCheckSum()
        == right.InputSymbols()->LabeledCheckSum()) {
      // Equivalent symbol tables, do numeric match
      fst::script::VectorFstClass ret(left.ArcType());
      fst::script::Compose(left, right, &ret, fst::AUTO_FILTER);
      return ret;
    }
    
    if (left.OutputSymbols() == 0) {
      fst::script::VectorFstClass ret(left.ArcType());
      fst::script::VectorFstClass vright(right);
      vright.SetInputSymbols(0);

      fst::script::Compose(left, vright, &ret, fst::AUTO_FILTER);
      return ret;
    } else if (right.InputSymbols() == 0) {
      fst::script::VectorFstClass ret(left.ArcType());
      fst::script::VectorFstClass vleft(left);
      vleft.SetOutputSymbols(0);

      fst::script::Compose(vleft, right, &ret, fst::AUTO_FILTER);
      return ret;
    }

    fst::script::VectorFstClass vleft(left);
    fst::script::VectorFstClass vright(right);

    std::vector<std::pair<int64, int64> > ipairs; // keep empty
    std::vector<std::pair<int64, int64> > opairs;
    fst::SymbolTable target = *right.InputSymbols();
    int64 null = target.AddSymbol("/dev/null");
    for (fst::SymbolTableIterator stit(*vleft.OutputSymbols()); ! stit.Done();
         stit.Next()) {
      int64 target_lab = right.InputSymbols()->Find(stit.Symbol());
      if (target_lab == fst::SymbolTable::kNoSymbol) {
        opairs.push_back(std::make_pair(stit.Value(), null));
      } else {
        opairs.push_back(std::make_pair(stit.Value(), target_lab));
      }
    }

    // Relabel(MutableFstClass, const SymbolTable, ...) doesn't allow missing label
    
    fst::script::Relabel(&vleft, ipairs, opairs);
    vleft.SetOutputSymbols(0);
    vright.SetInputSymbols(0);
    fst::script::VectorFstClass ret(left.ArcType());

    // if lookahead
    if (lookahead) {

      INFO("Converting to lookahead FST...");
      // So far, so hacky
      ::FLAGS_save_relabel_opairs = std::tmpnam(0);
      fst::script::FstClass laleft = *fst::script::Convert(vleft, "olabel_lookahead");
      
      INFO("Converted to lookahead FST. [relabel info = %s]",
           ::FLAGS_save_relabel_opairs.c_str());

      std::vector<std::pair<int64, int64> > ipairs, opairs;
      std::ifstream ifs(::FLAGS_save_relabel_opairs.c_str());
      while (1) {
        std::string line;
        std::getline(ifs, line);
        boost::trim(line);
        std::vector<std::string> vpair;
        boost::split(vpair, line, boost::is_any_of(" \t"), boost::token_compress_on);
        if (vpair.size() == 0 || vpair[0].size() == 0) {
          break;
        } else if (vpair.size() != 2) {
          INFO("Error: # column is not 2, %d. %s", vpair.size(), line.c_str());
          throw std::runtime_error("relabeler format error: # column is not 2");
        }
        std::pair<int64, int64> p;
        try {
          p = std::make_pair(boost::lexical_cast<int64>(vpair[0]),
                             boost::lexical_cast<int64>(vpair[1]));
        } catch (...) {
          throw std::runtime_error("relabeler format error: Numeral parsing error");
        }
        ipairs.push_back(p);
      }
      ::unlink(::FLAGS_save_relabel_opairs.c_str());
      INFO("Read relabeling information...");

      fst::script::Relabel(&vright, ipairs, opairs);
      INFO("Relabeled, start sorting...");
      fst::script::ArcSort(&vright, fst::script::ILABEL_SORT);
      INFO("Sorted, start composition...");
     
      fst::script::Compose(laleft, vright, &ret, fst::AUTO_FILTER);

      fst::SymbolTable nisymtab(*vleft.InputSymbols());
      ret.SetInputSymbols(&nisymtab);
    } else {      
      fst::script::ArcSort(&vleft, fst::script::OLABEL_SORT);
      fst::script::ArcSort(&vright, fst::script::ILABEL_SORT);

      fst::script::Compose(vleft, vright, &ret, fst::AUTO_FILTER);
    }
    return ret;
  }

    
}

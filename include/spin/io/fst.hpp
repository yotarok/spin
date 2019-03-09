#ifndef spin_io_fst_hpp_
#define spin_io_fst_hpp_

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <fst/script/fst-class.h>
#include <spin/fstext/latticearc.hpp>

namespace spin {

  template <typename Weight>
  inline Weight read_weight(const std::string& s) {
    std::istringstream iss(s);
    Weight w;
    iss >> w;
    return w;
  }

  template <typename Fst>
  void prepare_state(Fst* pfst, int s) {
    while (pfst->NumStates() <= s) {
      //std::cout << "ADDSTATE" << std::endl;
      pfst->AddState();
    }
  }

  int prepare_label(fst::SymbolTable* psymtab, const std::string& sym);

  template <typename Fst>
  void print_fst_state(std::ostream& os, const Fst& fst, 
                       typename Fst::StateId sid, bool is_init = false) {
    typedef typename Fst::Arc Arc;
    typedef typename Arc::Weight Weight;
    int output = false;
    for (fst::ArcIterator<Fst> ait(fst, sid); ! ait.Done(); ait.Next()) {
      const Arc& a = ait.Value();
      std::string isym = fst.InputSymbols()->Find(a.ilabel);
      std::string osym = fst.OutputSymbols()->Find(a.olabel);
      os << sid << '\t' << a.nextstate << '\t'
         << isym << '\t' << osym << '\t' << a.weight << std::endl;
      output = true;
    }
    Weight fin = fst.Final(sid);
    if (fin != Weight::Zero() || (is_init && ! output)) {
      os << sid << '\t' << fin << std::endl;
    }
  }

  template <typename Arc>
  void print_fst(std::ostream& os, const fst::Fst<Arc>& fst) {
    //typedef typename Fst::Arc Arc;
    typedef fst::Fst<Arc> Fst;
    typedef typename Fst::StateId StateId;
    typedef typename Arc::Weight Weight;

    StateId start = fst.Start();
    if (start < 0) {
      throw std::runtime_error("initial state is not specified");
    }
    print_fst_state(os, fst, start, true);
    for (fst::StateIterator<Fst> stit(fst); ! stit.Done(); stit.Next()) {
      StateId s = stit.Value();
      if (s == start) continue;
      print_fst_state(os, fst, s);
    }
  }

  void print_fst(std::ostream& os, const fst::script::FstClass& fst);

  template <typename Fst>
  void load_fst(Fst* pfst, std::istream& is) { 
    typedef typename Fst::Arc Arc;
    typedef typename Arc::Weight Weight;

    // attach empty symbol tables
    fst::SymbolTable isymtab;
    fst::SymbolTable osymtab;
    isymtab.AddSymbol("<eps>", 0);
    osymtab.AddSymbol("<eps>", 0);

    std::string line;
    int initst = -1;
    while (std::getline(is, line)) {
      //std::cout << "LINE: " << line << std::endl;
      std::vector<std::string> vs;
      boost::split(vs, line, boost::is_any_of(" \t"), boost::token_compress_on);
      if (vs.size() == 0) {
        continue;
      }
      else if (vs.size() < 3) {
        int s = boost::lexical_cast<int>(vs[0]);
        prepare_state(pfst, s);
        if (initst < 0) initst = s;
        pfst->SetFinal(s, (vs.size()==1) ? 
                       Weight::One() : read_weight<Weight>(vs[1]));
      }
      else if (vs.size() < 6) {
        int p = boost::lexical_cast<int>(vs[0]);
        int n = boost::lexical_cast<int>(vs[1]);
        if (initst < 0) initst = p;
        prepare_state(pfst, p);
        prepare_state(pfst, n);
        std::string isym = vs[2];
        std::string osym = (vs.size() >= 4) ? vs[3] : isym;

        Arc arc;
        arc.nextstate = n;
        arc.ilabel = prepare_label(&isymtab, isym);
        arc.olabel = prepare_label(&osymtab, osym);
        arc.weight = (vs.size() >= 5) ? 
          read_weight<Weight>(vs[4]) : Weight::One();

        pfst->AddArc(p, arc);
      }
      else {
        throw std::runtime_error("invalid format");
      }
    }
    pfst->SetStart(initst);
    pfst->SetInputSymbols(&isymtab);
    pfst->SetOutputSymbols(&osymtab);
  }

  fst::script::VectorFstClass make_fst(std::istream& is);

  fst::script::VectorFstClass read_fst(std::istream& is);

  vector_fst_ptr read_fst_file(const std::string& path);
}
#endif

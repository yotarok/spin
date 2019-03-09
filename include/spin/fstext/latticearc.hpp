#ifndef spin_fstext_latticearc_hpp_
#define spin_fstext_latticearc_hpp_

#include <fst/float-weight.h>
#include <fst/product-weight.h>
#include <spin/fstext/latticeweight.hpp>
#include <fst/vector-fst.h>

namespace spin {
  struct LatticeArc {
    typedef LatticeWeight Weight;
    typedef int Label;
    typedef int StateId;

    LatticeArc(Label i, Label o, const Weight& w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}
    
    LatticeArc() {}

    static const std::string& Type() {
      static const std::string TYPE("lattice");
      return TYPE;
    }
    
    Label ilabel;
    Label olabel;
    Weight weight;
    StateId nextstate;   
  };

  typedef fst::VectorFst<LatticeArc> Lattice;

}

#endif

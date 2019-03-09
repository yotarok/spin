#ifndef spin_fst_linear_hpp_
#define spin_fst_linear_hpp_

#include <fst/script/relabel.h>
#include <fst/script/compose.h>

namespace spin {
  template <typename Arc>
  bool CheckLinearity(const fst::Fst<Arc>& fst) {
    typedef fst::Fst<Arc> Fst;
    typedef typename Fst::Weight Weight;
    typedef typename Fst::StateId StateId;
    //typedef typename Fst::Arc Arc;
    StateId st = fst.Start();
    bool linear = true;
    while (linear) {
      int narc = 0;
      StateId next;
      for (fst::ArcIterator<Fst> ait(fst, st); ! ait.Done();
           ait.Next(), ++ narc) {
        next = ait.Value().nextstate;
      }
      if (narc > 1) linear = false;
      bool is_final = fst.Final(st) != Weight::Zero();
      if (narc == 1 && is_final) linear = false;
      if (narc == 0 && is_final) break;
      if (narc == 0 && ! is_final) linear = false;
      if (st == next) linear = false; // self-loop
      st = next;
    }
    return linear;
  }
  
  typedef
  fst::script::args::WithReturnValue<bool,const fst::script::FstClass&>
  CheckLinearityArgs;

  template <class Arc>
  void CheckLinearity(CheckLinearityArgs* args) {
    const fst::Fst<Arc> &fst = *(args->args.GetFst<Arc>());
    args->retval = CheckLinearity(fst);
  }
 
  bool CheckLinearity(const fst::script::FstClass& fst);

  
  template <typename Arc>
  void ExtractString(const fst::Fst<Arc>& fst, bool ilabel, std::vector<int>* dest) {
    typedef typename fst::Fst<Arc> Fst;
    typedef typename Fst::Weight Weight;
    typedef typename Fst::StateId StateId;
    //typedef typename Fst::Arc Arc;
    
    StateId st = fst.Start();
    bool reach_final = false;
    while (! reach_final) {
      fst::ArcIterator<Fst> ait(fst, st);
      if (ait.Done()) {
        throw std::runtime_error("FST is not linear");
      }
      Arc arc = ait.Value();
      if (ilabel)
        dest->push_back(arc.ilabel);
      else
        dest->push_back(arc.olabel);
      
      st = arc.nextstate;
      reach_final = fst.Final(st) != Weight::Zero();
    }
  }

  template <typename Arc>
  void ExtractWeights(const fst::Fst<Arc>& fst, 
                      std::vector<typename Arc::Weight>* dest) {
    typedef typename fst::Fst<Arc> Fst;
    typedef typename Fst::Weight Weight;
    typedef typename Fst::StateId StateId;
    
    StateId st = fst.Start();
    bool reach_final = false;
    while (! reach_final) {
      fst::ArcIterator<Fst> ait(fst, st);
      if (ait.Done()) {
        throw std::runtime_error("FST is not linear");
      }
      Arc arc = ait.Value();
      dest->push_back(arc.weight);
      st = arc.nextstate;
      reach_final = fst.Final(st) != Weight::Zero();
    }
  }

 
  typedef
  fst::script::args::Package<const fst::script::FstClass&,
                             bool, std::vector<int>*>
  ExtractStringArgs;

  template <class Arc>
  void ExtractString(ExtractStringArgs* args) {
    const fst::Fst<Arc> &fst = *(args->arg1.GetFst<Arc>());
    ExtractString(fst, args->arg2, args->arg3);
  }

  void ExtractString(const fst::script::FstClass& fst, bool ilabel,
                     std::vector<int>* dest);

}

#endif

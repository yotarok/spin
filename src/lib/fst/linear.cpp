#include <fst/script/fst-class.h>
#include <fst/script/script-impl.h>

#include <spin/fst/linear.hpp>
#include <spin/fstext/latticearc.hpp>

namespace spin {
  using namespace fst;
  using namespace fst::script;
  
  bool CheckLinearity(const fst::script::FstClass &ifst) {
    CheckLinearityArgs args(ifst);
    
    Apply<Operation<CheckLinearityArgs> >("CheckLinearity", ifst.ArcType(),
                                          &args);
    
    return args.retval;
  }

  void ExtractString(const fst::script::FstClass& fst, bool ilabel,
                     std::vector<int>* dest) {
    ExtractStringArgs args(fst, ilabel, dest);
    
    Apply<Operation<ExtractStringArgs> >("ExtractString", fst.ArcType(),
                                         &args);    
  }

  
  REGISTER_FST_OPERATION(CheckLinearity, StdArc, CheckLinearityArgs);
  REGISTER_FST_OPERATION(CheckLinearity, LogArc, CheckLinearityArgs);
  REGISTER_FST_OPERATION(CheckLinearity, Log64Arc, CheckLinearityArgs);
  REGISTER_FST_OPERATION(CheckLinearity, LatticeArc, CheckLinearityArgs);

  REGISTER_FST_OPERATION(ExtractString, StdArc, ExtractStringArgs);
  REGISTER_FST_OPERATION(ExtractString, LogArc, ExtractStringArgs);
  REGISTER_FST_OPERATION(ExtractString, Log64Arc, ExtractStringArgs);
  REGISTER_FST_OPERATION(ExtractString, LatticeArc, ExtractStringArgs);

  
}

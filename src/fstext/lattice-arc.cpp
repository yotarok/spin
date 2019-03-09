#include <fst/fst.h>
#include <fst/vector-fst.h>
#include <fst/const-fst.h>
#include <fst/script/fstscript.h>
#include <fst/script/register.h>

#include <spin/fstext/latticearc.hpp>

using namespace fst;
using namespace fst::script;
using namespace spin;

/*
REGISTER_FST(VectorFst, LogLatticeArc);
REGISTER_FST(ConstFst, LogLatticeArc);
REGISTER_FST_OPERATIONS(LogLatticeArc); 
*/

REGISTER_FST(VectorFst, LatticeArc);
REGISTER_FST(ConstFst, LatticeArc); 
REGISTER_FST_OPERATIONS(LatticeArc); 

REGISTER_FST_CLASSES(LatticeArc);

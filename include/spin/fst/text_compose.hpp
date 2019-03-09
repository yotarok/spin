#ifndef spin_fst_text_compose_hpp_
#define spin_fst_text_compose_hpp_

#include <spin/types.hpp>

namespace spin {
  fst_t fst_textual_compose(fst_t left, fst_t right, bool lookahead=false);
}

#endif

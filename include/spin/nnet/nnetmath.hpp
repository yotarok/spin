#ifndef spin_nnet_nnetmath_hpp_
#define spin_nnet_nnetmath_hpp_

#include <spin/nnet/nnet.hpp>
#include <spin/nnet/node.hpp>

#define BIG_VALF 10000000.0f

namespace spin {
  
  inline
  viennacl::matrix_slice<nnet_matrix>
  ones(int r, int c) {
    static nnet_matrix oneone = viennacl::scalar_matrix<float>(1, 1, 1.0f);
    return viennacl::project(oneone, viennacl::slice(0, 0, r), viennacl::slice(0, 0, c));
  }
}

#endif

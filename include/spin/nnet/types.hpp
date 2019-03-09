#ifndef spin_nnet_types_hpp_
#define spin_nnet_types_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>

#include <viennacl/scalar.hpp>
#include <viennacl/vector.hpp>
#include <viennacl/matrix.hpp>

namespace spin {
  typedef viennacl::matrix<float, viennacl::column_major> nnet_matrix;
  typedef viennacl::vector<float> nnet_vector;
}

#endif

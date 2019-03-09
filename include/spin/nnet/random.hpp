#ifndef spin_nnet_random_hpp_
#define spin_nnet_random_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/nnet/nnetmath.hpp>

namespace spin {
  class random_number_generator {
    viennacl::ocl::program& _prog;
    viennacl::ocl::kernel& _draw_uniform_kernel;

    viennacl::matrix<uint32_t, viennacl::column_major> _state;
    int _nthreads;
  public:
    random_number_generator(int n = 1024);
    void reset_seed(uint32_t seed);

    void draw_uniform(nnet_submat* pmat);
  };
}
#endif

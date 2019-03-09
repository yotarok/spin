#include <spin/nnet/random.hpp>
#include <boost/random.hpp>

namespace spin {
  static const char * random_source =
    "void update(__global uint* px, __global uint* py, __global uint* pz, __global uint* pw) {"
    "  uint t = *px;"
    "  t ^= t << 11;"
    "  t ^= t >> 8;"
    "  *px = *py; *py = *pz;*pz = *pw;"
    "  *pw ^= *pw >> 19;"
    "  *pw ^= t;"
    "}\n"
    "__kernel void draw_uniform(__global uint* state, uint stride,"
    "                           __global float* dest, uint M, uint N, "
    "                           uint Mstride) {"
    "  int th = get_global_id(0);"
    "  uint nelem = M * N;"
    "  for (int n = 0; n < nelem; n += get_global_size(0)) {"
    "    int row = (n + th) % M;"
    "    int col = (n + th) / M;"
    "    update(state + (stride * 0 + th), state + (stride * 1 + th),"
    "           state + (stride * 2 + th), state + (stride * 3 + th));"
    "    dest[row + col * Mstride] = "
    "      ((float) state[stride * 3 + th]) / 0xFFFFFFFF;\n"
    "  }"
    "}";

  random_number_generator::random_number_generator(int n)  
    : _prog(viennacl::ocl::current_context().add_program(random_source,
                                                         "random")),
      _draw_uniform_kernel(_prog.get_kernel("draw_uniform")),
      _nthreads(n), _state(n, 4) {
    
  }

  void random_number_generator::reset_seed(uint32_t seed) {
    Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic> init_state(_nthreads, 4);
    boost::mt19937 rng(seed);
    for (int n = 0; n < _nthreads; ++ n) {
      for (int b = 0; b < 4; ++ b) {
        init_state(n, b) = rng();
      }
    }

    viennacl::copy(init_state, _state);
  }


  void random_number_generator::draw_uniform(nnet_submat* pmat) {
    _draw_uniform_kernel.local_work_size(0, 32);
    _draw_uniform_kernel.global_work_size(0, _nthreads);

    viennacl::ocl::enqueue(_draw_uniform_kernel(_state,
                                                cl_uint(_state.internal_size1()),
                                                *pmat,
                                                cl_uint(pmat->size1()),
                                                cl_uint(pmat->size2()),
                                                cl_uint(pmat->internal_size1())
                                                )
                           );
  }
}

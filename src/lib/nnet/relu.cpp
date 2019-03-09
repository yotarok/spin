#include <spin/nnet/relu.hpp>

namespace spin {
  static const char * relu_source =
    "__kernel void relu("
    "          __global float * mat,"
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2) {"
    "  for (unsigned int j = get_global_id(1); j < size2; j += get_global_size(1)) {"
    "    for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    "    mat[i + stride * j] = max(0.0f, mat[i + stride * j]); "
    "    } "
    "  } "
    "} \n"
    "__kernel void drelu("
    "          __global float * mat,"
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2,"
    "          __global const float * inp,"
    "          unsigned int i_stride) {"
    "  for (unsigned int j = get_global_id(1); j < size2; j += get_global_size(1)) {"
    "    for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    
    "      mat[i + stride * j] *= (inp[i + i_stride * j] >= 0) ? 1.0f : 0.0f; "
    "    } "
    "  } "
    "} \n";

  nnet_node_relu::nnet_node_relu(const std::string& n)
    : nnet_simple_activation(n),
      _prog(viennacl::ocl::current_context().add_program(relu_source,
                                                         "relu")),
      _relu_kernel(_prog.get_kernel("relu")),
      _drelu_kernel(_prog.get_kernel("drelu")) {
  }

  void nnet_node_relu::feed_forward(const_node_config_ptr pcfg,
                                    node_context_ptr pctx) const {
    for (int n = 0; n < pctx->get_input().nbatch(); ++ n) {
      nnet_submat inp = NNET_BATCH(pctx->get_input(), n);
      int D = inp.size1(), T = inp.size2();
     
      _relu_kernel.local_work_size(0, 32);
      _relu_kernel.global_work_size(0, ((D / 32) + 1) * 32);
      _relu_kernel.local_work_size(1, 1);
      _relu_kernel.global_work_size(1, T);

      pctx->get_output().set_size(n, T);
      nnet_submat out = NNET_BATCH(pctx->get_output(), n);
      out = inp;
      viennacl::ocl::enqueue(_relu_kernel(out,
                                          cl_uint(out.internal_size1()),
                                          cl_uint(out.size1()),
                                          cl_uint(out.size2())));
    }
  }

  void nnet_node_relu::back_propagate(const_node_config_ptr pcfg,
                                      node_context_ptr pctx) const {
    for (int n = 0; n < pctx->get_diff_output().nbatch(); ++ n) {
      nnet_submat dout = NNET_BATCH(pctx->get_diff_output(), n);
      int D = dout.size1(), T = dout.size2();
      _drelu_kernel.local_work_size(0, 32);
      _drelu_kernel.global_work_size(0, ((D / 32) + 1) * 32);
      _drelu_kernel.local_work_size(1, 1);
      _drelu_kernel.global_work_size(1, T);

      pctx->get_diff_input().set_size(n, T);
      nnet_submat dinp = NNET_BATCH(pctx->get_diff_input(), n),
        inp = NNET_BATCH(pctx->get_input(), n);

      dinp = dout;
      
      viennacl::ocl::enqueue(_drelu_kernel(dinp,
                                           cl_uint(dinp.internal_size1()),
                                           cl_uint(dinp.size1()),
                                           cl_uint(dinp.size2()),
                                           inp,
                                           cl_uint(inp.internal_size1())));
    }
  }
  
}

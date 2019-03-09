#include <spin/nnet/sigmoid.hpp>

namespace spin {
  static const char * sigmoid_source =
    "__kernel void sigmoid("
    "          __global float * mat,"
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2) {"
    "  for (unsigned int j = get_global_id(1); j < size2; j += get_global_size(1)) {"
    "    for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    "    mat[i + stride * j] = 1.0f / (1.0f + native_exp(-mat[i + stride * j])); "
    "    } "
    "  } "
    "} \n"
    "__kernel void dsigmoid("
    "          __global float * mat,"
    "          unsigned int stride,"
    "          unsigned int size1, unsigned int size2,"
    "          __global const float * out,"
    "          unsigned int o_stride) {"
    "  for (unsigned int j = get_global_id(1); j < size2; j += get_global_size(1)) {"
    "    for (unsigned int i = get_global_id(0); i < size1; i += get_global_size(0)) {"
    "    float o = out[i + o_stride * j]; "
    "    mat[i + stride * j] *=  o * (1.0f - o); "
    "    } "
    "  } "
    "} \n";

  nnet_node_sigmoid::nnet_node_sigmoid(const std::string& n)
    : nnet_simple_activation(n),
      _prog(viennacl::ocl::current_context().add_program(sigmoid_source,
                                                         "sigmoid")),
      _sigmoid_kernel(_prog.get_kernel("sigmoid")),
      _dsigmoid_kernel(_prog.get_kernel("dsigmoid")) {
    
  }

  void nnet_node_sigmoid::feed_forward(const_node_config_ptr pcfg,
                                       node_context_ptr pctx) const {
    for (int n = 0; n < pctx->get_input().nbatch(); ++ n) {
      nnet_submat inp = NNET_BATCH(pctx->get_input(), n);
      int D = inp.size1(), T = inp.size2();

            
      _sigmoid_kernel.local_work_size(0, 16);
      _sigmoid_kernel.global_work_size(0, ((D / 16) + 1) * 16);
      _sigmoid_kernel.local_work_size(1, 1);
      _sigmoid_kernel.global_work_size(1, T);

      pctx->get_output().set_size(n, T);
      nnet_submat out = NNET_BATCH(pctx->get_output(), n);
      out = inp;
      viennacl::ocl::enqueue(_sigmoid_kernel(out,
                                             cl_uint(out.internal_size1()),
                                             cl_uint(out.size1()),
                                             cl_uint(out.size2())));
    }
  }

  void nnet_node_sigmoid::back_propagate(const_node_config_ptr pcfg,
                                         node_context_ptr pctx) const {
    for (int n = 0; n < pctx->get_diff_output().nbatch(); ++ n) {
      nnet_submat dout = NNET_BATCH(pctx->get_diff_output(), n);
      int D = dout.size1(), T = dout.size2();

      pctx->get_diff_input().set_size(n, T);
      nnet_submat dinp = NNET_BATCH(pctx->get_diff_input(), n), out = NNET_BATCH(pctx->get_output(), n);
      dinp = dout;

      _dsigmoid_kernel.local_work_size(0, 16);
      _dsigmoid_kernel.global_work_size(0, ((D / 16) + 1) * 16);
      _dsigmoid_kernel.local_work_size(1, 1);
      _dsigmoid_kernel.global_work_size(1, T);

      viennacl::ocl::enqueue(_dsigmoid_kernel(dinp,
                                              cl_uint(dinp.internal_size1()),
                                              cl_uint(dinp.size1()),
                                              cl_uint(dinp.size2()),
                                              out,
                                              cl_uint(out.internal_size1())));
    }
  }
  
}

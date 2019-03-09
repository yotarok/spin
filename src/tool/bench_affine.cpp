#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/types.hpp>
#include <spin/utils.hpp>
#include <viennacl/scalar.hpp>
#include <viennacl/vector.hpp>
#include <viennacl/matrix.hpp>

#include <clBLAS.h>

namespace spin {
  typedef viennacl::matrix<float, viennacl::column_major> nnet_matrix;
  typedef viennacl::vector<float> nnet_vector;

  //DEFINE_ARGCLASS(Arg, (gear::common_args),
  //);

  int tool_main(gear::common_args& arg, int argc, char* argv[]) {

    std::vector<viennacl::ocl::device> devices =
      viennacl::ocl::platform().devices();
    viennacl::ocl::setup_context(0, devices[1]);
    std::cout << "# of OCL devices = " << devices.size() << std::endl;
    for (int i = 0; i < devices.size(); ++ i) {
      std::cout << "Device " << i << std::endl
                << devices[i].info() << std::endl
                << std::endl;
    }
    viennacl::ocl::switch_context(0);
    
    std::vector<viennacl::ocl::device> curdevices =
      viennacl::ocl::current_context().devices();
    std::cout << "# of OCL devices in the current context = " << curdevices.size() << std::endl;
    for (int i = 0; i < curdevices.size(); ++ i) {
      std::cout << "Device " << i << std::endl
                << curdevices[i].info() << std::endl
                << std::endl;
    }

    viennacl::ocl::current_context().switch_device(devices[1]);

    
#ifdef DEBUG
    int D1 = 256, D2 = 256;
    int Ts[] = {256, 256, 256, 256}; //{234, 345, 456, 567};
    int Tmax = 256;
#else
    int D1 = 2048, D2 = 2048;
    int Ts[] = {1024, 1024, 1024, 1024}; //{234, 345, 456, 567};
    int Tmax = 1024;
#endif
    fmatrix randw = fmatrix::Random(D1, D2);
    fvector randb = fvector::Random(D1);

    double cpu_time = 0.0;
    int cpu_ntrial = 10;
    int cpu_nframes = 0;
    for (int trial = 0; trial < cpu_ntrial; ++ trial) {
      fmatrix inp = fmatrix::Random(D2, Ts[trial % 4]);
      double stt = get_wall_time();
      fmatrix res = (randw * inp);
      res.colwise() += randb;
      cpu_time += get_wall_time() - stt;
      cpu_nframes += Ts[trial % 4];
    }
    std::cout << "Average FPS (CPU) = " << cpu_nframes / cpu_time << std::endl;

    {
      cl_context ctx = viennacl::ocl::current_context().handle();
      cl_device_id dev = viennacl::ocl::current_device().id();
      cl_int err = clblasSetup();
      cl_command_queue queue = clCreateCommandQueue(ctx, dev, 0, &err);
      cl_event event = NULL;

      viennacl::backend::finish();
      nnet_matrix grandw;
      viennacl::copy(randw, grandw);
      fmatrix randb_mat = randb;
      nnet_matrix grandb;
      viennacl::copy(randb_mat, grandb);
      
      double gpu_time = 0.0;
      int gpu_ntrial = 10;
      int gpu_nframes = 0;
      for (int trial = 0; trial < gpu_ntrial; ++ trial) {
	fmatrix inp = fmatrix::Random(D2, Ts[trial % 4]);
	nnet_matrix ginp;
	viennacl::copy(inp, ginp);
	
	viennacl::backend::finish();
	double stt = get_wall_time();
	//nnet_matrix gres = viennacl::linalg::prod(grandw, ginp);

	nnet_matrix gres(D1, Ts[trial % 4]);
	err =
	  clblasSgemm(clblasColumnMajor, clblasNoTrans, clblasNoTrans,
		      D1, Ts[trial % 4], D2, 
		      1.0, grandw.handle().opencl_handle(), 0, D1,
		      ginp.handle().opencl_handle(), 0, D2, 1.0,
		      gres.handle().opencl_handle(), 0, D1,
		      1, &queue, 0, NULL, &event);

	gres = nnet_matrix(gres.handle().opencl_handle(),
			   gres.size1(), 
			   gres.size2()
			   );

	if (err != 0) {
	  std::cout << "clblas error code = " << err << std::endl;
	  throw std::runtime_error("clblas error");
	}

	
	//viennacl::scalar_matrix<float> ones(1, Ts[trial%4], 1.0f);
	//gres += viennacl::linalg::prod(grandb, nnet_matrix(ones));
	viennacl::backend::finish();

	gpu_time += get_wall_time() - stt;

	fmatrix exp = randw * inp;
	std::cout << exp(1, 1) << std::endl;
	std::cout << "=> " << gres(1, 1) << std::endl;


	gpu_nframes += Ts[trial % 4];
      }
      std::cout << "Average FPS (GPU) = " << gpu_nframes / gpu_time << std::endl;
      viennacl::backend::finish();
    }

    {      
      cl_int err = clblasSetup();
      cl_context ctx = viennacl::ocl::current_context().handle();
      cl_device_id dev = viennacl::ocl::current_device().id();
      cl_command_queue queue = clCreateCommandQueue(ctx, dev, 0, &err);
      cl_mem mem_randw, mem_randb, mem_inp, mem_out;
      cl_event event = NULL;
      double clb_time = 0.0;
      int clb_ntrial = 10;
      int clb_nframes = 0;

      mem_randw = clCreateBuffer(ctx, CL_MEM_READ_ONLY,
				 randw.rows() * randw.cols() * sizeof(cl_float),
				 NULL, &err);
      mem_randb = clCreateBuffer(ctx, CL_MEM_READ_ONLY,
				 randb.size() * sizeof(cl_float),
				 NULL, &err);
      mem_inp = clCreateBuffer(ctx, CL_MEM_READ_ONLY,
			       D2 * Tmax * sizeof(cl_float),
			       NULL, &err);
      mem_out = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
			       D1 * Tmax * sizeof(cl_float),
			       NULL, &err);

      err = clEnqueueWriteBuffer(queue, mem_randw, CL_TRUE, 0,
				 randw.rows() * randw.cols() * sizeof(cl_float),
				 randw.data(), 0, NULL, NULL);

      for (int trial = 0; trial < clb_ntrial; ++ trial) {
	int t = Ts[trial % 4];
	fmatrix out = fmatrix::Zero(D1, Ts[trial % 4]);
	fmatrix inp = fmatrix::Random(D2, Ts[trial % 4]);
	err = clEnqueueWriteBuffer(queue, mem_inp, CL_TRUE, 0,
				   D2 * t * sizeof(cl_float),
				   inp.data(), 0, NULL, NULL);
	err = clEnqueueWriteBuffer(queue, mem_out, CL_TRUE, 0,
				   D1 * t * sizeof(cl_float),
				   out.data(), 0, NULL, NULL);
	clFinish(queue);
	double stt = get_wall_time();
	err = clblasSgemm(clblasColumnMajor, clblasNoTrans, clblasNoTrans,
			  D1, t, D2, 
			  1.0, mem_randw, 0, D1,
			  mem_inp, 0, D2, 1.0,
			  mem_out, 0, D1,
			  1, &queue, 0, NULL, &event);
	if (err != 0) {
	  std::cout << "clblas error code = " << err << std::endl;
	  throw std::runtime_error("clblas error");
	}
	clFinish(queue);

	err = clEnqueueReadBuffer(queue, mem_out, CL_TRUE, 0,
				  D1 * t * sizeof(cl_float),
				  out.data(), 0, NULL, NULL);
	

	clb_time += get_wall_time() - stt;
	clb_nframes += Ts[trial % 4];
      }
      std::cout << "Average FPS (clblas) = " << clb_nframes / clb_time << std::endl;
      
      
    }

    
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Affine transform bench mark",
                          argc, argv, spin::tool_main);
}


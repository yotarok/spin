#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/nnet/random.hpp>

namespace {
  using namespace spin;
  
  TEST(nnet_test, construct) {
    random_number_generator rng(1024);
    rng.reset_seed(0);
    viennacl::matrix<float,viennacl::column_major> drawn(149, 131);
    nnet_submat s = viennacl::project(drawn, viennacl::range(0, 149), viennacl::range(0, 131));
    fmatrix drawn_cpu(149, 131);
    
    rng.draw_uniform(&s);
    viennacl::copy(drawn, drawn_cpu);
    std::cout << "Expectation (should be 0.5) = "
              << drawn_cpu.sum() / drawn_cpu.rows() / drawn_cpu.cols()
              << std::endl;
    rng.draw_uniform(&s);
    viennacl::copy(drawn, drawn_cpu);
    std::cout << "Expectation (should be 0.5) = " 
              << drawn_cpu.sum() / drawn_cpu.rows() / drawn_cpu.cols()
              << std::endl;
  }

}

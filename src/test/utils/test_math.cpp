#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include "../testutil.hpp"

namespace {
  using namespace spin;
  TEST(log_sum_exp_test, single) {
    fvector inp(2);
    inp << 0, -1;
    float ans = log_sum_exp(inp);
    ASSERT_NEAR(0.31326168751822286, ans, 0.0001);
  }

  TEST(log_sum_exp_test, batch) {
    fmatrix inp = fmatrix::Random(5, 10) * 2;
    fvector ans = log_sum_exp(inp).transpose();
    for (int t = 0; t < inp.cols(); ++ t) {
      fvector v = inp.col(t);
      float exp = log_sum_exp(v);
      ASSERT_NEAR(exp, ans(t), 0.0001);
    }
  }

}

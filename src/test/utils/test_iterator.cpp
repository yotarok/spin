#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include "../testutil.hpp"

namespace {
  using namespace spin;
  TEST(combinatorial_iterator_test, normal_path) {
    std::vector<int> ivec;
    ivec.push_back(1);
    ivec.push_back(2);
    ivec.push_back(3);
    auto comit = make_combinatorial_iterator(ivec, 4);
    int ans = 0;
    for (; ! comit.done(); comit.next()) {
      auto vals = comit.value();
      int exp = 0, actual = 0;
      for (auto it = vals.begin(), last = vals.end(); it != last; ++ it) {
        std::cout << *it << "\t";
        actual += (*it - 1) * std::pow(3, exp);
        ++ exp;
      }
      std::cout << std::endl;

      ASSERT_EQ(ans, actual);
      ++ ans;
    }
  }
}

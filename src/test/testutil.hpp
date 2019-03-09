#ifndef stac_test_testutil_hpp_
#define stac_test_testutil_hpp_

#include <boost/format.hpp>
#include <viennacl/matrix.hpp>

#define ASSERT_MATRIX_NEAR(val_exp, val_act, abserr) \
  ::assert_matrix_near_impl((val_exp), (val_act), abserr, #val_exp, #val_act)

#define ASSERT_MATRIX_EQ(val_exp, val_act) \
  ::assert_matrix_near_impl((val_exp), (val_act), abserr, #val_exp, #val_act)



namespace {
  template <typename MatrixT>
  inline void assert_matrix_near_impl(const MatrixT& mat_expected, 
                                      const MatrixT& mat_actual,
                                      float abs_error,
                                      const char* mat_expected_expr,
                                      const char* mat_actual_expr) {
    std::string trace1 = 
      (boost::format("Comparing (%1%) and (%2%) with tolerance abs error = %3%") 
       % mat_expected_expr % mat_actual_expr % abs_error).str();
    SCOPED_TRACE(trace1.c_str());

    ASSERT_EQ(mat_expected.rows(), mat_actual.rows());
    ASSERT_EQ(mat_expected.cols(), mat_actual.cols());
    for (int c = 0; c < mat_expected.cols(); ++ c) {
      for (int r = 0; r < mat_expected.rows(); ++ r) {
        if (std::abs(mat_expected(r, c) - mat_actual(r, c)) < abs_error) continue;
        // ^ prepared a last resort since formatting is so slow
        std::string trace2 = 
          (boost::format("Comparing (%1%, %2%)-th element") % r % c).str();
        SCOPED_TRACE(trace2.c_str());
        ASSERT_NEAR(mat_expected(r, c), mat_actual(r, c), abs_error);
      }
    }
  }

  template <typename MatrixT, typename F, unsigned int A>
  inline void assert_matrix_near_impl(const MatrixT& mat_expected, 
                                      const viennacl::matrix<
                                      typename MatrixT::Scalar, F, A>& mat_actual,
                                      float abs_error,
                                      const char* mat_expected_expr,
                                      const char* mat_actual_expr) {
    MatrixT mat_actual_host(mat_actual.size1(), mat_actual.size2());
    viennacl::copy(mat_actual, mat_actual_host);
    assert_matrix_near_impl(mat_expected, mat_actual_host, abs_error,
                            mat_expected_expr, mat_actual_expr);
  }
  
  template <typename MatrixT, typename F, unsigned int A>
  inline void assert_matrix_near_impl(const MatrixT& mat_expected, 
                                      const 
                                      viennacl::matrix_range<
                                      viennacl::matrix<
                                      typename MatrixT::Scalar, F, A> >& mat_actual,
                                      float abs_error,
                                      const char* mat_expected_expr,
                                      const char* mat_actual_expr) {
    MatrixT mat_actual_host(mat_actual.size1(), mat_actual.size2());
    viennacl::copy(mat_actual, mat_actual_host);
    assert_matrix_near_impl(mat_expected, mat_actual_host, abs_error,
                            mat_expected_expr, mat_actual_expr);
  }

  template <typename MatrixT>
  inline void assert_matrix_eq_impl(const MatrixT& mat_expected, 
                                    const MatrixT& mat_actual,
                                    const char* mat_expected_expr,
                                    const char* mat_actual_expr) {
    std::string trace1 = 
      (boost::format("Comparing exact identity between (%1%) and (%2%)") 
       % mat_expected_expr % mat_actual_expr).str();
    SCOPED_TRACE(trace1.c_str());

    ASSERT_EQ(mat_expected.rows(), mat_actual.rows());
    ASSERT_EQ(mat_expected.cols(), mat_actual.cols());
    for (int c = 0; c < mat_expected.cols(); ++ c) {
      for (int r = 0; r < mat_expected.rows(); ++ r) {
        if (mat_expected(r, c) != mat_actual(r, c)) continue;
        // ^ prepared a last resort since formatting is so slow
        std::string trace2 = 
          (boost::format("Comparing (%1%, %2%)-th element") % r % c).str();
        SCOPED_TRACE(trace2.c_str());
        ASSERT_EQ(mat_expected(r, c), mat_actual(r, c));
      }
    }
  }

}

#endif

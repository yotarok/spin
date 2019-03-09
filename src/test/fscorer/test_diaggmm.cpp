#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include "../testutil.hpp"
#include <spin/fscorer/diaggmm.hpp>
#include <spin/io/yaml.hpp>

namespace {
  const char test_gmm[] = 
    "states:\n"
    "  - means: [0, 1]\n"
    "    vars:  [0, 1]\n"
    "    weights: [0.3, 0.7]\n"
    "  - means: [2, 3]\n"
    "    vars:  [2, 3]\n"
    "    weights: [0.4, 0.6]\n"
    "means:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data:\n"
    "    - [0.0, 0.0]\n"
    "    - [1.0, 0.0]\n"
    "    - [0.0, 1.0]\n"
    "    - [1.0, 1.0]\n"
    "vars:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data:\n"
    "    - [1.0, 1.0]\n"
    "    - [2.0, 1.0]\n"
    "    - [1.0, 2.0]\n"
    "    - [2.0, 2.0]\n"
    "\n";

  using namespace spin;
  TEST(fscorer_diaggmm_test, normal_path) {
    diagonal_GMM_parameter_ptr pparam(new diagonal_GMM_parameter(convert_to_variant(YAML::Load(test_gmm))));
    diagonal_GMM_scorer scorer(pparam);
    fmatrix inp(2, 2);
    inp << 
      0.5, 0.1,
      0.5, 0.8;
    scorer.set_frames(inp);
    // G0,0: np.log(1.0 / sqrt(2 * math.pi * 1.0) * np.exp(-0.5 * (1.0)**-1 * (0.0 - 0.5)**2)) = -1.0439385332046727
    // G0,1: np.log(1.0 / sqrt(2 * math.pi * 1.0) * np.exp(-0.5 * (1.0)**-1 * (0.0 - 0.5)**2)) = -1.0439385332046727
    // G1,0: np.log(1.0 / sqrt(2 * math.pi * 2.0) * np.exp(-0.5 * (2.0)**-1 * (1.0 - 0.5)**2)) = -1.3280121234846454
    // G1,1: np.log(1.0 / sqrt(2 * math.pi * 1.0) * np.exp(-0.5 * (1.0)**-1 * (0.0 - 0.5)**2)) = -1.0439385332046727
    // G0: -2.0878770664093453
    // G1: -2.371950656689318
    // GMM: np.log(0.3*np.exp(-2.0878770664093453) + 0.7*np.exp(-2.371950656689318)) = -2.2779511461364579
    ASSERT_NEAR(-2.27795, scorer.get_score(0, 0), 0.0001);
    
  }
  

}

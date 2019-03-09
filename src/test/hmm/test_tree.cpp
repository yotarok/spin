#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include "../testutil.hpp"
#include <spin/hmm/tree.hpp>
#include <spin/io/yaml.hpp>

namespace {

  const char test_YAML[] = "categories:\n"
    "  \"sil\": [\"sil\"]\n"
    "  \"iy\": [\"iy\"]\n"
    "  \"aa\": [\"aa\"]\n"
    "  \"uh\": [\"uh\"]\n"
    "contextLengths: [0, 0]\n"
    "root:\n"
    "  - question: {qtype: \"context\", category: \"sil\", context: 0}\n"
    "    isTrue:\n"
    "      - question: {qtype: \"location\", value: 0}\n"
    "        isTrue: {leaf: 0, nosplit: false}\n"
    "      - question: {qtype: \"location\", value: 1}\n"
    "        isTrue: {leaf: 1, nosplit: false}\n"
    "        isFalse: {leaf: 2, nosplit: false}\n"
    "  - question: {qtype: \"context\", category: \"iy\", context: 0}\n"
    "    isTrue:\n"
    "      - question: {qtype: \"location\", value: 0}\n"
    "        isTrue: {leaf: 3, nosplit: false}\n"
    "      - question: {qtype: \"location\", value: 1}\n"
    "        isTrue: {leaf: 4, nosplit: false}\n"
    "        isFalse: {leaf: 5, nosplit: false}\n"
    "  - question: {qtype: \"context\", category: \"aa\", context: 0}\n"
    "    isTrue:\n"
    "      - question: {qtype: \"location\", value: 0}\n"
    "        isTrue: {leaf: 6, nosplit: false}\n"
    "      - question: {qtype: \"location\", value: 1}\n"
    "        isTrue: {leaf: 7, nosplit: false}\n"
    "        isFalse: {leaf: 8, nosplit: false}\n"
    "  - question: {qtype: \"context\", category: \"uh\", context: 0}\n"
    "    isTrue:\n"
    "      - question: {qtype: \"location\", value: 0}\n"
    "        isTrue: {leaf: 9, nosplit: false}\n"
    "      - question: {qtype: \"location\", value: 1}\n"
    "        isTrue: {leaf: 10, nosplit: false}\n"
    "        isFalse: {leaf: 11, nosplit: false}\n"
    "\n"; 
  
  using namespace spin;
  TEST(hmm_tree_test, deserialize) {
    variant_t treedata = convert_to_variant(YAML::Load(test_YAML));
    context_decision_tree* ptree = new context_decision_tree(treedata);
    variant_t dest;
    ptree->write(&dest);
    std::cout << make_node_from_variant(dest) << std::endl;
    HMM_context ctx;
    ctx.label.push_back("iy");
    ctx.loc = 1;
    ASSERT_EQ(4, ptree->resolve(ctx));
    ctx.loc = 2;
    ASSERT_EQ(5, ptree->resolve(ctx));


    delete ptree;
  }

}

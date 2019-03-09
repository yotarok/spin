#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>
#include <spin/nnet/nnet.hpp>
#include <spin/nnet/node.hpp>
#include <spin/nnet/affine.hpp>
#include <spin/nnet/relu.hpp>
#include <spin/nnet/sigmoid.hpp>
#include <spin/nnet/ident.hpp>

#include "../testutil.hpp"

namespace {
  using namespace spin;

  template <typename HidActT>
  void make_sample_nnet(nnet& nnet) {
    auto inpnode = new nnet_node_ident("input"); 
    auto aff1node = new nnet_node_affine_transform("aff1");
    auto hid1node = new HidActT("hid1");
    auto aff2node = new nnet_node_affine_transform("aff2");
    auto outnode  = new nnet_node_ident("output");

    nnet.add_node(nnet_node_ptr(inpnode), std::vector<std::string>());
    nnet.add_node(nnet_node_ptr(aff1node), std::vector<std::string>(1, "input"));
    nnet.add_node(nnet_node_ptr(hid1node), std::vector<std::string>(1, "aff1"));
    nnet.add_node(nnet_node_ptr(aff2node), std::vector<std::string>(1, "hid1"));
    nnet.add_node(nnet_node_ptr(outnode), std::vector<std::string>(1, "aff2"));

    inpnode->set_ndim(2);

    fmatrix w1(2, 2);
    w1 << 0.1, 0.2,
      -0.3, 0.4;
    fmatrix b1(2, 1);
    b1 << -0.1, 0.1;

    viennacl::copy(w1, aff1node->weight());
    viennacl::copy(b1, aff1node->bias());

    hid1node->set_ndim(2);

    fmatrix w2(2, 2);
    w2 << 5, -4,
      -3, 2;
    fmatrix b2(2, 1);
    b2 << 0.1, -0.2;
    
    viennacl::copy(w2, aff2node->weight());
    viennacl::copy(b2, aff2node->bias());

    outnode->set_ndim(2);

  }

  void make_sample_batch(std::vector<corpus_entry>& batches) {
    batches.clear();
    fmatrix input(2, 2);
    input <<
      1.0, 0.0,
      0.0, 0.0;
    intmatrix label(1, 2);
    label << 0, 1;
    corpus_entry batch;
    batch["input"] = input;
    batch["output"] = label;
    batches.push_back(batch);
  }
  
  TEST(nnet_test, hand_verification) {

    nnet nnet;
    make_sample_nnet<nnet_node_relu>(nnet);
    auto inpnode =
      std::dynamic_pointer_cast<nnet_node_ident>(nnet.node("input"));
    auto aff1node =
      std::dynamic_pointer_cast<nnet_node_affine_transform>(nnet.node("aff1"));
    auto hid1node = 
      std::dynamic_pointer_cast<nnet_node_relu>(nnet.node("hid1"));
    auto aff2node = 
      std::dynamic_pointer_cast<nnet_node_affine_transform>(nnet.node("aff2"));
    auto outnode  = 
      std::dynamic_pointer_cast<nnet_node_ident>(nnet.node("output"));


    nnet_context context(nnet, 1, 2);
    std::vector<stream_ptr> streams;
    
    auto inputstream = new input_stream("input", "feature", "");
    auto labelstream = new xent_label_stream("output", "state", "");
    streams.push_back(stream_ptr(inputstream));
    streams.push_back(stream_ptr(labelstream));

    std::vector<corpus_entry> batches;
    make_sample_batch(batches);

    nnet_config config(nnet);

    fmatrix input(2, 2);
    input <<
      1.0, 0.0,
      0.0, 0.0;

    std::cout << "set forward stream" << std::endl;
    context.before_forward(streams, batches);
    ASSERT_MATRIX_NEAR(input, NNET_BATCH(context.get("input")->get_output(), 0), 0.00001);
    std::cout << "do forward steps" << std::endl;
    nnet.feed_forward(config, &context);
    ASSERT_MATRIX_NEAR(input,
                       NNET_BATCH(context.get("aff1")->get_input(), 0),
                       0.00001);
    std::cout << "check aff1 forward" << std::endl;
    fmatrix aff1ans(2, 2);
    aff1ans << 0.0, -0.1,
      -0.2, 0.1;
    ASSERT_MATRIX_NEAR(aff1ans, NNET_BATCH(context.get("aff1")->get_output(), 0), 0.00001);
    std::cout << "check hid1 forward" << std::endl;
    fmatrix reluans(2, 2);
    reluans << 0.0, 0.0,
      0.0, 0.1;
    ASSERT_MATRIX_NEAR(reluans,
                       NNET_BATCH(context.get("hid1")->get_output(), 0),
                       0.00001);

    std::cout << "check aff2 forward" << std::endl;
    fmatrix aff2ans(2, 2);
    aff2ans << 0.1, -0.3,
      -0.2, 0.0;
    ASSERT_MATRIX_NEAR(aff2ans,
                       NNET_BATCH(context.get("aff2")->get_output(), 0),
                       0.00001);

    std::cout << "set backward stream" << std::endl;
    context.before_backward(streams, batches);

    std::cout << "check diff_input backward" << std::endl;

    fmatrix dlossans(2, 2);
    dlossans << -0.425557, 0.425557,
      0.425557, -0.425557;
    ASSERT_MATRIX_NEAR(dlossans,
                       NNET_BATCH(context.get("output")->get_diff_input(), 0),
                       0.00001);

    std::cout << "do backward steps" << std::endl;
    nnet.back_propagate(config, &context);
    ASSERT_MATRIX_NEAR(dlossans,
                       NNET_BATCH(context.get("aff2")->get_diff_output(), 0),
                       0.00001);
    
    std::cout << "check aff2 backward" << std::endl;
    fmatrix aff2bans(2, 2);
    aff2bans << -8, 8,
      6, -6;
    aff2bans *= 0.425557;
    ASSERT_MATRIX_NEAR(aff2bans,
                       NNET_BATCH(context.get("aff2")->get_diff_input(), 0),
                       0.00001);

    std::cout << "check hid1 backward" << std::endl;
    fmatrix hid1bans(2, 2);
    hid1bans << -8, 0,
      0, -6;
    hid1bans *= 0.425557;
    ASSERT_MATRIX_NEAR(hid1bans,
                       NNET_BATCH(context.get("hid1")->get_diff_input(), 0),
                       0.00001);

    std::cout << "check aff1 backward" << std::endl;
    fmatrix aff1bans(2, 2);
    aff1bans << -0.8, 1.8,
      -1.6, -2.4;
    aff1bans *= 0.425557;
    ASSERT_MATRIX_NEAR(aff1bans,
                       NNET_BATCH(context.get("aff1")->get_diff_input(), 0),
                       0.00001);

    
    std::cout << "do accumulation" << std::endl;
    nnet_delta delta(nnet);
    context.accumulate_delta(config, &delta);
    
    std::cout << "check aff1 delta" << std::endl;
    fmatrix dw1ans(2, 2);
    dw1ans << -8, 0,
      0, 0;
    dw1ans *= 0.425557;
    auto pdaff1 = std::dynamic_pointer_cast<affine_delta>(delta.get("aff1"));
    ASSERT_MATRIX_NEAR(dw1ans, pdaff1->get_dweight(), 0.00001);
    
  }

  TEST(nnet_test, sigmoid_forward) {
    nnet nnet;
    make_sample_nnet<nnet_node_sigmoid>(nnet);
    auto inpnode =
      std::dynamic_pointer_cast<nnet_node_ident>(nnet.node("input"));
    auto aff1node =
      std::dynamic_pointer_cast<nnet_node_affine_transform>(nnet.node("aff1"));
    auto hid1node = 
      std::dynamic_pointer_cast<nnet_node_sigmoid>(nnet.node("hid1"));
    auto aff2node = 
      std::dynamic_pointer_cast<nnet_node_affine_transform>(nnet.node("aff2"));
    auto outnode  = 
      std::dynamic_pointer_cast<nnet_node_ident>(nnet.node("output"));

    nnet_context context(nnet, 1, 2);
    std::vector<stream_ptr> streams;
    
    auto inputstream = new input_stream("input", "feature", "");
    auto labelstream = new xent_label_stream("output", "state", "");
    streams.push_back(stream_ptr(inputstream));
    streams.push_back(stream_ptr(labelstream));

    std::vector<corpus_entry> batches;
    make_sample_batch(batches);

    nnet_config config(nnet);

    fmatrix input(2, 2);
    input <<
      1.0, 0.0,
      0.0, 0.0;

    std::cout << "set forward stream" << std::endl;
    context.before_forward(streams, batches);
    nnet.feed_forward(config, &context);
    for (int loc = 0; loc < nnet.nnodes(); ++ loc) {
      std::cout << "loc = " << loc << std::endl;
      std::cout << " input: " << std::endl;
      std::cout << NNET_BATCH(context.get(loc)->get_input(), 0) << std::endl;
      std::cout << " ---" << std::endl;
    }

  }
}


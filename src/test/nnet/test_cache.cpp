#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include "../testutil.hpp"
#include <spin/nnet/cache.hpp>
#include <spin/corpus/yaml.hpp>

namespace {
  const char test_float[] =
    "---\n"
    "+key: s1\n"
    "feature:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data: [[1.0, 0.2], [2.0, 0.5], [3.0, 0.8], [4.0, 1.1]]\n"
    "---\n"
    "+key: s2\n"
    "feature:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data: [[5.0, 0.4], [6.0, 0.8], [7.0, 0.3], [8.0, 0.7], [9.0, 1.1]]\n"
    "---\n"
    "+key: s3\n"
    "feature:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data: [[10.0, 0.6], [11.0, 0.1], [12.0, 0.7]]\n"
    "---\n"
    "+key: s4\n"
    "feature:\n"
    "  type: fmatrix\n"
    "  transpose: true\n"
    "  data: [[13.0,1.6], [14.0,-1.1], [15.0,-2.7], [16.0,-1.2], [17.0,-2.1]]\n"
    ;

  const char test_int[] =
    "---\n"
    "+key: s1\n"
    "state:\n"
    "  type: intmatrix\n"
    "  transpose: false\n"
    "  data: [[17, 16, 15, 14]]\n"
    "---\n"
    "+key: s2\n"
    "state:\n"
    "  type: intmatrix\n"
    "  transpose: false\n"
    "  data: [[13, 12, 11, 10, 9]]\n"
    "---\n"
    "+key: s3\n"
    "state:\n"
    "  type: intmatrix\n"
    "  transpose: false\n"
    "  data: [[8, 7, 6]]\n"
    "---\n"
    "+key: s4\n"
    "state:\n"
    "  type: intmatrix\n"
    "  transpose: false\n"
    "  data: [[5, 4, 3, 2, 1]]\n"
    ;

 
  using namespace spin;

  TEST(cache_test, run_with_zip) {
    corpus_iterator_ptr pcit1(new yaml_corpus_iterator(new std::istringstream(test_float)));
    corpus_iterator_ptr pcit2(new yaml_corpus_iterator(new std::istringstream(test_int)));
    zipped_corpus_iterator_ptr zcit = zip_corpus("feature_feature", pcit1, "state_state", pcit2);

    zcit->import_key(0, "feature", "feature");
    zcit->import_key(1, "state", "state");

    std::vector<stream_ptr> streams;
    streams.push_back(stream_ptr(new input_stream("feature", "feature", "")));
    streams.push_back(stream_ptr(new xent_label_stream("state", "state", "")));

    std::vector<gear::flow_ptr> flows;
    flows.push_back(gear::flow_ptr());
    flows.push_back(gear::flow_ptr());

    nnet_input_data nn_input_data(zcit, streams, flows);

    random_frame_cache cache(nn_input_data, streams, 2, 8, 0);
    cache.initialize();

    std::set<int> appeared;
    while (! cache.done()) {
      fmatrix feat = boost::get<fmatrix>(cache.data("feature"));
      intmatrix state = boost::get<intmatrix>(cache.data("state"));
      std::cout << "feat = " << std::endl << feat << std::endl;
      std::cout << "state= " << std::endl << state << std::endl;
      for (int t = 0; t < feat.cols(); ++ t) {
        ASSERT_EQ(18, static_cast<int>(feat(0, t)) + state(0, t));
      }
      cache.next();
    }

  }

}

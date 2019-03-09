#ifndef spin_flow_flowutils_hpp_
#define spin_flow_flowutils_hpp_

#include <gear/flow/flow.hpp>

namespace spin {
  template <typename T>
  void add_source_sink_inplace(gear::flow_ptr flow) {
    if (! flow) return; // just ignores null

    auto src = new gear::vector_source_node<T>("source", "");
    // ^ TO DO: Implement variant source that changes its behaviour depending
    //          on the underlying corpus entry, and change to use it
    auto sink = new gear::vector_sink_node<T>("sink", "");
    flow->add(src);
    flow->add(sink);
    flow->connect("source", "", "_input", "");
    flow->connect("_output", "", "sink", "");
  }

  /**
   * Apply matrix transformation defined in flow graph
   *
   * WARNING: This assumes a flow with source and sink node 
   *           named "source" and "sink", respectively.
   */
  template <typename NumT>
  void apply_matrix_flow_inplace(variant_t* var, gear::flow_ptr flow) {
    if (! flow) return;
    // Assumed to have source and sink already
    Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic> inp;
    read_matrix<NumT>(&inp, *var);
    auto src = std::dynamic_pointer_cast<gear::vector_source_node<NumT>
                                         >(flow->node("source"));
    src->set_meta_data(inp.rows(), 0, "");
    src->append_data(inp);

    src->feed_BOS();
    src->feed_data_all();
    src->feed_EOS();

    auto sink = std::dynamic_pointer_cast<gear::vector_sink_node<NumT>
                                          >(flow->node("sink"));


    *var = sink->get_output();
  }

}

#endif

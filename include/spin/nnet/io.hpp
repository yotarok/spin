#ifndef spin_nnet_io_hpp_
#define spin_nnet_io_hpp_

#include <spin/types.hpp>
#include <spin/io/variant.hpp>
#include <spin/nnet/node.hpp>
#include <spin/nnet/stream.hpp>
#include <spin/corpus/corpus.hpp>
#include <gear/flow/flow.hpp>

namespace spin {
  class nnet_context;
  
  /**
   * Class representing input corpora for neural nets
   */
  class nnet_input_data {
    zipped_corpus_iterator_ptr zit_;
    const std::vector<stream_ptr>& streams_;
    std::vector<gear::flow_ptr> flows_;

    /**
     * Prepare flows by adding source and sink
     */
    void initialize_flows(const std::vector<gear::flow_ptr>& flows_raw);

    /**
     * Prepare zipped_corpus_iterator
     */
    void initialize_zipped_iterator();
  public:
    /**
     * Create nnet_input with set of streams
     */
    nnet_input_data(const std::vector<stream_ptr>& streams,
                    const std::vector<gear::flow_ptr>& flows);

    /**
     * Construct nnet_input from zipped corpus_iterator
     *
     * This is only for debug purposes. 
     * This constructor ignores all source specifications in 
     * stream objects, and reads data from the given iterator.
     */
    nnet_input_data(zipped_corpus_iterator_ptr zit,
                    const std::vector<stream_ptr>& streams,
                    const std::vector<gear::flow_ptr>& flows);

    bool done() const { return zit_->done(); }
    /**
     * Load next sequence, transform with flow, and store it to a dictionary 
     * keyed by component names
     */
    void pull_next_sequence(corpus_entry* dest);
  };

  class nnet_output_writer {
    const std::vector<stream_ptr>& streams_;
    std::vector<corpus_writer_ptr> writers_;
  public:
    nnet_output_writer(const std::vector<stream_ptr>& streams);
    void write_sequence(const nnet_context& context,
                        const corpus_entry& metainfo);
  };

  /**
   * Create zipped_corpus_iterator with refering all corpora used in streams
   */
  zipped_corpus_iterator_ptr
  prepare_zipped_corpus_iterator(const std::vector<stream_ptr>&);


  /**
   * Apply prepared flows
   */
  void apply_prepared_flow_inplace(const std::vector<stream_ptr>& streams,
                                   const std::vector<gear::flow_ptr>& flows,
                                   corpus_entry* e);

  /**
   * Remap corpus entry with tag name keys to corpus entry with 
   * component name keys
   */
  void remap_corpus_entries(const std::vector<stream_ptr>& streams,
                            const corpus_entry& src,
                            corpus_entry* e);

}

#endif

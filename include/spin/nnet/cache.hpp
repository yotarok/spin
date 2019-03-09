#ifndef spin_nnet_framecache_hpp_
#define spin_nnet_framecache_hpp_

#include <boost/variant.hpp>
#include <boost/random.hpp>

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/corpus/corpus.hpp>
#include <gear/flow/flow.hpp>
#include <spin/nnet/stream.hpp>
#include <spin/nnet/io.hpp>


namespace spin {
  /**
   * Random cache for supporting minibatch SGD with several data sources
   */
  class random_frame_cache {
    // This cache has several stages as below
    // _cit => _next_seq (one segment) indexed with NN component name
    //           => _data (several shuffled segment) => _cur_data
    //
    // _cit => _next_seq
    //   apply pre-specified flow
    //   check consistency of type and dimensionality
    //
    // _next_seq => _data
    //   Copy head of _next_seq as frames are consumed from _data
    //   _next_off keep track of how many frames are used from _next_seq
    //   if _next_off == _next_len, reload _next_seq
    //   After each move from _next_seq to _data, moved frames are randomly replaced
    //
    // _data => _cur_data (implemented in "set_cursor")
    //
   

    nnet_input_data& _input_data;
    size_t _batchsize;
    size_t _cachesize;

    boost::mt19937 _rng;
    
    corpus_entry _next_seq;
    int _next_len;
    int _next_off;

    std::map<std::string, variant_t> _data;
    // ^ variant but expect only fmatrix or intmatrix
    std::map<std::string, int> _dims;
    std::map<std::string, int> _types;
    std::map<std::string, variant_t> _cur_data;
    // ^ variant but expect only fmatrix or intmatrix

    size_t _leftbound; // used after finishing stream
    bool _inited;
    bool _done;
    

    std::vector<stream_ptr> _streams;
    std::vector<gear::flow_ptr> _flows;
    
    int fill_cache(int right);
    bool pull_next_sequence();
    void shuffle_cache(int left, int right, boost::mt19937& rng);
    void set_cursor(int offset);
  public:
    random_frame_cache(nnet_input_data& input_data,
                       const std::vector<stream_ptr>& streams,
                       size_t batchsize, size_t cachesize,
                       int seed = 0x5EED);

    void next();
    bool done();

    const variant_t& data(const std::string& k) const;
    void retrieve(corpus_entry* pent) const;
    

    void initialize();
  };
}

#endif

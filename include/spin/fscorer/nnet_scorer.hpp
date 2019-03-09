#ifndef spin_fscorer_nnet_hpp_
#define spin_fscorer_nnet_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/fscorer/frame_scorer.hpp>

namespace spin {
  class nnet;
  class nnet_context;
  class nnet_config;
  
  class nnet_scorer : public frame_scorer {
    // So far, this does not support multistream inputs, and tag
    //
    std::shared_ptr<nnet> _parameter;
    std::shared_ptr<nnet_config> _nnet_config;

    std::shared_ptr<nnet_context> _context;

    bool _feed_forward_done;
    fmatrix _score;
  public:
    nnet_scorer(std::shared_ptr<nnet> param);
    virtual void set_frames(const fmatrix& frames);
    virtual float get_score(int t, int s);
    virtual size_t nstates() const;
  };
}

#endif

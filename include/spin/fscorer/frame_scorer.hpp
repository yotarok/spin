#ifndef spin_fscorer_frame_scorer_hpp_
#define spin_fscorer_frame_scorer_hpp_

namespace spin {
  
  // parameter, statistics, gradients shares same topology
  class frame_scorer_parameter {
  public:
    ~frame_scorer_parameter() { }
    //zero()
    //virtual void add(const frame_scorer_parameter& oth)=0;
    //virtual void scale(const frame_scorer_parameter& oth)=0;
    
    //
  };

  class frame_scorer {
  public:
    virtual void set_frames(const fmatrix& features)=0;
    virtual float get_score(int t, int s)=0;
    virtual size_t nstates() const =0;
  };
}

#endif

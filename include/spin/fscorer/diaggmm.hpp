#ifndef spin_fscorer_diaggmm_hpp_
#define spin_fscorer_diaggmm_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/fscorer/frame_scorer.hpp>

namespace spin {

  struct diagonal_GMM_statedef {
    std::vector<int> mean_ids;
    std::vector<int> var_ids;
    fvector logweights;
  };
  
  class diagonal_GMM_parameter;

  class diagonal_GMM_statistics {
  public:
    typedef double statscalar;
    typedef Eigen::Matrix<statscalar, Eigen::Dynamic, Eigen::Dynamic> statmatrix;
    typedef Eigen::Matrix<statscalar, Eigen::Dynamic, 1> statvector;
  private:
    statscalar _loglike;
    statmatrix _zero; // (max # mixt) x (# state)
    statmatrix _first; // (feadim) x (# mean)
    statmatrix _second; // (feadim) x (# var)
  public:

    void accumulate(const diagonal_GMM_statistics& other);
    void accumulate(int s, fmatrix features,
                    const diagonal_GMM_parameter& param);
    
    diagonal_GMM_statistics() { }
    diagonal_GMM_statistics(const diagonal_GMM_parameter& param);
    diagonal_GMM_statistics(const variant_t& src) { read(src); }
    diagonal_GMM_statistics(int ndim, int nmaxmix, int nstate,
                            int nmean, int nvar);

    statscalar get_loglikelihood() const { return _loglike; }
    statscalar& get_loglikelihood() { return _loglike; }
    const statmatrix& get_zero() const { return _zero; }
    statmatrix& get_zero() { return _zero; }
    const statmatrix& get_first() const { return _first; }
    statmatrix& get_first() { return _first; }
    const statmatrix& get_second() const { return _second; }
    statmatrix& get_second() { return _second; }
    //fvector& get_first_denom() { return _first_denom; }
    //fvector& get_second_denom() { return _second_denom; }

    void read(const variant_t& src);
    void write(variant_t* dest);
  };
  
  class diagonal_GMM_parameter : public frame_scorer_parameter {
  public:
    typedef diagonal_GMM_statistics::statscalar statscalar;
    typedef diagonal_GMM_statistics::statvector statvector;
    typedef diagonal_GMM_statistics::statmatrix statmatrix;
  private:
    fmatrix _means; // feadim x nmeans
    fmatrix _sqrtprecs;
    fvector _logzs;
    std::vector<diagonal_GMM_statedef> _states;
    void recompute_logZs();
  public:
    diagonal_GMM_parameter() { }
    diagonal_GMM_parameter(variant_t node);

    size_t nstates() const { return _states.size(); }
    size_t nmeans() const { return _means.cols(); }
    size_t nsqrtprecs() const { return _sqrtprecs.cols(); }
    size_t nfeatures() const { return _means.rows(); }
    size_t max_nmixtures() const;

    fmatrix& means() { return _means; }
    const fmatrix& means() const { return _means; }

    fmatrix means(int s) const;
    std::vector<int> mean_ids(int s) const;

    fmatrix& sqrtprecs() { return _sqrtprecs; }
    const fmatrix& sqrtprecs() const { return _sqrtprecs; }

    fmatrix sqrtprecs(int s) const;
    std::vector<int> sqrtprec_ids(int s) const;

    fvector& logzs() { return _logzs; }
    const fvector& logzs() const { return _logzs; }
    fvector logzs(int s) const;

    fvector logweights(int s) const;

    std::vector<diagonal_GMM_statedef>& statedefs() { return _states; }

    void reestimation(const diagonal_GMM_statistics& stats,
                      float var_minocc, float var_floor);

    void split_all_components(int s, float deltafac);

    template <typename NumT>
    void
    get_gaussian_scores(const fmatrix& feat,
                        const fmatrix& means,
                        const fmatrix& sqrtprecs,
                        const fvector& logzs,
                        Eigen::Matrix<NumT,Eigen::Dynamic,Eigen::Dynamic>* dest)
      const {
      Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic> dists =
        Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic>(means.cols(), feat.cols());
      for (int g = 0; g < means.cols(); ++ g) {
        dists.row(g) = (sqrtprecs.col(g).cast<NumT>().asDiagonal() *
                        (feat.colwise() - means.col(g)).cast<NumT>())
          .colwise().squaredNorm();
      }
      *dest = (-0.5 * dists).colwise() - logzs.cast<NumT>();
    }
    

    void write(variant_t* dest);
  };

  typedef boost::shared_ptr<diagonal_GMM_parameter> diagonal_GMM_parameter_ptr;

  class diagonal_GMM_scorer : public frame_scorer {
    diagonal_GMM_parameter_ptr _parameter;

    fmatrix _input_buffer;
    fmatrix _score_cache;
  public:
    diagonal_GMM_scorer(diagonal_GMM_parameter_ptr param);
    virtual void set_frames(const fmatrix& frames);
    virtual float get_score(int t, int s);
    virtual size_t nstates() const;
  };
}

#endif

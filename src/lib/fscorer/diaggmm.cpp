#include <spin/fscorer/diaggmm.hpp>
#include <spin/utils.hpp>
#include <gear/io/logging.hpp>
#include <cfloat>

namespace spin {

  diagonal_GMM_statistics::diagonal_GMM_statistics(const diagonal_GMM_parameter&
                                                   param) {
    _zero = statmatrix::Zero(param.max_nmixtures(), param.nstates());
    _first = statmatrix::Zero(param.nfeatures(), param.nmeans());
    _second = statmatrix::Zero(param.nfeatures(), param.nsqrtprecs());
  }

  diagonal_GMM_statistics::diagonal_GMM_statistics(int ndim, int nmaxmix,
                                                   int nstate, int nmean,
                                                   int nvar) {
    _zero = statmatrix::Zero(nmaxmix, nstate);
    _first = statmatrix::Zero(ndim, nmean);
    _second = statmatrix::Zero(ndim, nvar);    
  }

  
  void
  diagonal_GMM_statistics::accumulate(const diagonal_GMM_statistics& other) {
    _loglike += other._loglike;
    _zero += other._zero;
    _first += other._first;
    _second += other._second;
  }
  
  void
  diagonal_GMM_statistics::accumulate(int s, fmatrix features,
                                      const diagonal_GMM_parameter& param) {
    dmatrix gll;
    param.get_gaussian_scores<double>(features,
                                      param.means(s),
                                      param.sqrtprecs(s),
                                      param.logzs(s),
                                      &gll);
    //std::cout << "Gaussian Loglikes = " << std::endl << gll << std::endl;
    std::vector<int> means = param.mean_ids(s);
    std::vector<int> sqprs = param.sqrtprec_ids(s);
    for (int t = 0; t < features.cols(); ++ t) {
      statvector lik = gll.col(t).cast<statscalar>();
      statvector logprior = param.logweights(s).cast<statscalar>();
      statvector logjoint = lik + logprior;
      //logjoint = logjoint.cwiseMax(logjoint.maxCoeff() - 10.0f);
      statscalar framell = log_sum_exp(logjoint);
      this->get_loglikelihood() += framell;

      statvector logpost =
        (logjoint.array() - framell).matrix();
      //std::cout << "Log posterior " << t << " = " << std::endl << logpost << std::endl;
      statvector post = logpost.array().exp().matrix();

      this->get_zero().col(s) += post;
      statvector x = features.col(t).cast<statscalar>();
      for(int m = 0; m < gll.rows(); ++ m) {
        this->get_first().col(means[m]) += post(m) * x;
        this->get_second().col(sqprs[m]) += post(m) * x.cwiseProduct(x);
      }
    }
  }

  void diagonal_GMM_statistics::read(const variant_t& src) {
    const variant_map& map = boost::get<variant_map>(src);
    _loglike = get_prop<statscalar>(map, "loglikes");
    _zero = get_prop<statmatrix>(map, "zero");
    _first = get_prop<statmatrix>(map, "first");
    _second = get_prop<statmatrix>(map, "second");
  }
  
  void diagonal_GMM_statistics::write(variant_t* dest) {
    variant_map props;
    props["loglikes"] = _loglike;
    props["zero"] = _zero;
    props["first"] = _first;
    props["second"] = _second;
    *dest = props;
  }

  diagonal_GMM_parameter::diagonal_GMM_parameter(variant_t node) {
    variant_map& map = boost::get<variant_map>(node);
    _means = boost::get<fmatrix>(map["means"]);
    fmatrix diagvars = boost::get<fmatrix>(map["vars"]);
    _sqrtprecs = diagvars.array().sqrt().inverse().matrix();
    int dim = _means.rows();
    recompute_logZs();
    
    variant_vector& states = boost::get<variant_vector>(map["states"]);

    for (int s = 0; s < states.size(); ++ s) {
      variant_map& st = boost::get<variant_map>(states[s]);
      variant_vector& means = boost::get<variant_vector>(st["means"]);
      variant_vector& vars = boost::get<variant_vector>(st["vars"]);
      variant_vector& weights = boost::get<variant_vector>(st["weights"]);


      diagonal_GMM_statedef statedef;
      statedef.logweights = fvector::Zero(weights.size());
      for (int n = 0; n < means.size(); ++ n) 
        statedef.mean_ids.push_back(boost::get<int>(means[n]));
      for (int n = 0; n < vars.size(); ++ n) 
        statedef.var_ids.push_back(boost::get<int>(vars[n]));
      for (int n = 0; n < weights.size(); ++ n) 
        statedef.logweights(n) = std::log(boost::get<double>(weights[n]));

      _states.push_back(statedef);
    }
  }

  void diagonal_GMM_parameter::write(variant_t* dest) {
    variant_map props;
    props["means"] = _means;
    fmatrix vars = _sqrtprecs.array().inverse().square().matrix();
    props["vars"] = vars;
    variant_vector states;
    for (int s = 0; s < _states.size(); ++ s) {
      variant_vector meanids, varids, weights;
      for (int m = 0; m < _states[s].mean_ids.size(); ++ m) {
        meanids.push_back(_states[s].mean_ids[m]);
      }
      for (int m = 0; m < _states[s].var_ids.size(); ++ m) {
        varids.push_back(_states[s].var_ids[m]);
      }
      for (int m = 0; m < _states[s].logweights.size(); ++ m) {
        weights.push_back(std::exp(_states[s].logweights(m)));
      }
      variant_map stateprops;
      stateprops["means"] = meanids;
      stateprops["vars"] = varids;
      stateprops["weights"] = weights;
      states.push_back(stateprops);
    }
    props["states"] = states;
    *dest = props;
  }

  void diagonal_GMM_parameter::recompute_logZs() {
    _logzs = (- _sqrtprecs.array().log().matrix().colwise().sum().array() + _means.rows() * (0.5 * std::log(2.0 * M_PI))).matrix();
  }
  
  size_t diagonal_GMM_parameter::max_nmixtures() const {
    size_t max = 0;
    for (std::vector<diagonal_GMM_statedef>::const_iterator
           it = _states.begin(), last = _states.end(); it != last; ++ it) {
      size_t m = it->mean_ids.size();
      if (max < m) max = m;
    }
    return max;
  }

  void
  diagonal_GMM_parameter::reestimation(const diagonal_GMM_statistics& stats,
                                       float var_minocc, float var_floor) {
    statvector zero_mean = statvector::Zero(this->nmeans());
    statvector zero_var = statvector::Zero(this->nsqrtprecs());
    statmatrix first_var = statmatrix::Zero(this->nfeatures(), this->nsqrtprecs());

    statvector states_zero = statvector::Zero(this->nstates());
    statmatrix states_first = statmatrix::Zero(this->nfeatures(), this->nstates());
    statmatrix states_second = statmatrix::Zero(this->nfeatures(), this->nstates());
    statmatrix states_mean = statmatrix::Zero(this->nfeatures(), this->nstates());
    statmatrix states_var = statmatrix::Zero(this->nfeatures(), this->nstates());

    statscalar global_zero = 0.0;
    statvector global_first = statvector::Zero(this->nfeatures());
    statvector global_second = statvector::Zero(this->nfeatures());
    statvector global_mean = statvector::Zero(this->nfeatures());
    statvector global_var = statvector::Zero(this->nfeatures());
      
    std::map<int, int> varid2sid;
    bool error = false;

    for (int s = 0; s < this->nstates(); ++ s) {
      for (int m = 0; m < _states[s].mean_ids.size(); ++ m) {
        zero_mean(_states[s].mean_ids[m]) += stats.get_zero()(m, s);
        zero_var(_states[s].var_ids[m]) += stats.get_zero()(m, s);
        first_var.col(_states[s].var_ids[m]) +=
          stats.get_first().col(_states[s].mean_ids[m]);

        states_zero(s) += stats.get_zero()(m, s);
        states_first.col(s) += stats.get_first().col(_states[s].mean_ids[m]);
        states_second.col(s) += stats.get_second().col(_states[s].mean_ids[m]);

        global_zero += stats.get_zero()(m, s);
        global_first += stats.get_first().col(_states[s].mean_ids[m]);
        global_second += stats.get_second().col(_states[s].mean_ids[m]);

        varid2sid[_states[s].var_ids[m]] = s;
      }
      states_mean.col(s) = states_first.col(s) / states_zero(s);
      states_var.col(s) =
        states_second.col(s) / states_zero(s)
        - states_mean.col(s).array().square().matrix();
    }
    global_mean = global_first / global_zero;
    global_var = global_second / global_zero
      - global_mean.array().square().matrix();

    for (int i = 0; i < this->nmeans(); ++ i) {
      if (zero_mean(i) > 0.0) {
        _means.col(i) = (stats.get_first().col(i) / zero_mean(i)).cast<float>();
      } else {
        WARN("Denominator of mean %d is zero", i);
      }
    }
    for (int i = 0; i < this->nsqrtprecs(); ++ i) {
      //std::cout << "Denominator of variance " << i << " is " << zero_var(i) << std::endl;
      if (zero_var(i) < var_minocc) {
        std::cout << "Denominator of variance " << i << " is less than threshold"
                  << " (" << zero_var(i) << " < " << var_minocc << ")"
                  << std::endl;
        continue;
      }
      statvector mean = first_var.col(i) / zero_var(i);
      statvector var = stats.get_second().col(i) / zero_var(i) -
        mean.array().square().matrix();

      statvector floor = static_cast<statscalar>(var_floor) * states_var.col(varid2sid[i]); 
      if ((var.array() < floor.array()).any()) {
        WARN("One of elements in variance %d is less than threshold", i);
        std::cerr << "var = " << std::endl << var << std::endl;
        std::cerr << "floor = " << std::endl << floor << std::endl;
        var = var.cwiseMax(floor);
      }
      _sqrtprecs.col(i) = var.array().sqrt().inverse().matrix().cast<float>();
    }

    for (int s = 0; s < this->nstates(); ++ s) {
      statscalar denom = std::max(stats.get_zero().col(s).sum(),
                                  (statscalar) FLT_MIN);
      _states[s].logweights =
        (stats.get_zero().col(s) / denom).array().log().matrix().cast<float>();
      if (denom == 0) {
        ERROR("Denominator of weight for state %d is zero", s);
        error = true;
      }
    }
    if (error) {
      throw std::runtime_error("Reestimation error occuered");
    }

    recompute_logZs();
  }

  
  void diagonal_GMM_parameter::split_all_components(int s, float deltafac) {
    int oldM = _states[s].logweights.size();
    for (int m = 0; m < oldM; ++ m) {
      _states[s].logweights(m) = _states[s].logweights(m) - M_LOG2;
      int nM = _states[s].logweights.size();
      _states[s].logweights.conservativeResize(_states[s].logweights.size() + 1);
      _states[s].logweights(nM) = _states[s].logweights(m) - M_LOG2;

      int ovarid = _states[s].var_ids[m];
      int omeanid = _states[s].mean_ids[m];
      fvector dev = deltafac * _sqrtprecs.col(ovarid).array().inverse().matrix();

      int nmeanid = _means.cols();
      _means.col(omeanid) = _means.col(omeanid) - dev;
      _means.conservativeResize(_means.rows(), _means.cols() + 1);
      _means.col(nmeanid) = _means.col(omeanid) + dev;

      int nvarid = _sqrtprecs.cols();
      _sqrtprecs.conservativeResize(_sqrtprecs.rows(), _sqrtprecs.cols() + 1);
      _sqrtprecs.col(nvarid) = _sqrtprecs.col(ovarid);

      _states[s].mean_ids.push_back(nmeanid);
      _states[s].var_ids.push_back(nvarid);
    }
  }

  diagonal_GMM_scorer::diagonal_GMM_scorer(diagonal_GMM_parameter_ptr param) 
    : _parameter(param) {
  }

  inline fmatrix bind_vectors(int dim, const std::vector<int>& ids,
                              const fmatrix& source) {
    fmatrix ret(dim, ids.size());
    for (int n = 0; n < ids.size(); ++ n) {
      ret.col(n) = source.col(ids[n]);
    }
    return ret;
  }
  inline fvector bind_scalars(const std::vector<int>& ids,
                              const fvector& source) {
    fvector ret(ids.size());
    for (int n = 0; n < ids.size(); ++ n) {
      ret(n) = source(ids[n]);
    }
    return ret;
  }

  fmatrix diagonal_GMM_parameter::means(int s) const {
    return bind_vectors(nfeatures(), _states[s].mean_ids, _means);
  }
  std::vector<int> diagonal_GMM_parameter::mean_ids(int s) const {
    return _states[s].mean_ids;
  } 

  std::vector<int> diagonal_GMM_parameter::sqrtprec_ids(int s) const {
    return _states[s].var_ids;
  }
  
 
  fmatrix diagonal_GMM_parameter::sqrtprecs(int s) const {
    return bind_vectors(nfeatures(), _states[s].var_ids, _sqrtprecs);
  }
  fvector diagonal_GMM_parameter::logweights(int s) const {
    return _states[s].logweights;
  }
  fvector diagonal_GMM_parameter::logzs(int s) const {
    return bind_scalars(_states[s].var_ids, _logzs);
  }


  void diagonal_GMM_scorer::set_frames(const fmatrix& f) {
    _input_buffer = f;
    _score_cache = fmatrix::Constant(this->nstates(), f.cols(), std::nanf(""));
  }


  float diagonal_GMM_scorer::get_score(int t, int s) {
    if (! std::isnan(_score_cache(s, t))) {
      return _score_cache(s, t);
    }
    if (t < 0 || t >= _input_buffer.cols()) 
      throw std::runtime_error("Input range error");
    if (s < 0 || s >= _parameter->nstates())
      throw std::runtime_error("State range error");

    static int batchsize = 16;
    int bs = std::min(static_cast<int>(t + batchsize),
                      static_cast<int>(_input_buffer.cols())) - t;
    size_t D = _input_buffer.rows();

    fmatrix glogll;
    _parameter->get_gaussian_scores<float>(_input_buffer.block(0, t, D, bs),
                                           _parameter->means(s),
                                           _parameter->sqrtprecs(s),
                                           _parameter->logzs(s),
                                           &glogll);
    glogll.colwise() += _parameter->logweights(s);
    _score_cache.block(s, t, 1, bs) = log_sum_exp(glogll);
    return _score_cache(s, t);
  }

  size_t diagonal_GMM_scorer::nstates() const {
    return _parameter->nstates();
  }
}

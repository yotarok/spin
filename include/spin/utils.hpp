#ifndef spin_utils_hpp_
#define spin_utils_hpp_

#include <time.h>
#include <sys/time.h>

namespace spin {

#ifndef M_LOG2
#  define M_LOG2 0.69314718055994529
#endif
#ifndef M_LOG_2PI
#  define M_LOG2PI 1.8378770664093453
#endif

  template<typename T, int sz>
  int const_array_size(T(&)[sz]) {
    return sz;
  }

  template <typename NumT>
  NumT log_sum_exp(const Eigen::Matrix<NumT, Eigen::Dynamic, 1>& v) {
    NumT maxc = v.maxCoeff();
    NumT logsum = std::log((v.array() - maxc).exp().sum());
    return logsum + maxc;
  }

  // TO DO: Should avoid construct/ copy
  template <typename NumT>
  Eigen::Matrix<NumT, 1, Eigen::Dynamic> log_sum_exp(const Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic>& m) {
    Eigen::Matrix<NumT, 1, Eigen::Dynamic>
      maxc = m.colwise().maxCoeff();
    return (m.rowwise() - maxc).array().exp().colwise().sum().log().matrix() + maxc;
  }

  template <typename NumT>
  Eigen::Matrix<NumT, Eigen::Dynamic, 1> softmax(const Eigen::Matrix<NumT, Eigen::Dynamic, 1>& v) {
    return (v.array() - log_sum_exp(v)).matrix();
  }

  template <typename It>
  class combinatorial_iterator {
    std::vector<It> _begins;
    std::vector<It> _ends;
    std::vector<It> _iterators;
  public:
    combinatorial_iterator() {
    }
    
    void add_range(It beg, It end) {
      // this function resets the current status of iterator
      _begins.push_back(beg);
      _ends.push_back(end);
      _iterators.push_back(beg);
      _iterators = _begins;
    }

    bool next(int n = 0) {
      // return true if the iterator reached to the end
      if (n >= _iterators.size() || _iterators[n] == _ends[n]) return true;
      ++ _iterators[n];
      if (_iterators[n] == _ends[n]) {
        if (! next(n + 1)) {
          _iterators[n] = _begins[n];
        }
      }
      return false;
    }

    bool done() {
      return _iterators.back() == _ends.back();
    }
    
    template <typename OutIt>
    void value(OutIt oit) {
      for (int n = 0; n < _iterators.size(); ++ n) {
        *oit = *(_iterators[n]);
        ++ oit;
      }
    }
    
    std::vector<typename It::value_type> value() {
      // copying value might be costly sometimes, take care
      std::vector<typename It::value_type> ret;
      this->value(std::back_inserter(ret));
      return ret;
    }
  };

  template <typename Container>
  combinatorial_iterator<typename Container::const_iterator>
  make_combinatorial_iterator(const Container& c, int N) {
    combinatorial_iterator<typename Container::const_iterator> comit;
    for (int n = 0; n < N; ++ n) {
      comit.add_range(c.cbegin(), c.cend());
    }
    return comit;
  }


  inline double get_wall_time() {
    struct timeval time;
    if (::gettimeofday(&time, 0)) return 0;
    return static_cast<double>(time.tv_sec)
      + static_cast<double>(time.tv_usec) / 1000000;
  }
  
  template <template<class,class,class...> class C,
            typename V, typename... Args>
  const V& get(const C<std::string, V, Args...>& m, const std::string& k) {
    auto pit = m.find(k);
    if (pit == m.cend()) {
      throw std::runtime_error(k + " is not found from the given dictionary");
    }
    return pit->second;
  }

  //template <typename T>
  template <template<class,class,class...> class C,
            typename K, typename V, typename... Args>
  const V& get_or_else(const C<K, V, Args...>& m, const K& k, const V& def) {
    auto pit = m.find(k);
    if (pit == m.cend()) {
      return def;
    }
    return pit->second;
  }
  

}

#endif

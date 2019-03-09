#ifndef spin_fstext_latticeweight_hpp_
#define spin_fstext_latticeweight_hpp_

#include <boost/lexical_cast.hpp>
#include <fst/util.h>
#include <boost/algorithm/string.hpp>
#include <spin/fstext/timingweight.hpp>

namespace spin {
  /** 
   * This weight is similar to that of lattice weight used in lattice
   * But I decided to use scaled acoustic weight so that we don't need to
   * have acoustic scale parameter in all programs manipulating lattices.
   * And, instead of having graph weight explicitly, a summed weight and 
   * acoustic model weight is used.
   */
  class LatticeWeight {
    float _weight;
    float _acweight;
    TimingWeight<int> _time;
  public:
    typedef LatticeWeight ReverseWeight;

    LatticeWeight() : _weight(0.0), _acweight(0.0), _time() {
    }

    LatticeWeight(float w, float acw, TimingWeight<int> tm)
      : _weight(w), _acweight(acw), _time(tm) {
    }

    LatticeWeight(const LatticeWeight& w)
      : _weight(w._weight), _acweight(w._acweight), _time(w._time) {
    }

    LatticeWeight& operator = (const LatticeWeight& w) {
      _weight = w._weight;
      _acweight = w._acweight;
      _time = w._time;
      return *this;
    }

    static const LatticeWeight& Zero() {
      static const LatticeWeight ZERO(std::numeric_limits<float>::infinity(),
                                      0.0, TimingWeight<int>::Zero());
      return ZERO;
    }

    static const LatticeWeight& One() {
      static const LatticeWeight ONE(0.0, 0.0, TimingWeight<int>::One());
      return ONE;
    }
    
    static const LatticeWeight& NoWeight() {
      static const LatticeWeight NW(std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN(),
                                    TimingWeight<int>::NoWeight());
      return NW;
    }

    bool Member() const {
      return _weight == _weight && _acweight == _acweight && // NaN check
        _weight != - std::numeric_limits<float>::infinity() &&
        _acweight != - std::numeric_limits<float>::infinity() &&
        _time.Member();
    }

    static std::string Type() {
      return "spinlattice";
    }

    std::istream& Read(std::istream& is) {
      fst::ReadType(is, &_weight);
      fst::ReadType(is, &_acweight);
      return _time.Read(is);
    }

    std::ostream& Write(std::ostream& os) const {
      fst::WriteType(os, _weight);
      fst::WriteType(os, _acweight);
      return _time.Write(os);
    }

    size_t Hash() const {
      union { float f; size_t s; } u;
      u.s = 0; u.f = _weight;
      union { float f; size_t s; } v;
      v.s = 0; v.f = _acweight;
      size_t ret = (u.s << 5) ^ ( u.s >> 27) ^ v.s;
      ret ^= _time.Hash();
      return ret;
    }

    LatticeWeight Quantize(float delta = fst::kDelta) const {
      return LatticeWeight(floor(_weight / delta + 0.5F) * delta,
                           floor(_acweight / delta + 0.5F) * delta,
                           _time);
    }


    LatticeWeight Reverse() const { return *this; }

    static uint64 Properties() {
      return fst::kLeftSemiring | fst::kRightSemiring | fst::kCommutative
        | fst::kIdempotent | fst::kPath;
    }

    float Value() const { return _weight; }
    float AcousticWeight() const { return _acweight; }
    //const std::vector<int> StateIDs() const { return _sids; }
    TimingWeight<int> Time() const { return _time; }
    
  };

  inline
  bool operator ==(const LatticeWeight& w1,
                   const LatticeWeight& w2) {
    return w1.Value() == w2.Value()
      && w1.AcousticWeight() == w2.AcousticWeight()
      && w1.Time() == w2.Time();
  }

  inline
  bool operator !=(const LatticeWeight& w1,
                   const LatticeWeight& w2) {
    return ! (w1 == w2);
  }

  inline
  bool ApproxEqual(const LatticeWeight& w1,
                   const LatticeWeight& w2,
                   float delta = fst::kDelta) {
    return w1.Quantize(delta) == w2.Quantize(delta);
  }

  
  inline std::ostream& operator << (std::ostream& os, 
                                    const LatticeWeight& w) {
    os << "(" << w.Value() << "," << w.AcousticWeight() << "," << w.Time() << ")";
    return os;
  }

  inline std::istream& operator >> (std::istream& is,
                                    LatticeWeight& w) {
    std::string line;
    std::getline(is, line, ')');
    if (line[0] != '(')
      throw std::runtime_error("lattice weight format error");
    line = line.substr(1);
    std::vector<std::string> vals;
    boost::algorithm::split(vals, line, boost::is_any_of(","));
    float val = boost::lexical_cast<float>(vals[0]),
      acw = boost::lexical_cast<float>(vals[1]);
    TimingWeight<int> tmw = boost::lexical_cast<TimingWeight<int> >(vals[2]);
    w = LatticeWeight(val, acw, tmw);
    return is;
  }

  inline
  LatticeWeight Times(const LatticeWeight & w1,
                      const LatticeWeight & w2) {
    return LatticeWeight(w1.Value() + w2.Value(),
                         w1.AcousticWeight() + w2.AcousticWeight(),
                         Times(w1.Time(), w2.Time()));
  }

  inline
  LatticeWeight Plus(const LatticeWeight & w1,
                     const LatticeWeight & w2) {
    if (w1.Value() < w2.Value()) {
      return w1;
    } else {
      return w2;
    }
  }

  inline
  LatticeWeight Divide(const LatticeWeight & w1,
                       const LatticeWeight & w2,
                       fst::DivideType = fst::DIVIDE_ANY) {
    return LatticeWeight::NoWeight();
    // TO DO: We could define appropriate devide
  }

}

namespace fst {
  template <>
  struct WeightConvert<spin::LatticeWeight, TropicalWeight> {
    TropicalWeight operator()(spin::LatticeWeight w) const {
      return TropicalWeight(w.Value());
    }
  };

  template <>
  struct WeightConvert<TropicalWeight, spin::LatticeWeight> {
    spin::LatticeWeight operator()(TropicalWeight w) const {
      return spin::LatticeWeight(w.Value(), 0.0,
                                 spin::TimingWeight<int>::One());
    }
  };

}

#endif

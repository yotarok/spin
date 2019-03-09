#ifndef spin_fstext_timingweight_hpp_
#define spin_fstext_timingweight_hpp_

#include <boost/lexical_cast.hpp>
#include <fst/util.h>

namespace spin {
  template <typename T>
  class TimingWeight {
    
    T _start;
    T _end;
  public:
    typedef TimingWeight<T> ReverseWeight;

    TimingWeight() : _start(std::numeric_limits<T>::max()), _end(0) {
    }
    TimingWeight(T s, T e) : _start(s), _end(e) {
    }
    TimingWeight(const TimingWeight<T>& w) : _start(w._start), _end(w._end) {
    }

    TimingWeight<T>& operator=(const TimingWeight<T>& w) {
      this->_start = w._start;
      this->_end = w._end;
      return *this;
    }


    static const TimingWeight<T>& Zero() { 
      static const TimingWeight ZERO(0, std::numeric_limits<T>::max());
      return ZERO; 
    }
    static const TimingWeight<T>& One() { 
      static const TimingWeight ONE(std::numeric_limits<T>::max(), 0);    
      return ONE; 
    }
    static const TimingWeight<T>& NoWeight() { 
      static const TimingWeight NOWEIGHT(0, 0);
      return NOWEIGHT; 
    }

    bool Member() const {
      return _start == _start && _end == _end;
    }

    static std::string Type() { // TO DO: Do meta-programming!
      std::string type = "timing";
      if (std::numeric_limits<T>::is_integer) {
        type += "Int";
      } else {
        type += "Float";
      }
      type += boost::lexical_cast<std::string>(sizeof(T));
      return type;
    }

    std::istream& Read(std::istream& is) {
      fst::ReadType(is, &_start);
      return fst::ReadType(is, &_end);
    }

    std::ostream& Write(std::ostream& os) const {
      fst::WriteType(os, _start);
      return fst::WriteType(os, _end);
    }

    size_t Hash() const {
      union { T f; size_t s; } u_s, u_e;
      u_s.s = 0; u_e.s = 0;
      u_s.f = _start; u_e.f = _end;
      return (u_s.s << 5) ^ (u_s.s >> 27) ^ u_e.s;
    }

    TimingWeight<T> Quantize(float delta = fst::kDelta) const {
      if (std::numeric_limits<T>::is_integer) {
        return *this;
      } else {
        return TimingWeight(floor(_start / delta + 0.5F) * delta,
                            floor(_end / delta + 0.5F) * delta);
      }
    }
    
    TimingWeight<T> Reverse() const { return *this; }
    
    static uint64 Properties() {
      return fst::kLeftSemiring | fst::kRightSemiring | fst::kCommutative
        | fst::kIdempotent;
    }

    const T& Start() const { return _start; }
    const T& End() const { return _end; }
  };

  template <typename T>
  bool operator ==(const TimingWeight<T>& w1,
                   const TimingWeight<T>& w2) {
    return w1.Start() == w2.Start() && w1.End() == w2.End();
  }

  template <typename T>
  bool operator !=(const TimingWeight<T>& w1,
                   const TimingWeight<T>& w2) {
    return w1.Start() != w2.Start() || w1.End() != w2.End();
  }

  template <typename T>
  bool ApproxEqual(const TimingWeight<T>& w1,
                   const TimingWeight<T>& w2,
                   float delta = fst::kDelta) {
    return w1.Quantize(delta) == w2.Quantize(delta);
  }

  template <typename T>
  inline std::ostream& operator << (std::ostream& os, 
                                    const TimingWeight<T>& w) {
    os << "[" << w.Start() << ":" << w.End() << "]" ;
    return os;
  }

  template <typename T>
  inline std::istream& operator >> (std::istream& is,
                                    TimingWeight<T>& w) {
    std::string s;
    int c;
    do {
      c = is.get();
    } while (std::isspace(c));
    if (c != '[') {
      FSTERROR() << " format error, expected '[', but got " << (char) c << std::endl;
      is.clear(std::ios::failbit);
      return is;
    }
    std::string s1, s2;
    c = is.get();
    while (c != ':') {
      s1 += (char) c;
      c = is.get();
    }
    c = is.get();
    while (c != ']') {
      s2 += (char) c;
      c = is.get();
    }

    w = TimingWeight<T>(boost::lexical_cast<T>(s1),
                        boost::lexical_cast<T>(s2));
    return is;
  }


  template <typename T>
  TimingWeight<T> Times(const TimingWeight<T> & w1,
                        const TimingWeight<T> & w2) {
    return TimingWeight<T>(std::min(w1.Start(), w2.Start()),
                           std::max(w1.End(), w2.End()));
  }

  template <typename T>
  TimingWeight<T> Plus(const TimingWeight<T> & w1,
                       const TimingWeight<T> & w2) {
    return TimingWeight<T>(std::max(w1.Start(), w2.Start()),
                           std::min(w1.End(), w2.End()));
  }

  template <typename T>
  TimingWeight<T> Divide(const TimingWeight<T> & w1,
                         const TimingWeight<T> & w2,
                         fst::DivideType = fst::DIVIDE_ANY) {
    return TimingWeight<T>::NoWeight();
  }



}

#endif

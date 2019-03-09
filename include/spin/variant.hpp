#ifndef spin_variant_hpp_
#define spin_variant_hpp_

#include <spin/types.hpp>
#include <boost/variant.hpp>
#include <map>
#include <vector>
#include <gear/io/fileformat.hpp>
#include <gear/io/matrix.hpp>

namespace spin {
  // variant is base building block of corpus
  // object represented in this type is serializable via Yaml or msgpack

  struct nil_t { };

  struct ext_ref {
    std::string loc;
    std::string format;

    ext_ref(const std::string& u, const std::string& f)
      : loc(u), format(f) { }
    ext_ref() { }
  };
  
  inline bool operator == (const ext_ref& v1, const ext_ref& v2) {
    return v1.loc == v2.loc && v1.format == v2.format;
  }
  
  inline std::ostream& operator << (std::ostream& os, const ext_ref& v) {
    os << v.loc << "[" << v.format << "]";
    return os;
  }

  enum VARIANT_TYPES {
    VARIANT_NIL = 0,
    VARIANT_MAP,
    VARIANT_VECTOR,
    VARIANT_BOOL,
    VARIANT_INT,
    VARIANT_DOUBLE,
    VARIANT_STRING,
    VARIANT_FST,
    VARIANT_FMATRIX,
    VARIANT_DMATRIX,
    VARIANT_INTMATRIX,
    VARIANT_EXTREF
  };

  typedef boost::make_recursive_variant<
    nil_t,
    std::map<std::string, boost::recursive_variant_>, 
    std::vector<boost::recursive_variant_>, 
    bool,
    int, 
    double, 
    std::string, 
    vector_fst, 
    fmatrix, 
    dmatrix, 
    intmatrix,
    ext_ref>::type variant_t;

  typedef std::map<std::string, variant_t> variant_map;
  typedef std::vector<variant_t> variant_vector;


  template <typename ValT>
  const ValT& get_prop(const variant_map& m, const std::string& key) {
    variant_map::const_iterator it = m.find(key);
    if (it == m.end()) {
      throw std::runtime_error("Cannot read data: Key " + key + " not found");
    }
    try {
      return boost::get<ValT>(it->second);
    } catch(boost::bad_get) {
      throw std::runtime_error("Cannot read data: Data type for key " + key + " is not consistent");
    }
  }

  template <typename ValT>
  const ValT& get_prop(const variant_t& m, const std::string& key) {
    return get_prop<ValT>(boost::get<variant_map>(m), key);
  }

}
#endif

#ifndef spin_types_hpp_
#define spin_types_hpp_

#include <memory>
#include <Eigen/Eigen>
#include <map>
#include <fst/script/fst-class.h>
#include <boost/shared_ptr.hpp>

namespace spin {
  typedef Eigen::MatrixXf fmatrix;
  typedef Eigen::VectorXf fvector;
  typedef Eigen::MatrixXd dmatrix;
  typedef Eigen::VectorXd dvector;
  typedef Eigen::MatrixXi intmatrix;
  typedef Eigen::VectorXi intvector;

  typedef fst::script::FstClass fst_t;
  typedef fst::script::VectorFstClass vector_fst;
  typedef boost::shared_ptr<fst::script::VectorFstClass> vector_fst_ptr;


  template <typename Map>
  const typename Map::mapped_type& get_or_default(const Map& m,
                                                  const typename Map::key_type& key,
                                                  const typename Map::mapped_type& def) {
    typename Map::const_iterator it = m.find(key);
    if (it == m.end()) {
      return def;
    }
    return it->second;
  }
  
}

#endif

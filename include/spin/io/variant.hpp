#ifndef spin_io_variant_hpp_
#define spin_io_variant_hpp_

#include <spin/types.hpp>
#include <spin/io/file.hpp>
#include <spin/io/yaml.hpp>
#include <spin/io/msgpack.hpp>

namespace spin {
  void load_variant(variant_t* dest, const std::string& path);
  void load_variant(variant_t* dest, std::string* pmagic, const std::string& path);
  
  void write_variant(const variant_t& src,
                     const std::string& path,
                     bool text,
                     const std::string& magic);

  // Read a matrix and applies a cast operator if necessary
  void read_fmatrix(fmatrix* dest, const variant_t& v);

  template <typename NumT>
  void read_matrix(Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic>* dest,
                   const variant_t& v) {
    *dest = boost::get<Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic> >(v);
  }

  template <>
  inline
  void read_matrix(Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic>* dest,
                   const variant_t& v) {
    read_fmatrix(dest, v);
  }
  
}

#endif

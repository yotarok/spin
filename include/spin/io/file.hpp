#ifndef spin_io_file_hpp_
#define spin_io_file_hpp_

#include <spin/types.hpp>

namespace spin {
  bool check_binary_header(const std::string& path);
  void make_directories(const std::string& path);
}

#endif

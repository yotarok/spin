#include <spin/types.hpp>
#include <spin/io/file.hpp>

#include <sys/stat.h>

namespace spin {
  bool check_binary_header(const std::string& path) {
    std::ifstream ifs(path.c_str());

    char magic[9]; magic[8] = '\0';
    ifs.read(magic, 8);
    return std::string(magic).substr(0,4) == "Stac" || std::string(magic).substr(0,4) == "Spin";
  }
  
  void make_directories(const std::string& path) {
    ::mkdir(path.c_str(), 0777);
  }
}

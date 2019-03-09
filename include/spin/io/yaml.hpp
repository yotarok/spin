#ifndef spin_io_yaml_hpp_
#define spin_io_yaml_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <yaml-cpp/yaml.h>
#include <boost/variant.hpp>
#include <fst/script/print.h>
#include <spin/io/fst.hpp>
#include <boost/format.hpp>


namespace spin {

  // TO DO: Unify with gear::read_yaml_matrix
  template <typename NumT>
  Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic> 
  convert_to_matrix(const YAML::Node& node, bool transpose = false) {
    Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic> ret;
      
    bool inited = false;
    for (int row = 0; row < node.size(); ++ row) {
      YAML::Node rownode = node[row];
      for (int col = 0; col < rownode.size(); ++ col) {
        if (! inited) {
          ret.resize(node.size(), rownode.size());
          inited = true;
        }
        ret(row, col) = rownode[col].as<NumT>();
      }
    }
    if (transpose) {
      return ret.transpose();
    } else {
      return ret;
    }
  }

  inline YAML::Node make_node_from_variant(const variant_t& data);

  template <typename NumT>
  const char* get_matrix_typename() { throw std::runtime_error("not supported"); }

  template <>
  inline const char* get_matrix_typename<float>() {
    return "fmatrix";
  }
  template <>
  inline const char* get_matrix_typename<double>() {
    return "dmatrix";
  }
  template <>
  inline const char* get_matrix_typename<int>() {
    return "intmatrix";
  }

  struct yaml_node_maker : boost::static_visitor<YAML::Node> {
    template<typename T>
    YAML::Node operator()(const T& val) const {
      return YAML::Node(val);
    }

    YAML::Node operator()(const double& val) const {
      return YAML::Node((boost::format("%e") % val).str());
    }

    YAML::Node operator()(const std::string& val) const {
      YAML::Node ret(val);
      ret.SetTag("tag:yaml.org,2002:str"); // ugly...
      return ret;
    }

    YAML::Node operator()(const nil_t& val) const {
      return YAML::Load("~");
    }

    YAML::Node operator()(const ext_ref& val) const {
      YAML::Node ret;
      ret["type"] = "ref";
      ret["loc"] = val.loc;
      ret["format"] = val.format;
      return ret;
    }

    YAML::Node operator()(const std::vector<variant_t>& val) const {
      YAML::Node ret;
      for (std::vector<variant_t>::const_iterator 
             it = val.begin(), last = val.end(); it != last; ++ it) {
        ret.push_back(make_node_from_variant(*it));
      }
      return ret;
    }

    YAML::Node operator()(const std::map<std::string, variant_t>& val) const {
      YAML::Node ret;
      for (std::map<std::string, variant_t>::const_iterator 
             it = val.begin(), last = val.end(); it != last; ++ it) {
        std::string key = it->first;
        ret[key] = make_node_from_variant(it->second);
      }
      return ret;
    }

    YAML::Node operator()(const fst::script::VectorFstClass& val) const {
      YAML::Node ret;
      ret["type"] = "fst";
      std::ostringstream oss;
      
      print_fst(oss, val);
      //fst::script::PrintFst(val, oss, "", val.InputSymbols(), val.OutputSymbols(),
      //                    0, false, true);
                            
      ret["data"] = oss.str();
      return ret;
    }

    template <typename NumT>
    YAML::Node 
    operator()(const Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic>& val)
      const {
      YAML::Node ret;
      bool transpose = true;
      ret["type"] = get_matrix_typename<NumT>();
      YAML::Node data;
      if (transpose) {
        for (int col = 0; col < val.cols(); ++ col) {
          YAML::Node line;
          for (int row = 0; row < val.rows(); ++ row) {
            line.push_back(val(row, col));
          }
          data.push_back(line);
        }

        ret["transpose"] = true;
      } else {
        
        for (int row = 0; row < val.rows(); ++ row) {
          YAML::Node line;
          for (int col = 0; col < val.cols(); ++ col) {
            line.push_back(val(row, col));
        }
          data.push_back(line);
        }
      }
      ret["data"] = data;
      return ret;
    }

    
  };

  inline YAML::Node make_node_from_variant(const variant_t& data) {
    return boost::apply_visitor(yaml_node_maker(), data);
  }
  
  inline variant_t convert_to_variant(const YAML::Node& node) {
    if (node.Type() == YAML::NodeType::Null) return nil_t();
    else if (node.Type() == YAML::NodeType::Map) {
      if (! node["type"]) {
        std::map<std::string, variant_t> ret;
        for (YAML::Node::const_iterator it = node.begin(), last = node.end();
             it != last; ++ it) {
          std::string keyname = it->first.as<std::string>();
          ret.insert(std::make_pair(keyname,
                                    convert_to_variant(it->second)));
        }
        return ret;
      } else {        
        std::string type = node["type"].as<std::string>();
        if (type == "ref") {
          return ext_ref(node["loc"].as<std::string>(), 
                         node["format"].as<std::string>());
        } else if (type == "fst") {
          std::string 
            data = node["data"].as<std::string>();
          std::istringstream iss(data);
          fst::script::VectorFstClass fst = make_fst(iss);
          return fst;
        } else if (type == "fmatrix") {
          bool tr = node["transpose"] && node["transpose"].as<bool>();
          return convert_to_matrix<float>(node["data"], tr);
        } else if (type == "dmatrix") {
          bool tr = node["transpose"] && node["transpose"].as<bool>();
          return convert_to_matrix<double>(node["data"], tr);
        } else if (type == "intmatrix") {
          bool tr = node["transpose"] && node["transpose"].as<bool>();
          return convert_to_matrix<int>(node["data"], tr);
        }
        else {
          throw std::runtime_error((boost::format("unknown type [%1%]") % type).str());
        }
      }
    }
    else if (node.Type() == YAML::NodeType::Sequence) { 
      std::vector<variant_t> ret;
      size_t siz = node.size();
      for (size_t i = 0; i < siz; ++ i) {
        ret.push_back(convert_to_variant(node[i]));
      }
      return ret;
    }
    else if (node.Type() == YAML::NodeType::Scalar) {
      if (node.Tag() == "!" || // YAML-cpp stores "!" Tag if value is quoted
          node.Tag() == "tag:yaml.org,2002:str") { 
        return node.as<std::string>();
      }

      try {
        bool b = node.as<bool>();
        return b;
      } catch(std::exception& e) {
      }
      try {
        int i = node.as<int>();
        return i;
      } catch(std::exception& e) {
      }
      try {
        double d = node.as<double>();
        return d;
      } catch(std::exception& e) {
      }
      std::string s = node.as<std::string>();
      if (s == "~") return nil_t(); // TO DO: is it necessary?
      return s;
    }
    throw std::runtime_error("unknown error");
  }
}

#endif

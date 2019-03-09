#include <spin/io/variant.hpp>

namespace spin {
  void load_variant(variant_t* dest, const std::string& path) {
    load_variant(dest, 0, path);
  }
  void load_variant(variant_t* dest, std::string* pmagic, const std::string& path) {
    if (check_binary_header(path)) {
      std::ifstream ifs(path);
      uint64 siz;
      char magic[9]; magic[8] = '\0';
      ifs.read(magic, 8);
      ifs.read(reinterpret_cast<char*>(&siz), sizeof(siz));
      if (pmagic) *pmagic = magic;
      char* data = new char[siz];
      ifs.read(data, siz);
      msgpack::unpacked msg;
      msgpack::unpack(msg, data, siz);
      msg.get().convert(dest);
      delete[] data;
    } else {
      std::ifstream ifs(path);
      if (ifs.peek() == '#') { // Retrieve header comment
        std::string line;
        std::getline(ifs, line);

        std::map<std::string, std::string> headerinfo;
        std::vector<std::string> kvs;
        boost::algorithm::split(kvs, line, boost::is_any_of(" \t"));
        for (auto it = kvs.cbegin(), last = kvs.cend(); it != last; ++ it) {
          size_t pos = it->find('=');
          if (pos == std::string::npos) {
            throw std::runtime_error("Header is not in KEY=VALUE format");
          }
          headerinfo[it->substr(0, pos)] = it->substr(pos+1);
        }

        if (pmagic && headerinfo.find("magic") != headerinfo.end()) {
          *pmagic = headerinfo["magic"];
        }
      }
      *dest = convert_to_variant(YAML::Load(ifs));
    }
  }

  void write_variant(const variant_t& src,
                     const std::string& path,
                     bool text,
                     const std::string& magic) {
    if (text) {
      std::ofstream ofs(path);
      ofs << "#magic=" << magic << std::endl;
      ofs << make_node_from_variant(src) << std::endl;
    } else {
      std::ostringstream buf;
      msgpack::pack(buf, src);
      uint64 siz = buf.str().size();

      std::ofstream ofs(path);
      ofs.write(magic.c_str(), 8);
      ofs.write(reinterpret_cast<const char*>(&siz), sizeof(siz));
      ofs.write(buf.str().c_str(), siz);
    }
  }

  void read_fmatrix(fmatrix* dest, const variant_t& v) {
    if (v.which() == VARIANT_FMATRIX) {
      *dest = boost::get<fmatrix>(v);
    } else if (v.which() == VARIANT_DMATRIX) {
      *dest = boost::get<dmatrix>(v).cast<float>();
    } else if (v.which() == VARIANT_INTMATRIX) {
      *dest = boost::get<intmatrix>(v).cast<float>();
    } else if (v.which() == VARIANT_EXTREF) {
      ext_ref ref = boost::get<ext_ref>(v);
      gear::content_type type(ref.format);
      gear::read_fmatrix(dest, ref.loc, type);
    }               

  }


}

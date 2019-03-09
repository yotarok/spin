#ifndef spin_io_msgpack_hpp_
#define spin_io_msgpack_hpp_

#include <msgpack_fwd.hpp>
#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <msgpack/adaptor/nil_fwd.hpp>
#include <spin/io/fst.hpp>

inline const msgpack::type::nil& get_nil();

namespace msgpack {
  enum spin_msgpack_typeid {
    STAC_FST,
    STAC_FMATRIX,
    STAC_DMATRIX,
    STAC_INTMATRIX,
    STAC_REF,
  };

  MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
    // Convert from msgpacl::object to variant_t.
    inline msgpack::object const& operator>>(object const& o, spin::variant_t& v) {
      switch(o.type) {
      case type::NIL:
        v = spin::nil_t();
        break;        
      case type::BOOLEAN:
        v = bool();
        o.convert(boost::get<bool>(&v));
        break;
      case type::MAP:
        v = std::map<std::string, spin::variant_t>();
        o.convert(boost::get<std::map<std::string, spin::variant_t> >(&v));
        break;
      case type::ARRAY:
        v = std::vector<spin::variant_t>();
        o.convert(boost::get<std::vector<spin::variant_t> >(&v));
        break;
      case type::POSITIVE_INTEGER:
      case type::NEGATIVE_INTEGER:
        v = int();
        o.convert(boost::get<int>(&v));
        break;
      case type::FLOAT:
        v = double();
        o.convert(boost::get<double>(&v));
        break;
      case type::STR:
      case type::BIN:
        v = std::string();
        o.convert(boost::get<std::string>(&v));
        break;
      case type::EXT: {
        size_t siz = o.via.ext.size;
        std::string buf(o.via.ext.data(), siz);
        std::istringstream iss(buf);
        switch(o.via.ext.type()) {
        case STAC_FST: {
          //fst::FstReadOptions opt;
          //fst::script::FstClass* p = fst::script::FstClass::Read(iss, opt);
          v = spin::read_fst(iss);
          break;
        }
        case STAC_FMATRIX: {
          uint64_t row, col;
          iss.read(reinterpret_cast<char*>(&row), sizeof(row));
          iss.read(reinterpret_cast<char*>(&col), sizeof(col));
          v = spin::fmatrix(row, col);
          iss.read(reinterpret_cast<char*>(boost::get<spin::fmatrix>(v).data()),
                   row * col * sizeof(float));
          break;
        }
        case STAC_DMATRIX: {
          uint64_t row, col;
          iss.read(reinterpret_cast<char*>(&row), sizeof(row));
          iss.read(reinterpret_cast<char*>(&col), sizeof(col));
          v = spin::dmatrix(row, col);
          iss.read(reinterpret_cast<char*>(boost::get<spin::dmatrix>(v).data()),
                   row * col * sizeof(double));
          break;
        }
        case STAC_INTMATRIX: {
          uint64_t row, col;
          iss.read(reinterpret_cast<char*>(&row), sizeof(row));
          iss.read(reinterpret_cast<char*>(&col), sizeof(col));
          v = spin::intmatrix(row, col);
          iss.read(reinterpret_cast<char*>(boost::get<spin::intmatrix>(v).data()),
                   row * col * sizeof(int));          
          break;
        }
        case STAC_REF: {
          spin::ext_ref ref;
          std::getline(iss, ref.loc);
          std::getline(iss, ref.format);
          v = ref;
          break;
        }
        default:
          break;
        }
        break;
      }
      default:
        break;
      }
      return o;
    }
    
    template <typename Stream>
    struct packer_imp : boost::static_visitor<void> {
      template <typename T>
      void operator()(T const& value) const {
        o_.pack(value);
      }

      void operator()(fst::script::VectorFstClass const& value) const {
        std::ostringstream oss;
        fst::FstWriteOptions opt;
        value.Write(oss, opt);
        o_.pack_ext(oss.str().size(), STAC_FST);
        o_.pack_ext_body(oss.str().c_str(), oss.str().size());
      }

      void operator()(spin::fmatrix const& value) const {
        std::ostringstream oss;
        uint64_t row = value.rows(), col = value.cols();
        oss.write(reinterpret_cast<const char*>(&row), sizeof(row));
        oss.write(reinterpret_cast<const char*>(&col), sizeof(col));
        oss.write(reinterpret_cast<const char*>(value.data()),
                  sizeof(float) * row * col);
        o_.pack_ext(oss.str().size(), STAC_FMATRIX);
        o_.pack_ext_body(oss.str().c_str(), oss.str().size());
      }

      void operator()(spin::dmatrix const& value) const {
        std::ostringstream oss;
        uint64_t row = value.rows(), col = value.cols();
        oss.write(reinterpret_cast<const char*>(&row), sizeof(row));
        oss.write(reinterpret_cast<const char*>(&col), sizeof(col));
        oss.write(reinterpret_cast<const char*>(value.data()),
                  sizeof(double) * row * col);
        o_.pack_ext(oss.str().size(), STAC_DMATRIX);
        o_.pack_ext_body(oss.str().c_str(), oss.str().size());
      }

      void operator()(spin::intmatrix const& value) const {
        std::ostringstream oss;
        uint64_t row = value.rows(), col = value.cols();
        oss.write(reinterpret_cast<const char*>(&row), sizeof(row));
        oss.write(reinterpret_cast<const char*>(&col), sizeof(col));
        oss.write(reinterpret_cast<const char*>(value.data()),
                  sizeof(int) * row * col);
        o_.pack_ext(oss.str().size(), STAC_INTMATRIX);
        o_.pack_ext_body(oss.str().c_str(), oss.str().size());
      }

      void operator()(spin::ext_ref const& value) const {
        std::ostringstream oss;
        oss << value.loc << std::endl << value.format << std::endl;
        o_.pack_ext(oss.str().size(), STAC_REF);
        o_.pack_ext_body(oss.str().c_str(), oss.str().size());        
      }

      void operator()(spin::nil_t const& value) const {
        o_.pack(get_nil());
      }


      packer_imp(packer<Stream>& o):o_(o) {}
      packer<Stream>& o_;
    };
    
    template <typename Stream>
    inline msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const spin::variant_t& v) {
      boost::apply_visitor(packer_imp<Stream>(o), v);
      return o;
    }
  }
}

#include <msgpack.hpp>

inline const msgpack::type::nil& get_nil() {
  static const msgpack::type::nil n = {};
  return n;
}


#endif

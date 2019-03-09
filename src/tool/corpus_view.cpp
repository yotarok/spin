#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <spin/corpus/corpus.hpp>
#include <spin/io/fst.hpp>
#include <spin/io/yaml.hpp>
#include <spin/fst/linear.hpp>
#include <spin/io/file.hpp>
//#include <boost/process.hpp>
//#include <subprocess.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <fst/script/print.h>
#include <fst/script/draw.h>

namespace spin {
  
  DEFINE_ARGCLASS(arg_type, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output, 
                   ("o", "output", "", true, "", "DIRECTORY"))
                  );

  void dump_variant(std::ostream& os, const variant_t& item);

  void escape_string(std::string* ps) {
    boost::replace_all(*ps, "\\", "\\\\");
    boost::replace_all(*ps, "\n", "\\n");
    boost::replace_all(*ps, "\t", "\\t");
    boost::replace_all(*ps, "\"", "\\\"");
  }
  
  void dump_FST(std::ostream& os, const vector_fst& fst) {
    os <<
      "{"
      "\"type\": \"fst\","
      "\"src\":\"";
    {
      std::ostringstream oss;
      fst::script::PrintFst(fst, oss, "",
                            fst.InputSymbols(), fst.OutputSymbols(),
                            0, false, true);
      std::string buf = oss.str();
      escape_string(&buf);
      os << buf;
    }
    os << "\"}";
  }

  template <typename NumT>
  void dump_matrix(std::ostream& os,
                   const Eigen::Matrix<NumT, Eigen::Dynamic, Eigen::Dynamic>& mat
                   ) {
    os <<
      "{"
      "\"type\": \"matrix\","
      "\"data\":";

    for (int row = 0; row < mat.rows(); ++ row) {
      if (row == 0) os << '[';
      else os << ',';
      for (int col = 0; col < mat.cols(); ++ col) {
        if (col == 0) os << '[';
        else os << ',';
        os << mat(row, col);
      }
      os << ']';
    }
    os << "]}";
  }
  
  void dump_map(std::ostream& os, const variant_map& item) {
    int n = 0;
    for (auto it = item.cbegin(), last = item.cend(); it != last; ++ it, ++ n) {
      if (n == 0) os << "{";
      else os << ",";
      os << "\"" << it->first << "\":";
      dump_variant(os, it->second);
    }
    os << "}";
  }

  void dump_vector(std::ostream& os, const variant_vector& item) {
    int n = 0;
    for (auto it = item.cbegin(), last = item.cend(); it != last; ++ it, ++ n) {
      if (n == 0) os << "[";
      else os << ",";
      dump_variant(os, *it);
    }
    os << "]";
  }

  void dump_variant(std::ostream& os, const variant_t& item) {
    switch (item.which()) {
    case VARIANT_NIL:
      { os << "null"; break; }
    case VARIANT_MAP:
      { dump_map(os, boost::get<variant_map>(item)); break; }
    case VARIANT_VECTOR:
      { dump_vector(os, boost::get<variant_vector>(item)); break; }
    case VARIANT_BOOL:
      { os << (boost::get<bool>(item) ? "true" : "false"); break; }
    case VARIANT_INT:
      { os << boost::get<int>(item); break; }
    case VARIANT_DOUBLE:
      { os << boost::get<double>(item); break; }
    case VARIANT_STRING:
      {
        std::string s = boost::get<std::string>(item);
        escape_string(&s);
        os << "\"" << s << "\"";
        break;
      }
    case VARIANT_FST:
      { dump_FST(os, boost::get<vector_fst>(item)); break; }
    case VARIANT_FMATRIX:
      { dump_matrix(os, boost::get<fmatrix>(item)); break; }
    case VARIANT_DMATRIX:
      { dump_matrix(os, boost::get<dmatrix>(item)); break; }
    case VARIANT_INTMATRIX:
      { dump_matrix(os, boost::get<intmatrix>(item)); break; }
    case VARIANT_EXTREF:
      {
        ext_ref ref = boost::get<ext_ref>(item);
        os << "{\"loc\":" << ref.loc << "\",\"format\":\""
           << ref.format << "\"}";
        break;
      }
    default:
      throw std::runtime_error("Unknown variant type");
    }
  }

  void write_resource(const std::string& dir, const std::string& name,
                      const char* buf) {
    std::string path = dir + "/" + name;
    std::ofstream ofs(path);
    ofs << buf;
  }

  namespace resource {
    extern const char* corpus_view_css;
    extern const char* corpus_item_view_js;
  }

  int tool_main(arg_type& arg, int argc, char* argv[]) {
    corpus_iterator_ptr cit = make_corpus_iterator(arg.input.getValue());

    make_directories(arg.output.getValue());
    write_resource(arg.output.getValue(), "corpus_view.css",
                   resource::corpus_view_css);
    write_resource(arg.output.getValue(), "corpus_item_view.js",
                   resource::corpus_item_view_js);
    std::ofstream ofs_index(arg.output.getValue() + "/keys.js");
    std::ofstream ofs_index_html(arg.output.getValue() + "/index.html");
    ofs_index_html << "<html><head></head><body><ul>";
    ofs_index << "list_items=";
    for (int n = 0; ! cit->done() ; cit->next(), ++ n) {
      INFO("Processing %s", cit->get_key().c_str());

      if (n == 0) {
        ofs_index << "[";
      } else {
        ofs_index << ",";
      }

      std::string item_fn = boost::lexical_cast<std::string>(n);
      std::string js_path = arg.output.getValue() + "/" + item_fn + ".js";
      std::string html_path = arg.output.getValue() + "/" + item_fn + ".html";
      ofs_index_html << "<li><a href=\"" << item_fn << ".html\">"
                     << cit->get_key() << "</a></li>";
      
      ofs_index << "[\"" << cit->get_key().c_str() << "\", \""
                << item_fn << "\"]";

      std::ofstream ofs_item(js_path);
      ofs_item << "item_data=";
      corpus_entry item = cit->value();
      dump_map(ofs_item, item);

      std::ofstream ofs_item_html(html_path);
      ofs_item_html
        << "<html><head>"
        << "<link rel=\"stylesheet\" type=\"text/css\" href=\"corpus_view.css\">"
        << "</head>"
        << "<script src=\"corpus_item_view.js\"></script>"
        << "<script src=\"" << item_fn << ".js\"></script>"
        << "<body></body>"
        << "</html>";
    }
    ofs_index << "]";
    ofs_index_html << "</ul></body></html>";

    INFO("DONE");
    return 0;
  }

}
int main(int argc, char* argv[]) {
  return gear::wrap_main("Make HTML-based corpus viewer",
                          argc, argv, spin::tool_main);
}

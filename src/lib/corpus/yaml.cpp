#include <spin/corpus/yaml.hpp>
#include <spin/io/yaml.hpp>

namespace spin {
  yaml_corpus_iterator::yaml_corpus_iterator(const std::string& filepath,
                                             corpus_pos_t pos)
    : _curpos(pos), _nextpos(pos) {
    _input_stream = new std::ifstream(filepath);
    _input_stream->seekg(pos, std::ios_base::beg);
    _buffer = "\n";
    _done = false;
    std::string s;
    this->load_document(&s); // skip empty document
    this->next();
  }

  yaml_corpus_iterator::yaml_corpus_iterator(std::istream* is)
    : _input_stream(is), _curpos(0), _nextpos(0) {
    _buffer = "\n";
    _done = false;
    std::string s;
    this->load_document(&s); // skip empty document
    this->next();
  }

  yaml_corpus_iterator::~yaml_corpus_iterator() {
    //delete _parser;
    if (_input_stream) delete _input_stream;
  }

  bool yaml_corpus_iterator::done() {
    return _done; //! _input_stream;
  }

  void yaml_corpus_iterator::load_document(std::string* doc) {
    size_t bufsiz = 1024;
    std::string buf(bufsiz, '\0');

    if (_input_stream) {
      // fill from stream until boundary found
      while (_buffer.find("\n---\n") == std::string::npos) {
        _input_stream->read(&buf[0], bufsiz);
        if (_input_stream->eof()) {
          buf = buf.substr(0, _input_stream->gcount());
        }
        _buffer += buf;
        if (_input_stream->eof()) {
          delete _input_stream;
          _input_stream = 0;
          break;
        }
      }
    }

    if (_buffer.size() == 0) {
      _done = true;
      return;
    }
    
    if (_buffer.find("\n---\n") != std::string::npos) {
      size_t consumed = _buffer.find("\n---\n");
      *doc = _buffer.substr (0, consumed);
      _buffer = _buffer.substr(consumed + 5);
    } else {
      *doc = _buffer;
      _buffer = "";
    }
  }

  void yaml_corpus_iterator::next() {
    std::string doc;
    this->load_document(&doc);
    // TO DO: 5 is depending on the fact that separator is "\n---\n"
    //        The program should allow "\n--- \n" as well...
    _curpos = _nextpos;
    _nextpos += doc.size() + 5;
    if (_done) {
      _cursor = nil_t();
    } else {
      YAML::Node current_node = YAML::Load(doc);
      _cursor = convert_to_variant(current_node);
    }
  }

  const corpus_entry& yaml_corpus_iterator::value() {
    return boost::get<corpus_entry>(_cursor);
  }

  yaml_corpus_writer::yaml_corpus_writer(const std::string& filepath) {
    _output_stream = new std::ofstream(filepath);
  }

  yaml_corpus_writer::~yaml_corpus_writer() {
    if (_output_stream) delete _output_stream;
  }

  void yaml_corpus_writer::write(const corpus_entry& entry) {
    variant_t v = entry;
    YAML::Node n = make_node_from_variant(v);
    *_output_stream << "---" << std::endl << n << std::endl;
  }
  
}

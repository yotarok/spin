#include <spin/corpus/msgpack.hpp>
#include <spin/io/msgpack.hpp>

namespace spin {
  msgpack_corpus_iterator::msgpack_corpus_iterator(const std::string& filepath,
                                                   corpus_pos_t pos) {
    _input_stream = new std::ifstream(filepath);
    _input_stream->seekg(pos, std::ios_base::beg);
    this->next();
  }

  msgpack_corpus_iterator::~msgpack_corpus_iterator() {
    if (_input_stream) delete _input_stream;
  }

  bool msgpack_corpus_iterator::done() {
    return ! _input_stream;
  }

  void msgpack_corpus_iterator::next() {
    uint64 siz;
    char magic[9]; magic[8] = '\0';
    _curpos = _input_stream->tellg();
    _input_stream->read(magic, 8);
    //assert(std::string(magic) == "StacCrps");
    
    _input_stream->read(reinterpret_cast<char*>(&siz), sizeof(siz));

    if (_input_stream->eof()) {
      delete _input_stream;
      _input_stream = 0;
      return;
    }
    
    std::string buf(siz, '\0');
    _input_stream->read(&buf[0], siz); // is it safe operation?
    
    msgpack::unpacked result;
    msgpack::unpack(result, buf.data(), buf.size());
    
    msgpack::object deserialized = result.get();
    deserialized.convert(&_cursor);
  }

  const corpus_entry& msgpack_corpus_iterator::value() {
    return _cursor;
  }

  msgpack_corpus_writer::msgpack_corpus_writer(const std::string& filepath) {
    _output_stream = new std::ofstream(filepath);
  }

  msgpack_corpus_writer::~msgpack_corpus_writer() {
    if (_output_stream) { delete _output_stream; }
  }

  void msgpack_corpus_writer::write(const corpus_entry& object) {
    std::ostringstream oss;
    msgpack::pack(oss, object);
    uint64 siz = oss.str().size();
    const char* magic = "StacCrps";
    _output_stream->write(magic, 8);
    _output_stream->write(reinterpret_cast<const char*>(&siz), sizeof(siz));
    _output_stream->write(reinterpret_cast<const char*>(oss.str().c_str()),
                          siz);
  }


}

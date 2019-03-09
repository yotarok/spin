#ifndef spin_corpus_msgpack_hpp_
#define spin_corpus_msgpack_hpp_

#include <spin/types.hpp>
#include <spin/corpus/corpus.hpp>
#include <spin/io/msgpack.hpp>

namespace spin {

  class msgpack_corpus_iterator : public corpus_iterator {
    std::istream* _input_stream;
    corpus_entry _cursor;
    size_t _curpos;
  public:
    msgpack_corpus_iterator(const std::string& filepath, corpus_pos_t pos = 0);
    virtual ~msgpack_corpus_iterator();
    bool done();
    void next();
    const corpus_entry& value();

    virtual corpus_pos_t pos() const { return _curpos; }
  };

  class msgpack_corpus_writer : public corpus_writer {
    std::ostream* _output_stream;
  public:
    msgpack_corpus_writer(const std::string& filepath);
    virtual ~msgpack_corpus_writer();
    virtual void write(const corpus_entry& object);
  };

}

#endif


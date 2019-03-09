#ifndef spin_corpus_yaml_hpp_
#define spin_corpus_yaml_hpp_

#include <memory>
#include <yaml-cpp/yaml.h>
#include <spin/types.hpp>
#include <spin/corpus/corpus.hpp>

namespace spin {

  class yaml_corpus_iterator : public corpus_iterator {
    std::istream* _input_stream;
    variant_t _cursor;

    std::string _buffer;

    void load_document(std::string* doc);

    bool _done;
    size_t _curpos;
    size_t _nextpos;
  public:
    yaml_corpus_iterator(const std::string& filepath, corpus_pos_t pos = 0);
    // Convenient for unit testing, delegate ownership of is
    yaml_corpus_iterator(std::istream* is);
    virtual ~yaml_corpus_iterator();
    bool done();
    void next();

    const corpus_entry& value();
    virtual corpus_pos_t pos() const { return _curpos; }
  };

  class yaml_corpus_writer : public corpus_writer {
    std::ostream* _output_stream;
  public:
    yaml_corpus_writer(const std::string& filepath);
    virtual ~yaml_corpus_writer();
    virtual void write(const corpus_entry& entry);
  };

}

#endif

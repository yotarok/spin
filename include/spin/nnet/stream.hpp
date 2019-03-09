#ifndef spin_nnet_stream_hpp_
#define spin_nnet_stream_hpp_

#include <spin/types.hpp>
#include <spin/io/variant.hpp>
#include <spin/nnet/node.hpp>
#include <spin/corpus/corpus.hpp>
#include <gear/flow/flow.hpp>

namespace spin {
  enum stream_typeflag {
    INPUT_STREAM = 0x01, // implies subclass of source
    LOSS_STREAM = 0x02,
    OUTPUT_STREAM = 0x04,
    GENERATOR_STREAM = 0x08,
  };

  enum source_datatype {
    FLOAT_SOURCE_DATATYPE,
    INT_SOURCE_DATATYPE
  };


  struct stream_specifier {
    std::string type;
    std::string corpus_path;
    std::string corpus_tag;
    std::string component;
    std::string flow_path;

    stream_specifier(const std::string& arg);
  };


  class stream;
  typedef std::shared_ptr<stream> stream_ptr;
  
  class stream {
    std::string _compname;
  protected:
    /**
     * Construct stream object with target component name
     */
    stream(const std::string& compname)
      : _compname(compname) { }
  public:
    virtual const std::string& target_component() const { return _compname; }

    virtual bool before_forward(node_context_ptr dest,
                                const std::vector<corpus_entry>& batch) {
      return false;
    }

    virtual bool before_backward(node_context_ptr dest,
                                 const std::vector<corpus_entry>& batch) {
      return false;
    }

    /**
     * Return type flags as an OR-combination of stream_typeflag
     */
    virtual uint32_t typeflags() const=0;

    /**
     * Write statistics info to a destination variant
     */
    virtual void write_statistics(variant_t* dest) = 0;
    
    /**
     * Create new stream instance of concrete class
     */
    static stream_ptr create(const stream_specifier&);

    /**
     * Returns loss function value
     *
     * It doesn't make sense to override it in non-loss stream, however
     * for avoiding multiple inheritance, default implementation is provided
     * in this root base class
     */
    virtual float loss() const { return 0.0; }

    /**
     * Clear loss function value
     *
     * It doesn't make sense to override it in non-loss stream, however
     * for avoiding multiple inheritance, default implementation is provided
     * in this root base class
     */
    virtual void clear_loss() { }
  };

  class source_stream : public stream {
    std::string _tagname;
    std::string _source;
  protected:
    source_stream(const std::string& compname, const std::string& tagname,
                  const std::string& source)
      : stream(compname), _tagname(tagname), _source(source) {
    }
  public:
    virtual const std::string& tagname() const { return _tagname; }

    /**
     * Return URI for source corpus
     */
    virtual const std::string& source() const { return _source; }

    virtual const source_datatype datatype() const=0;
  };

  class input_stream : public source_stream {
    int _nframes;
  public:
    input_stream(const std::string& compname, const std::string& tagname,
                 const std::string& source);
    bool before_forward(node_context_ptr dest,
                        const std::vector<corpus_entry>& batch);

    void write_statistics(variant_t* dest);
    virtual uint32_t typeflags() const { return INPUT_STREAM; }
    virtual const source_datatype datatype() const {
      return FLOAT_SOURCE_DATATYPE;
    }
  };

  class xent_label_stream : public source_stream {
    float _loss;
    int _nframes;
    int _nerror;
  public:
    xent_label_stream(const std::string& compname, const std::string& tagname,
                      const std::string& source);
    bool before_backward(node_context_ptr dest,
                         const std::vector<corpus_entry>& batch);
    virtual float loss() const { return _loss; }
    virtual void clear_loss() { _loss = 0.0; }
    void write_statistics(variant_t* dest);
    virtual uint32_t typeflags() const { return LOSS_STREAM; }
    virtual const source_datatype datatype() const {
      return INT_SOURCE_DATATYPE;
    }
  };

  class square_error_stream : public source_stream {
    float _loss;
    int _nframes;
  public:
    square_error_stream(const std::string& compname, const std::string& tagname,
                        const std::string& source);
    
    bool before_backward(node_context_ptr dest,
                         const std::vector<corpus_entry>& batch);
    float loss() const { return _loss; }
    virtual void clear_loss() { _loss = 0.0; }

    void write_statistics(variant_t* dest);
    virtual uint32_t typeflags() const { return LOSS_STREAM; }
    virtual const source_datatype datatype() const {
      return FLOAT_SOURCE_DATATYPE;
    }
  };

  class output_stream : public stream {
    std::string tagname_;
    std::string destination_;
  public:
    output_stream(const std::string& compname, const std::string& tagname,
                  const std::string& destination);

    virtual uint32_t typeflags() const { return OUTPUT_STREAM; }

    const std::string& destination() const { return destination_; }

    const std::string& tagname() const { return tagname_; }
    virtual void write_statistics(variant_t* dest);
  };

}

#endif

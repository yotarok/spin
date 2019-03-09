#ifndef spin_corpus_corpus_hpp_
#define spin_corpus_corpus_hpp_

#include <string>
#include <map>
#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>

namespace spin {
  typedef std::map<std::string, variant_t> corpus_entry;
  class corpus_iterator;
  class corpus_writer;

  typedef size_t corpus_pos_t;

  typedef boost::shared_ptr<corpus_iterator> corpus_iterator_ptr;
  typedef boost::shared_ptr<corpus_writer> corpus_writer_ptr;
  
  corpus_iterator_ptr
  make_corpus_iterator(const std::string& path, 
                       boost::logic::tribool is_binary = boost::logic::indeterminate);
  corpus_iterator_ptr
  make_corpus_iterator(const std::string& path, corpus_pos_t pos,
                       boost::logic::tribool is_binary = boost::logic::indeterminate);

  corpus_writer_ptr
  make_corpus_writer(const std::string& path, bool is_binary);

  //void
  //remove_nonsticky_tags(corpus_entry* pent);

  void copy_sticky_tags(corpus_entry* pent, const corpus_entry& src);


  class corpus_iterator {
  public:
    virtual ~corpus_iterator() { }
    virtual bool done() = 0;
    virtual void next() = 0;
    virtual const corpus_entry& value() = 0;

    virtual std::string get_key() {
      const corpus_entry& entry = this->value();
      corpus_entry::const_iterator it = entry.find("+key");
      if (it == entry.end()) {
        throw std::runtime_error("Invalid corpus: Cannot find +key");
      }
      return boost::get<std::string>(it->second);
    }

    virtual corpus_pos_t pos() const = 0;
  };


  class corpus_writer {
  public:
    virtual ~corpus_writer() { }
    virtual void write(const corpus_entry& entry)=0;
  };

  class zipped_corpus_iterator : public corpus_iterator {
    std::vector<std::pair<std::string, corpus_iterator_ptr> > _iterators;
    corpus_entry _merged;
    void update_merged_entry();
    void skip_unseen_entries();

    std::vector<boost::tuple<int, std::string, std::string> > _imports;
    
  public:
    typedef
    std::vector<std::pair<std::string,corpus_iterator_ptr> > corpus_iterators;

    zipped_corpus_iterator(corpus_iterators& ites);
    virtual ~zipped_corpus_iterator();

    virtual bool done();

    virtual void next();

    virtual std::string get_key();

    virtual void import_key(int idx, const std::string& origkey, const std::string& newkey);

    virtual const corpus_entry& value();
    virtual const corpus_entry& value(int n);
    virtual corpus_pos_t pos() const {
      // TO DO: Implement abstraction to corpus_pos_t and support zipped
      throw std::runtime_error("Pos is not supported in zipped corpus");
    }

  };
  typedef boost::shared_ptr<zipped_corpus_iterator> zipped_corpus_iterator_ptr;

  zipped_corpus_iterator_ptr zip_corpus(const std::string& name1,
                                        corpus_iterator_ptr it1,
                                        const std::string& name2,
                                        corpus_iterator_ptr it2);

}
#endif

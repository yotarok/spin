#include <spin/corpus/corpus.hpp>
#include <spin/corpus/yaml.hpp>
#include <spin/corpus/msgpack.hpp>
#include <spin/io/file.hpp>
#include <gear/io/logging.hpp>

namespace spin {
  corpus_iterator_ptr
  make_corpus_iterator(const std::string& path, 
                       boost::logic::tribool is_binary) {
    if (boost::logic::indeterminate(is_binary)) {
      is_binary = check_binary_header(path);
    }
    corpus_iterator* p = (is_binary)
      ? (corpus_iterator*) new msgpack_corpus_iterator(path)
      : (corpus_iterator*) new yaml_corpus_iterator(path);

    return corpus_iterator_ptr(p);
  }
  corpus_iterator_ptr

  make_corpus_iterator(const std::string& path, corpus_pos_t pos,
                       boost::logic::tribool is_binary) {
    if (boost::logic::indeterminate(is_binary)) {
      is_binary = check_binary_header(path);
    }
    corpus_iterator* p = (is_binary)
      ? (corpus_iterator*) new msgpack_corpus_iterator(path, pos)
      : (corpus_iterator*) new yaml_corpus_iterator(path, pos);

    return corpus_iterator_ptr(p);
  }

  
  corpus_writer_ptr
  make_corpus_writer(const std::string& path, 
                     bool is_binary) {
    corpus_writer* p = (is_binary)
      ? (corpus_writer*) new msgpack_corpus_writer(path)
      : (corpus_writer*) new yaml_corpus_writer(path);

    return corpus_writer_ptr(p);   
  }

  void copy_sticky_tags(corpus_entry* pent, const corpus_entry& src) {
    for (corpus_entry::const_iterator it = src.begin(), last = src.end();
         it != last; ++ it) {
      if (it->first[0] == '+') {
        pent->insert(std::make_pair(it->first, it->second));
      }
    }
  }

  /* deleted since it's not recommended due to performance
  void remove_nonsticky_tags(corpus_entry* pent) {
    std::set<std::string> removed_tags;
    for (corpus_entry::iterator it = pent->begin(), last = pent->end();
         it != last; ++ it) {
      if (it->first[0] != '+') {
        removed_tags.insert(it->first);
      }
    }
    for (std::set<std::string>::iterator it = removed_tags.begin(), 
           last = removed_tags.end(); it != last; ++ it) {
      pent->erase(*it);
    }
  }
  */

  zipped_corpus_iterator::zipped_corpus_iterator(corpus_iterators& ites) 
    : _iterators(ites) {
    skip_unseen_entries();
    update_merged_entry();
  }

  zipped_corpus_iterator::~zipped_corpus_iterator() {
  }

  bool zipped_corpus_iterator::done() {
    bool any = false;
    for (corpus_iterators::iterator itit = _iterators.begin(),
           last = _iterators.end(); itit != last; ++ itit) {
      if (itit->second->done()) {
        any = true;
        break;
      }
    }
    return any;
  }

  void zipped_corpus_iterator::update_merged_entry() {
    // create merged entry object
    _merged.clear();
    // copy sticky entries from the first iterator
    for (corpus_entry::const_iterator
           vit = _iterators.begin()->second->value().begin(),
           vlast = _iterators.begin()->second->value().end();
         vit != vlast; ++ vit) {
      if (vit->first[0] == '+') {
        _merged.insert(std::make_pair(vit->first,
                                      vit->second));
      }
    }
    // copy entries according to the import rules
    for (auto it = _imports.cbegin(), last = _imports.cend();
         it != last; ++ it) {
      auto srcit = _iterators[it->get<0>()].second;
      const corpus_entry& srcmap = srcit->value();
      auto srcvit = srcmap.find(it->get<1>());
      if (srcvit == srcmap.end()) {
        WARN("Key %s is not found from corpus %s, skip",
             it->get<1>().c_str(), _iterators[it->get<0>()].first.c_str());
      } else {
        _merged.insert(std::make_pair(it->get<2>(), srcvit->second));
      }
                                    
    }
  }
  

  void zipped_corpus_iterator::skip_unseen_entries() {
    // forward iterators that is lesser than max key
    do {
      std::string maxkey = "";
      std::string key = _iterators.begin()->second->get_key();

      bool matched = true;
      for (corpus_iterators::iterator itit = _iterators.begin(),
             last = _iterators.end(); itit != last; ++ itit) {
        if (itit->second->get_key() != key) {
          matched = false;
        }
        if (itit->second->get_key() > maxkey) maxkey = itit->second->get_key();
      }

      if (matched) {
        break;
      }

      bool done = false;
      for (corpus_iterators::iterator
             itit = _iterators.begin(), last = _iterators.end();
           itit != last; ++ itit) {
        if (itit->second->get_key() != maxkey) {
          std::cout << "Skipped " << itit->second->get_key() << " in "
                    << itit->first << std::endl;
          if (itit->second->done()) {
            done = true;
            break;
          }
          itit->second->next();
        }
      }
      if (done) break;
    } while (1);
  }

  void zipped_corpus_iterator::next() {
    // forward all iterators
    for (corpus_iterators::iterator itit = _iterators.begin(),
           last = _iterators.end(); itit != last; ++ itit) itit->second->next();

    if (! this->done()) {
      skip_unseen_entries();
      update_merged_entry();
    }
  }

  std::string zipped_corpus_iterator::get_key() {
    return _iterators.begin()->second->get_key();
  }

  const corpus_entry& zipped_corpus_iterator::value() {
    return _merged;
  }

  const corpus_entry& zipped_corpus_iterator::value(int n) {
    return _iterators[n].second->value();
  }

  void zipped_corpus_iterator::import_key(int idx, const std::string& origkey,
                                          const std::string& newkey) {
    if (idx >= _iterators.size()) {
      throw std::runtime_error("Specified invalid iterator id");
    }
    _imports.push_back(boost::make_tuple(idx, origkey, newkey));
    update_merged_entry();
  }

  zipped_corpus_iterator_ptr zip_corpus(const std::string& name1,
                                        corpus_iterator_ptr it1,
                                        const std::string& name2,
                                        corpus_iterator_ptr it2) {
    std::vector<std::pair<std::string, corpus_iterator_ptr> > v;
    v.push_back(std::make_pair(name1, it1));
    v.push_back(std::make_pair(name2, it2));
    return zipped_corpus_iterator_ptr(new zipped_corpus_iterator(v));
  }


}

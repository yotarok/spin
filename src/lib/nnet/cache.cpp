#include <spin/nnet/cache.hpp>
#include <spin/io/variant.hpp>
#include <spin/nnet/stream.hpp>

namespace spin {

  
  random_frame_cache::random_frame_cache(nnet_input_data& input_data,
                                         const std::vector<stream_ptr>& streams,
                                         size_t batchsize, size_t cachesize,
                                         int seed)
    : _input_data(input_data), _streams(streams),
      _batchsize(batchsize), _cachesize(cachesize), _leftbound(0),
      _rng(seed), _inited(false), _done(false) {

  }
  
  bool random_frame_cache::pull_next_sequence() {
    if (_input_data.done()) return false;

    _input_data.pull_next_sequence(&_next_seq);
    
    int len = -1;
    for (auto it = _streams.begin(), last = _streams.end();
         it != last; ++ it) {
      const source_stream* sstr = dynamic_cast<const source_stream*>(it->get());
      if (! sstr) continue;

      std::string compname = sstr->target_component();

      variant_t var = _next_seq[compname];
      int n = 0, d = 0, typ = var.which();
      if (typ == VARIANT_INTMATRIX) {
        d = boost::get<intmatrix>(var).rows();
        n = boost::get<intmatrix>(var).cols();
      } else if (typ == VARIANT_FMATRIX) {
        d = boost::get<fmatrix>(var).rows();
        n = boost::get<fmatrix>(var).cols();
      } else {
        throw std::runtime_error("Unsupported type consumed by random cache");
      }

      if (len < 0) len = n;
      else if (len != n) {
        ERROR("Number of frames doesn't match %d vs %d", len, n);
      }

      if (_types.find(compname) == _types.end()) {
        _types.insert(std::make_pair(compname, typ));
      } else if (_types[compname] != typ) {
        ERROR("Data type doesn't match %d vs %d", _types[compname], typ);
      }
      
      if (_dims.find(compname) == _dims.end()) {
        _dims.insert(std::make_pair(compname, d));
      } else if (_dims[compname] != d) {
        ERROR("Dimensionality doesn't match %d vs %d", _dims[compname], d);
      }
    }

    _next_len = len;
    _next_off = 0;
    return true;
  }

  
  void random_frame_cache::next() {
    if (! _inited) {
      throw std::runtime_error("Call initialize before using random cache");
    }
    if (_leftbound >= _cachesize) {
      _done = true;
      return;
    }
    int filled = fill_cache(_batchsize);
    if (filled > 0) {
      shuffle_cache(0, filled, _rng);
    } 

    set_cursor(_leftbound);
    if (filled < _batchsize) {
      _leftbound = std::min(_leftbound + _batchsize, _cachesize);
    }
  }

  bool random_frame_cache::done() {
    if (! _inited) {
      throw std::runtime_error("Call initialize before using random cache");
    }
    return _done;
  }

  int random_frame_cache::fill_cache(int right) {
    int read = 0;

    while (read < right) {
      int len = std::min(_next_len - _next_off, right - read);
      if (len > 0) {
        for (auto it = _streams.cbegin(), last = _streams.cend();
             it != last; ++ it) {
          const std::string& compname = (*it)->target_component();
          int vartype = _next_seq[compname].which();
          int d = _dims[compname];

          if (vartype == VARIANT_FMATRIX) {
            fmatrix& buffer = boost::get<fmatrix>(_data[compname]);
            buffer.block(0, read, d, len) =
              boost::get<fmatrix>(_next_seq[compname]).block(0, _next_off,
                                                             d, len);
          }
          else if (vartype == VARIANT_INTMATRIX) {
            intmatrix& buffer = boost::get<intmatrix>(_data[compname]);
            buffer.block(0, read, d, len) =
              boost::get<intmatrix>(_next_seq[compname]).block(0, _next_off,
                                                               d, len);
          }
        }
      }

      read += len;
      _next_off += len;
      if (_next_off == _next_len) {
        if (! pull_next_sequence()) break;
      }
    }
    return read;
  }

  struct permute_inplace : public boost::static_visitor<> {
    int _off;
    const std::vector<int>& _indices;

    permute_inplace(int off, const std::vector<int>& indices)
      : _off(off), _indices(indices) { }

    template <typename NumT>
    void operator() (Eigen::Matrix<NumT,Eigen::Dynamic,Eigen::Dynamic>& data) const {
      Eigen::Matrix<NumT, Eigen::Dynamic, 1> tmpvec(data.rows());
      for (int tau = 0; tau < _indices.size(); ++ tau) {
        int col = _off + tau;
        int rcol = _indices[tau];
        
        tmpvec = data.col(col);
        data.col(col) = data.col(rcol);
        data.col(rcol) = tmpvec;
      }
    }

    template <typename T>
    void operator() (T data) const {
      throw std::runtime_error("Unsupported data type: permute_inplace");
    }
  };

  void random_frame_cache::shuffle_cache(int left, int right,
                                         boost::mt19937& rng) {
    std::vector<int> indices;
    boost::random::uniform_int_distribution<> randomcol(0, _cachesize - 1);
    for (int i = 0; i < (right - left); ++ i) {
      indices.push_back(randomcol(rng));
    }

    for (auto it = _streams.cbegin(), last = _streams.cend();
         it != last; ++ it) {
      const std::string& compname = (*it)->target_component();
      boost::apply_visitor(permute_inplace(left, indices),
                           _data[compname]);
    }
  }

  struct set_cursor_impl : public boost::static_visitor<> {
    variant_t& _cur_data;
    int _left, _batchsize, _dims;
    
    set_cursor_impl(variant_t& cur_data, int l, int bs, int d)
      : _cur_data(cur_data), _left(l), _batchsize(bs), _dims(d) { }

    template <typename NumT>
    void operator() (Eigen::Matrix<NumT,Eigen::Dynamic,Eigen::Dynamic>& data) const {
      typedef Eigen::Matrix<NumT,Eigen::Dynamic,Eigen::Dynamic>  MatT;
      boost::get<MatT>(_cur_data) = data.block(0, _left, _dims, _batchsize);
    }
    template <typename T>
    void operator() (T data) const {
      throw std::runtime_error("Unsupported data type: set_cursor_impl");
    }
  };

  void random_frame_cache::set_cursor(int offset) {
    int left = std::min((int)offset, (int) (_cachesize - _batchsize));
    for (auto it = _streams.cbegin(), last = _streams.cend();
         it != last; ++ it) {
      const std::string& compname = (*it)->target_component();
      boost::apply_visitor(set_cursor_impl(_cur_data[compname], left,
                                           _batchsize, _dims[compname]),
                           _data[compname]);
    }
  }
  
  void random_frame_cache::initialize() {
    pull_next_sequence(); // for getting dims

    int n = 0;
    for (auto it = _streams.cbegin(), last = _streams.cend();
         it != last; ++ it, ++ n) {
      const std::string& compname = (*it)->target_component();
      if (_types[compname] == VARIANT_FMATRIX) {
        fmatrix zero = fmatrix::Zero(_dims[compname], _cachesize);
        fmatrix bzero = fmatrix::Zero(_dims[compname], _batchsize);
        _data.insert(std::make_pair(compname, zero));
        _cur_data.insert(std::make_pair(compname, bzero));
      } else if (_types[compname] == VARIANT_INTMATRIX) {
        intmatrix zero = intmatrix::Zero(_dims[compname], _cachesize);
        intmatrix bzero = intmatrix::Zero(_dims[compname], _batchsize);
        _data.insert(std::make_pair(compname, zero));
        _cur_data.insert(std::make_pair(compname, bzero));
      }
    }

    fill_cache(_cachesize);
    shuffle_cache(0, _cachesize, _rng);
    set_cursor(0);

    _leftbound = 0;
    _inited = true;
    _done = false;
  }

  
  const variant_t& random_frame_cache::data(const std::string& k) const {
    auto it = _cur_data.find(k);
    if (it == _cur_data.cend())
      throw std::runtime_error("Key " + k + " is not found");
    return it->second;
  }

  void random_frame_cache::retrieve(corpus_entry* pent) const {
    pent->clear();
    for (auto it = _cur_data.cbegin(), last = _cur_data.cend();
         it != last; ++ it) {
      pent->insert(std::make_pair(it->first, it->second));
      /*
      if (it->second.which() == 0) {
        pent->insert(std::make_pair(it->first,
                                    variant_t(boost::get<fmatrix>(it->second))));
      } else if (it->second.which() == 1) {
        pent->insert(std::make_pair(it->first,
                                    variant_t(boost::get<intmatrix>(it->second))));
      } else {
        ERROR("Unknown batchdata type");
      }
      */
    }
  }
    
}

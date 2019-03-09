#ifndef spin_nnet_signal_hpp_
#define spin_nnet_signal_hpp_

#include <spin/types.hpp>
#include <spin/variant.hpp>
#include <spin/nnet/types.hpp>

#include <boost/lexical_cast.hpp>
#include <spin/utils.hpp>
#include <gear/io/logging.hpp>


#define NNET_BATCH(sig, n) viennacl::project((sig).raw_data(), (sig).rows(), (sig).cols(n))

namespace spin {
  typedef viennacl::matrix_range<nnet_matrix> nnet_submat;

  /**
   * Class for representing a single batch of input/ output of NNs
   */
  class nnet_signal {
    int _maxnbatch;
    int _maxbatchsize;
    nnet_matrix _data;
    std::vector<size_t> _size;
    nnet_signal& operator = (const nnet_signal& other); // forbidden
    
    mutable viennacl::matrix_range<nnet_matrix>* _hot_range;
    viennacl::matrix_range<nnet_matrix>& batch(int n) const;
    // ^ tentatively disabled
  public:
    nnet_signal(int maxnbatch, int maxbatchsize, int dim);
    
    virtual ~nnet_signal() { }

    
    nnet_signal& load(const nnet_signal& other);

    size_t ndim() const { return _data.size1(); }
    void load(int n, const fmatrix& m);
    
    

    viennacl::matrix_range<nnet_matrix>& allocate(int n, int t) {
      set_size(n, t);
      return batch(n);
    }
    int set_size_M(int n, int t) { _size[n] = t; return 0; }

    void set_size(int n, int t) { _size[n] = t; }

    nnet_matrix& raw_data() { return _data; }

   
    const nnet_matrix& raw_data() const { return _data; }

    viennacl::range rows() const { return viennacl::range(0, _data.size1()); }
    viennacl::range cols(int n) const {
      return viennacl::range(n * _maxbatchsize, n * _maxbatchsize + _size[n]);
    }
  
    const size_t size(int n) const { return _size[n]; }

    void clear() {
      _data = viennacl::zero_matrix<float>(_data.size1(),
                                           _data.size2());
    }

    int nbatch() const { return _maxnbatch; }
  };

}

#endif

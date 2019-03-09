#include <spin/nnet/signal.hpp>

namespace spin {
  nnet_signal::nnet_signal(int maxnbatch, int maxbatchsize, int dim) :
    _maxnbatch(maxnbatch), _maxbatchsize(maxbatchsize),
    _data(dim, maxbatchsize * maxnbatch),
    _size(maxnbatch, 0), _hot_range(0) {
    for (int i = 0; i < maxnbatch; ++ i) {
      _size.push_back(0);
    }
  }

  
  nnet_signal& nnet_signal::load(const nnet_signal& other) {
    if (this->nbatch() != other.nbatch()) {
      ERROR("numbers of batches in 2 signal must be equal");
    }
    _data = other._data;
    _size = other._size;
    return *this;
  }

  void nnet_signal::load(int n, const fmatrix& m) {
    viennacl::range allrows(0, m.rows());
    viennacl::range selcols(_maxbatchsize * n, _maxbatchsize * n + m.cols());
    _size[n] = m.cols();

    nnet_submat d = viennacl::project(_data, allrows, selcols);
    viennacl::copy(m, d);
  }

  viennacl::matrix_range<nnet_matrix>& nnet_signal::batch(int n) const {
    viennacl::range allrows(0, _data.size1());
    viennacl::range selcols(_maxbatchsize * n, _maxbatchsize * n + _size[n]);
    //return viennacl::project(_data, allrows, selcols);
    if (_hot_range != 0) delete _hot_range;
    _hot_range = new viennacl::matrix_range<nnet_matrix>(_data, allrows, selcols);
    return *_hot_range;
  }
  

}

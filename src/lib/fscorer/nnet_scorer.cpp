#include <spin/fscorer/nnet_scorer.hpp>
#include <spin/nnet/nnet.hpp>
#include <spin/utils.hpp>
#include <gear/io/logging.hpp>
#include <spin/nnet/ident.hpp>

namespace spin {
  nnet_scorer::nnet_scorer(std::shared_ptr<nnet> param) 
    : _parameter(param), _nnet_config(new nnet_config(*param)) {
    _context.reset(new nnet_context(*_parameter, 1, 2048));
  }

  void nnet_scorer::set_frames(const fmatrix& f) {
    _feed_forward_done = false;
    if (f.cols() > 2048) {
      throw std::runtime_error("Not implemented");
    }
    _context->get("input")->get_output().load(0, f);
    _context->set_forward_stream_flag(_parameter->get_node_location("input"),
                                      true);
  }

  float nnet_scorer::get_score(int t, int s) {
    if (! _feed_forward_done) {
      _parameter->feed_forward(*_nnet_config, _context.get());
      _score.resize(_context->get("output")->get_input().ndim(),
                    _context->get("output")->get_input().size(0));
      viennacl::copy(NNET_BATCH(_context->get("output")->get_input(), 0),
                     _score);
      _feed_forward_done = true;
    }

    return _score(s, t);
  }

  size_t nnet_scorer::nstates() const {
    auto p = _parameter->node("output");
    auto pident = std::dynamic_pointer_cast<const nnet_node_ident>(p);
    return pident->ndim();
  }
}

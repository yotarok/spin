#include <spin/nnet/stream.hpp>
#include <spin/nnet/nnet.hpp>
#include <gear/io/logging.hpp>
#include <iostream>
#include <spin/flow/flowutils.hpp>

#include <boost/algorithm/string.hpp>
#include <viennacl/linalg/norm_frobenius.hpp>

#define EPSILON 0.0000000001f

namespace spin {
  
  stream_specifier::stream_specifier(const std::string& arg) {
    std::vector<std::string> vals;
    boost::split(vals, arg, boost::is_any_of(":;"));
    type = vals[0];
    corpus_path = vals[1];
    corpus_tag = vals[2];
    component = vals[3];
    if (vals.size() > 4) {
      flow_path = vals[4];
    } else {
      flow_path.clear();
    }
  }



  input_stream::input_stream(const std::string& compname,
                             const std::string& tagname,
                             const std::string& source)
    : source_stream(compname, tagname, source), _nframes(0) {
  }

  bool input_stream::before_forward(node_context_ptr dest,
                                    const std::vector<corpus_entry>& batch) {
    node_basic_context* pctx = dynamic_cast<node_basic_context*>(dest.get());
    for (int n = 0; n < batch.size(); ++ n) {
      auto mit = batch[n].find(target_component());
      if (mit == batch[n].cend()) {
        ERROR("component %s is not found", target_component().c_str());
      }
      fmatrix m = boost::get<fmatrix>(mit->second);
      if (m.hasNaN() || ! m.allFinite()) {
        if (m.hasNaN()) {
          throw std::runtime_error("Input has NaN");
        } else {
          throw std::runtime_error("Input has infinity");
        }        
      }
      //std::cout << "input range = " << m.minCoeff() << " - " << m.maxCoeff() << std::endl;
      _nframes += m.cols();
      pctx->get_output().load(n, m);
    }
    return true;
  }

  void input_stream::write_statistics(variant_t* dest) {
    variant_map props;
    props["nframes"] = _nframes;
    *dest = props;
  }

  xent_label_stream::xent_label_stream(const std::string& compname,
                                       const std::string& tagname,
                                       const std::string& source)
    : source_stream(compname, tagname, source),
      _loss(0.0f), _nframes(0), _nerror(0) {
  }

  bool
  xent_label_stream::before_backward(node_context_ptr dest,
                                     const std::vector<corpus_entry>& batch) {
    node_basic_context* pctx = dynamic_cast<node_basic_context*>(dest.get());

    for (int n = 0; n < batch.size(); ++ n) {
      auto lit = batch[n].find(target_component());
      if (lit == batch[n].cend()) {
        ERROR("Label tag %s is not found", target_component().c_str());
      }
      intmatrix labs = boost::get<intmatrix>(lit->second);

      nnet_submat m = NNET_BATCH(pctx->get_input(), n);
      fmatrix score(m.size1(), m.size2());
      viennacl::copy(m, score);

      //fmatrix logdenom =
      //score.array().exp().colwise().sum().log().matrix();
        
      if (labs.cols() != score.cols()) {
        ERROR("Nums of frames in forward and backward pass are different; "
              "%d vs %d", score.cols(), labs.cols());
      }

      fmatrix dloss(score.rows(), score.cols());
      for (int tau = 0; tau < labs.cols(); ++ tau) {
        fmatrix::Index hypidx;
        score.col(tau).maxCoeff(&hypidx);
        if (hypidx != labs(0, tau)) {
          _nerror += 1;
        }

        float maxscore = score.col(tau).maxCoeff();
        float logdenom = std::log((score.col(tau).array() - maxscore).exp().sum()) + maxscore;
        
        dloss.col(tau) =
          (score.col(tau).array() - logdenom).exp().matrix();
        
        _loss -= std::log(std::max((float) dloss(labs(0, tau), tau), EPSILON));
        dloss(labs(0, tau), tau) -= 1.0;


        if (dloss.col(tau).hasNaN()
            || dloss.col(tau).maxCoeff() > 1.01
            || dloss.col(tau).minCoeff() < -1.01) {
          std::cerr << "Numerical error found " << dloss.col(tau).hasNaN() << " " << dloss.col(tau).maxCoeff() << " " << dloss.col(tau).minCoeff() << " " << logdenom << std::endl;
          throw optimization_diverged();
          dloss.col(tau) = fvector::Zero(dloss.rows());
        }
      }

      _nframes += labs.cols();

      pctx->get_diff_input().load(n, dloss);
    }
    return true;
  }


  void xent_label_stream::write_statistics(variant_t* dest) {
    variant_map props;
    props["nframes"] = _nframes;
    props["loss"] = _loss;
    props["nerror"] = _nerror;
    *dest = props;
  }


  square_error_stream::square_error_stream(const std::string& compname,
                                           const std::string& tagname,
                                           const std::string& source)
    : source_stream(compname, tagname, source), _loss(0.0f), _nframes(0) {
  }

  bool
  square_error_stream::before_backward(node_context_ptr dest,
                                       const std::vector<corpus_entry>& batch) {
    node_basic_context* pctx = dynamic_cast<node_basic_context*>(dest.get());

    for (int n = 0; n < batch.size(); ++ n) {
      auto mit = batch[n].find(target_component());
      if (mit == batch[n].cend()) {
        ERROR("Output tag %s is not found", target_component().c_str());
      }
      nnet_submat pred = NNET_BATCH(pctx->get_input(), n);
      int D = pred.size1(), T = pred.size2();

      fmatrix predd(pred.size1(), pred.size2());
      viennacl::copy(pred, predd);
      nnet_matrix actual(D, T);
      fmatrix actd = boost::get<fmatrix>(mit->second);
      viennacl::copy(actd, actual);
      fmatrix dlossd = predd - actd;

      _loss += (dlossd).squaredNorm();

      pctx->get_diff_input().load(n, dlossd);
      
      _nframes += T;
    }    
    return true;
  }

  
  void square_error_stream::write_statistics(variant_t* dest) {
    variant_map props;
    props["nframes"] = _nframes;
    props["loss"] = _loss;
    *dest = props;
  }

  output_stream::output_stream(const std::string& compname,
                               const std::string& tagname,
                               const std::string& destination)
    : stream(compname), tagname_(tagname), destination_(destination) {
  }

  void output_stream::write_statistics(variant_t* dest) {
    variant_map props;
    *dest = props;
  }



  stream_ptr stream::create(const stream_specifier& spec) {

    stream* p;
    if (spec.type == "input") {
      p = new input_stream(spec.component, spec.corpus_tag, spec.corpus_path);
    } else if (spec.type == "xent") {
      p = new xent_label_stream(spec.component, spec.corpus_tag, spec.corpus_path); 
    } else if (spec.type == "mse") {
      p = new square_error_stream(spec.component, spec.corpus_tag, spec.corpus_path); 
    } else if (spec.type == "output") {
      p = new output_stream(spec.component, spec.corpus_tag, spec.corpus_path); 
    } else {
      throw std::runtime_error("Unknown stream type");
    }
    return stream_ptr(p);
  }

  
}

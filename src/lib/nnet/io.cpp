#include <spin/nnet/io.hpp>
#include <spin/nnet/nnet.hpp>
#include <gear/io/logging.hpp>
#include <iostream>
#include <spin/flow/flowutils.hpp>

#include <boost/algorithm/string.hpp>

namespace spin {

  nnet_input_data::nnet_input_data(const std::vector<stream_ptr>& streams,
                                   const std::vector<gear::flow_ptr>& flows)
    : streams_(streams) {

    if (streams.size() != flows.size()) {
      throw std::runtime_error("stream list size and flow list size must be equal");
    }
    initialize_flows(flows);
    initialize_zipped_iterator();
  }

  nnet_input_data::
  nnet_input_data(zipped_corpus_iterator_ptr zit,
                  const std::vector<stream_ptr>& streams,
                  const std::vector<gear::flow_ptr>& flows)
    : zit_(zit), streams_(streams), flows_(flows) {
  }
  

  void
  nnet_input_data::
  initialize_flows(const std::vector<gear::flow_ptr>& flows_raw) {
    for (int n = 0; n < streams_.size(); ++ n) {
      auto pf = flows_raw[n];
      const source_stream* sstr =
        dynamic_cast<const source_stream*>(streams_[n].get());
      if (! sstr || ! pf) {
        flows_.push_back(gear::flow_ptr());
      } else {
        gear::flow_ptr newpf = pf->clone();
        if (sstr->datatype() == FLOAT_SOURCE_DATATYPE) {
          add_source_sink_inplace<float>(newpf);
        } else if (sstr->datatype() == INT_SOURCE_DATATYPE) {
          add_source_sink_inplace<int>(newpf);
        } else {
          throw std::runtime_error("Unsupported source datatype");
        }
        flows_.push_back(newpf);
      }
    }    
  }

  void
  nnet_input_data::
  initialize_zipped_iterator() {
    zipped_corpus_iterator::corpus_iterators corpora;

    for (auto pstr : streams_) {
      const source_stream* psstr = dynamic_cast<const source_stream*>(pstr.get());
      if (psstr == 0) { // Not source stream, skip
        continue;
      }
      INFO("Loading %s...", psstr->source().c_str());
      corpus_iterator_ptr cit = make_corpus_iterator(psstr->source().c_str());
      std::string corpusname = psstr->target_component() + "_" + psstr->tagname();
      corpora.push_back(std::make_pair(corpusname, cit));
    }

    zipped_corpus_iterator_ptr zit(new zipped_corpus_iterator(corpora));
    std::set<std::string> check_dup; // just for error checking
    for (int n = 0; n < streams_.size(); ++ n) {
      auto pstr = streams_[n];
      const source_stream* psstr = dynamic_cast<const source_stream*>(pstr.get());
      if (psstr == 0) { // Not source stream, skip
        continue;
      }
      if (check_dup.find(psstr->tagname()) != check_dup.cend()) {
        throw std::runtime_error("The current implementation doesn't allow duplicated tag names");
      }
      check_dup.insert(psstr->tagname());
      zit->import_key(n, psstr->tagname(), psstr->tagname());
    }

    zit_ = zit;
  }

  void nnet_input_data::pull_next_sequence(corpus_entry* dest) {
    variant_map entry = zit_->value();
    zit_->next();

    copy_sticky_tags(dest, entry);
    // Apply flow
    for (int n = 0; n < streams_.size(); ++ n) {
      const source_stream* sstr =
        dynamic_cast<const source_stream*>(streams_[n].get());
      if (! sstr) continue;
      if (! flows_[n]) continue;

      variant_t v = entry[sstr->tagname()];
      if (sstr->datatype() == FLOAT_SOURCE_DATATYPE) {
        apply_matrix_flow_inplace<float>(&v, flows_[n]);
      } else if (sstr->datatype() == INT_SOURCE_DATATYPE) {
        apply_matrix_flow_inplace<int>(&v, flows_[n]);
      } else {
        throw std::runtime_error("Unsupported source datatype");
      }
      entry[sstr->tagname()] = v;
    }

    // Move from tagname to component name
    for (auto it = streams_.begin(), last = streams_.end();
         it != last; ++ it) {
      const source_stream* sstr = dynamic_cast<const source_stream*>(it->get());
      if (! sstr) continue;
      
      std::string compname = sstr->target_component();
      auto sit = entry.find(sstr->tagname());
      if (sit != entry.cend()) {
        (*dest)[compname] = sit->second;
      }
    }

  }

  nnet_output_writer::nnet_output_writer(const std::vector<stream_ptr>& streams)
    : streams_(streams) {
    for (int n = 0; n < streams_.size(); ++ n) {
      const output_stream* ostr =
        dynamic_cast<const output_stream*>(streams_[n].get());
      if (ostr == 0) { // Not output stream, skip
        writers_.push_back(corpus_writer_ptr());
      } else {
        writers_.push_back(make_corpus_writer(ostr->destination(), true));
      }
    }
  }

  void nnet_output_writer::write_sequence(const nnet_context& context,
                                          const corpus_entry& metainfo) {
    for (int n = 0; n < streams_.size(); ++ n) {
      const output_stream* ostr =
        dynamic_cast<const output_stream*>(streams_[n].get());
      if (ostr == 0) continue; // Not output stream, skip

      corpus_entry data;
      copy_sticky_tags(&data, metainfo);
      node_context_cptr nctx = context.get(ostr->target_component());
      if (nctx->get_output().nbatch() != 1) {
        throw std::runtime_error("Currently, writing batch-of-batch is not supported");
      }

      fmatrix mat(nctx->get_output().ndim(),
                  nctx->get_output().size(0));
      viennacl::copy(NNET_BATCH(nctx->get_output(), 0), mat);
      data[ostr->tagname()] = mat;

      writers_[n]->write(data);
    }
  }
}

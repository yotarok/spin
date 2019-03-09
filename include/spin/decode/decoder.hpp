#ifndef spin_decode_decoder_hpp_
#define spin_decode_decoder_hpp_

#include <queue>
#include <map>
#include <unordered_map>
#include <fst/fst.h>
#include <boost/tuple/tuple.hpp>
#include <boost/container/small_vector.hpp>
#include <fst/topsort.h>

namespace spin {
  class no_hypothesis : public std::runtime_error {
  public:
    no_hypothesis() : std::runtime_error("No hypothesis") {
    }
  };
  
  class decoder {
    typedef fst::Fst<fst::StdArc> network;
    typedef fst::Fst<fst::StdArc>::StateId fst_state;
    typedef fst::Fst<fst::StdArc>::Arc fst_arc;
    network& _decodingnet;
    frame_scorer* _scorer;

    std::vector<int> _ilabel_to_hmm_state;
    
    struct transition {
      fst_state prev_fst_state;
      boost::container::small_vector<fst_arc, 2> arcs;
      typedef boost::container::small_vector<fst_arc, 2>::const_reverse_iterator
      const_reverse_arc_iterator;      
      
      float weight; // sum of arc transition weight, i.e. LM weight
      int next_hmm_state;
      bool self_loop;

      transition()
        : prev_fst_state(-1), arcs(), weight(0.0), next_hmm_state(-1),
          self_loop(false) {
      }
      
      transition(fst_state st, float w)
        : prev_fst_state(st), arcs(), weight(w), next_hmm_state(-1),
          self_loop(false) {
      }

      const fst_arc& get_last_arc() const {
        assert(arcs.size() != 0);
        return arcs[arcs.size() - 1];
      }

      fst_state get_next_fst_state() const {
        return (arcs.size() == 0) ? prev_fst_state : get_last_arc().nextstate;
      }

      uint32_t update_signature(uint32_t sign) const {
        for (auto arc : arcs) {
          if (arc.olabel == 0) continue;
          sign = sign << 11;
          sign ^= static_cast<uint32_t>(arc.olabel);
        }
        return sign;
      }

      bool has_epsilon() const { return arcs.size() > 1; }

      bool is_self_loop() const { return self_loop; }
    };

    struct hypo {
      hypo* prev_hypo;
      transition trans;
      float weight; // used as a sort key

      std::vector<hypo> merged_branch;
      uint32_t signature;
      
      hypo() : prev_hypo(0), weight(HUGE_VALF), signature(0) { }
      hypo(hypo* p, transition tr, float w, uint32_t sign)
        : prev_hypo(p), trans(tr), weight(w), signature(sign) { }

      const fst_arc& get_last_arc() const {
        return trans.get_last_arc();
      }
      fst_state get_next_fst_state() const {
        return trans.get_next_fst_state();
      }
    };
    
    struct hypo_weight_comp {
      bool operator()(const hypo& h1, const hypo& h2) const {
        return h1.weight < h2.weight;
      }
    };

    
    typedef std::vector<hypo> hypos;
    std::list<hypos> _frame_hypos;
    
    int _maxactive;
    float _beamwidth;
    float _acscale;
    int _maxbranch;
    
  public:
    /// Initialize decoder object
    decoder(network& decodingnet, frame_scorer* scorer)
      : _decodingnet(decodingnet), _scorer(scorer), 
        _maxactive(10000), _beamwidth(250.0), _acscale(0.2), _maxbranch(10) {
      initialize_ilabel_map();
    }

    /// Load mapping between FST ilabel and HMM state
    void initialize_ilabel_map() {
      if (_decodingnet.InputSymbols() == 0) {
        ERROR("Decoding net doesn't have an input symbol table");
        throw std::runtime_error("Decoding net doesn't have an input symbol table");
      }
      for (fst::StateIterator<network> stit(_decodingnet);
           ! stit.Done(); stit.Next()) {
        for (fst::ArcIterator<network> ait(_decodingnet, stit.Value());
             ! ait.Done(); ait.Next()) {
          int ilab = ait.Value().ilabel;
          const std::string& isym = _decodingnet.InputSymbols()->Find(ilab);
          int hmmst = -1;
          if (ilab == 0) {
            hmmst = -1;
          } else if (isym[0] == 'S') {
            size_t sep = isym.find(';');
            size_t len = (sep == std::string::npos)
              ? std::string::npos : sep - 1;
            try {
              hmmst = boost::lexical_cast<int>(isym.substr(1, len));
            } catch (boost::bad_lexical_cast) {
              ERROR("Failed to parse input symbol: %s", isym.c_str());
              throw;
            }
          } else {
            hmmst = -1;
          }

          while (_ilabel_to_hmm_state.size() < ilab + 1) {
            _ilabel_to_hmm_state.push_back(-1);
          }
          _ilabel_to_hmm_state[ilab] = hmmst;
        }
      }
    }

    void set_max_active(int m) { _maxactive = m; }
    void set_beam_width(float w) { _beamwidth = w; }
    void set_acoustic_scale(float s) { _acscale = s; }
    void set_max_branch(int m) { _maxbranch = m; }
    
    hypos* last_hypos() { return &(*_frame_hypos.rbegin()); }

    hypo* root_hypo() {
      return &(_frame_hypos.begin()->at(0));
    }
    
    void push_init() {
      hypos h;
      _frame_hypos.clear();
      fst_arc dummyarc(0, 0, 0.0, _decodingnet.Start());
      hypo inithypo;
      inithypo.trans.prev_fst_state = -1;
      inithypo.trans.arcs.push_back(dummyarc);
      inithypo.weight = 0;
      h.push_back(inithypo);

      _frame_hypos.insert(_frame_hypos.end(), h);
    }
    
    void push_input(const fmatrix& inp) {
      _scorer->set_frames(inp);
      for (int t = 0; t < inp.cols(); ++ t) {
        if (! this->expand_frame(t)) {
          throw no_hypothesis();
        }
      }
    }

    void prune_by_beam(hypos* phypos) {
      std::sort(phypos->begin(), phypos->end(), hypo_weight_comp());

      TRACE("    score range = [%f:%f]",
            (phypos->size() > 0) ? (*phypos)[0].weight : -1,
            (phypos->size() > 0) ? (*phypos)[phypos->size()-1].weight : -1);
      if (phypos->size() == 0) {
        return;
      }

      float beamthres = (*phypos)[0].weight + _beamwidth;
      TRACE("    beamthres = %f", beamthres);

      int cutoff_idx = phypos->size() / 2;
      int left = 0;
      int right = phypos->size();
      while (1) {
        if (cutoff_idx == phypos->size() - 1) { // If the pivot is on the tail
          if ((*phypos)[cutoff_idx].weight < beamthres) {
            cutoff_idx += 1;
          }
          break;
        }
        
        if ((*phypos)[cutoff_idx].weight > beamthres) {
          if ((*phypos)[cutoff_idx - 1].weight <= beamthres) {
            // Search finished
            break;
          }
          // Search left side
          right = cutoff_idx;
          cutoff_idx = left + (right - left) / 2;
        }
        else {
          left = cutoff_idx;
          cutoff_idx = right - (right - left) / 2;
        }
      }
      hypos::iterator cutoff = phypos->begin() + cutoff_idx;

      phypos->assign(phypos->begin(), cutoff);
    }

    void prune_by_maxactive(hypos* phypos) {
      phypos->assign(phypos->begin(),
                     phypos->begin() + std::min((int)phypos->size(),
                                                (int)_maxactive));
    }

    void find_possible_transition(std::vector<transition>* ptrs,
                                  int state, bool search_final) {
      // TO DO: do not generate redundant transition
      std::queue<transition> queue;
      queue.push(transition(state, 0.0));

      while (! queue.empty()) {
        transition tr = queue.front();
        queue.pop();
        
        for (fst::ArcIterator<network> ait(_decodingnet, tr.get_next_fst_state());
             ! ait.Done(); ait.Next()) {
          transition ntr = tr;
          fst::StdArc arc = ait.Value();
          int hmmst = _ilabel_to_hmm_state[arc.ilabel];
          
          ntr.weight = tr.weight + arc.weight.Value();
          ntr.arcs.push_back(arc);
          if (search_final) {
            if (_decodingnet.Final(arc.nextstate) !=
                fst::TropicalWeight::Zero()) {
              fst::StdArc final(0, 0, _decodingnet.Final(arc.nextstate), -1);
              ntr.weight += final.weight.Value();
              ntr.arcs.push_back(final);
              ptrs->push_back(ntr);
            } else if (hmmst < 0) { // only transit epsilon arcs
              queue.push(ntr);
            }
          } else {
            if (hmmst < 0) {
              queue.push(ntr);
            } else {
              ntr.next_hmm_state = hmmst;
              ptrs->push_back(ntr);
            }
          }
        }
      }
    }

    void fold_and_branch(const hypos& oldhypos, hypos* pnewhypos) {
      // oldhypos must be sorted
      assert (&oldhypos != pnewhypos);
      
      static std::unordered_map<int, int> fstst_to_besthypoidx;
      fstst_to_besthypoidx.clear();

      for (auto hyp : oldhypos) {
        int fstst = hyp.get_next_fst_state();
        auto pit = fstst_to_besthypoidx.find(fstst);
        if (pit == fstst_to_besthypoidx.end()) {
          pnewhypos->push_back(hyp);
          fstst_to_besthypoidx.insert(std::make_pair(fstst,
                                                     pnewhypos->size() - 1));
        } else {
          bool found = false;
          hypo& besthypo = pnewhypos->at(pit->second);
          for (auto branch : besthypo.merged_branch) {
            if (branch.signature == hyp.signature) {
              found = true;
              break;
            }
          }
          if (found) {
            // redundant hypothesis
          } else if (besthypo.merged_branch.size() < (_maxbranch - 1)) {
            // add branch and delete this from active hypo
            besthypo.merged_branch.push_back(hyp);
          }
        }
      }

    }

    bool expand_frame(int scorer_toff) {
      float minweight = HUGE_VALF; // minweight tracker for earlier pruning

      static hypos next_vector_all;
      next_vector_all.clear();

      static std::vector<transition> trans;
      trans.clear();
          
      for (hypos::iterator it = last_hypos()->begin(),
             last = last_hypos()->end(); it != last; ++ it) {

        // Expand self loop
        if (scorer_toff != 0) {
          transition looptr(it->trans.prev_fst_state, 0.0);
          looptr.self_loop = true;
          looptr.next_hmm_state = it->trans.next_hmm_state;

          fst_arc arc = it->get_last_arc();
          looptr.arcs.push_back(arc);

          float w = it->weight;

          float acscore = _scorer->get_score(scorer_toff,
                                             looptr.next_hmm_state);
          w -= acscore * _acscale;
          
          if (minweight > w) minweight = w;
          if (w < minweight + _beamwidth) { // early pruning
            hypo h_loop(&(*it), looptr, w, it->signature);
            next_vector_all.push_back(h_loop);
          }
        }

        trans.clear();
        find_possible_transition(&trans, it->get_next_fst_state(), false);
        
        for (auto trit = trans.begin(), trlast = trans.end();
             trit != trlast; ++ trit) {
          float w = it->weight + trit->weight;

          int nsign = trit->update_signature(it->signature);
            
          float acscore = _scorer->get_score(scorer_toff, trit->next_hmm_state);
          w -= acscore * _acscale;

          for (int n = 0; n < trit->arcs.size() - 1; ++ n) {
            assert(_ilabel_to_hmm_state[trit->arcs[n].ilabel] < 0);
            assert(_decodingnet.InputSymbols()->Find(trit->arcs[n].ilabel)[0] != 'S');
          }
          
          if (minweight > w) minweight = w;
          if (w < minweight + _beamwidth) { // early pruning
            hypo h_enter(&(*it), *trit, w, nsign);
            next_vector_all.push_back(h_enter);
          }
        }
      }
      if (next_vector_all.size() == 0) {
        //throw no_hypothesis();
        return false;
      }
      TRACE("t = %d: Current # of hypotheses (before pruning) = %d",
            scorer_toff, next_vector_all.size());
      prune_by_beam(&next_vector_all);
      TRACE("t = %d: Current # of hypotheses (after beam pruning) = %d",
            scorer_toff, next_vector_all.size());
      if (next_vector_all.size() == 0) {
        return false;
      }

      auto lasthypos = _frame_hypos.insert(_frame_hypos.end(), hypos());

      fold_and_branch(next_vector_all, &(*lasthypos));
      prune_by_maxactive(&(*lasthypos));
      
      TRACE("t = %d: Current # of hypotheses (after pruning) = %d",
            scorer_toff, lasthypos->size());
      if (lasthypos->size() == 0) return false;
      return true;
    }
    
    bool push_final() {
      hypos next_vector_all;
      for (hypos::iterator it = last_hypos()->begin(),
             last = last_hypos()->end(); it != last; ++ it) {

        std::vector<transition> trans;
        find_possible_transition(&trans, it->get_next_fst_state(), true);
        for (std::vector<transition>::iterator
               trit = trans.begin(), trlast = trans.end();
             trit != trlast; ++ trit) {

          float w = it->weight + trit->weight ;
          hypo h(&(*it), *trit, w, it->signature);
          next_vector_all.push_back(h);
        }
      }
      prune_by_beam(&next_vector_all);
      auto lasthypos = _frame_hypos.insert(_frame_hypos.end(), hypos());
      fold_and_branch(next_vector_all, &(*lasthypos));
      prune_by_maxactive(&(*lasthypos));
      return lasthypos->size() != 0;
    }

    int find_lattice_state(std::unordered_map<const hypo*, int>* pmap,
                           Lattice* plattice,
                           const hypo* prev_hypo) const {
      auto it = pmap->find(prev_hypo);
      int dest;
      if (it == pmap->end()) {
        dest = plattice->AddState();
        pmap->insert(std::make_pair(prev_hypo, dest));
      } else {
        dest = it->second;
      }
      return dest;
    }

    std::pair<const hypo*, int>
    generate_path(Lattice* plattice,
                  std::unordered_map<const hypo*, int>* pmap,
                  fst::SymbolTable& isymtab,
                  fst::SymbolTable& osymtab,
                  const hypo* hyp, int end_t, int tailst) const {
      int beg_t = end_t - 1;
      float total_arc_weight = 0.0;
      float last_weight = hyp->weight;
      while (hyp->trans.is_self_loop()) {
        assert (hyp->trans.arcs.size() == 1);
        // ^ self loop must not have multiple arcs
        total_arc_weight += hyp->trans.weight;
        hyp = hyp->prev_hypo;
        beg_t -= 1;
      }
      float prev_weight = hyp->prev_hypo->weight;
      float acoustic_weight = last_weight - prev_weight - total_arc_weight;

      int head = -1;
      if (hyp->trans.has_epsilon()) {
        head = plattice->AddState();
      } else {
        head = find_lattice_state(pmap, plattice, hyp->prev_hypo);
      }
      int ilabel = hyp->trans.get_last_arc().ilabel,
        olabel = hyp->trans.get_last_arc().olabel;
      std::string
        isym = _decodingnet.InputSymbols()->Find(ilabel),
        osym = _decodingnet.OutputSymbols()->Find(olabel);

      LatticeWeight weight(acoustic_weight, total_arc_weight,
                           TimingWeight<int>(beg_t, end_t));
      LatticeArc arc(prepare_label(&isymtab, isym),
                     prepare_label(&osymtab, osym),
                     weight, tailst);
      plattice->AddArc(head, arc);

      if (hyp->trans.has_epsilon()) {
        int tail = head;
        head = find_lattice_state(pmap, plattice, hyp->prev_hypo);
        generate_eps_path(plattice, isymtab, osymtab, 
                          boost::next(hyp->trans.arcs.crbegin()),
                          hyp->trans.arcs.crend(),
                          head, tail, beg_t);
      }

      return std::make_pair(hyp->prev_hypo, beg_t);
    }
                            
    void generate_eps_path(Lattice* plattice,
                           fst::SymbolTable& isymtab,
                           fst::SymbolTable& osymtab,
                           transition::const_reverse_arc_iterator begin,
                           transition::const_reverse_arc_iterator end,
                           int headst, int tailst, int t) const {
      // generate from tail to head
      transition::const_reverse_arc_iterator last = end;
      -- last;
      for ( ; begin != end; ++ begin) {
        LatticeWeight weight(begin->weight.Value(), 0.0,
                             TimingWeight<int>(t, t));
        int nhead = (begin == last) ? headst : plattice->AddState();
        assert (_ilabel_to_hmm_state[begin->ilabel] == -1);
        std::string
          isym = _decodingnet.InputSymbols()->Find(begin->ilabel),
          osym = _decodingnet.OutputSymbols()->Find(begin->olabel);
        LatticeArc arc(prepare_label(&isymtab, isym),
                       prepare_label(&osymtab, osym), weight, tailst);
        plattice->AddArc(nhead, arc);
        tailst = nhead;
      }
    }

    float extract_lattice(Lattice* plattice, int nbranch) {
      // Lattice merges self-loop but generates epsilon transition
      if (last_hypos()->size() == 0)
        throw std::runtime_error("Could not reach to a final state");

      fst::SymbolTable isymtab, osymtab;
      isymtab.AddSymbol("<eps>", 0);
      osymtab.AddSymbol("<eps>", 0);
      int initial_state = plattice->AddState();
      plattice->SetStart(initial_state);

      std::unordered_map<const hypo*, int> hypo_to_latst;
      // ^ hypo is a part of arc, and this maps arc to dest state
      hypo_to_latst.insert(std::make_pair(root_hypo(), initial_state));
      
      std::set<const hypo*> visited;
      std::queue<std::pair<const hypo*, int> > heads;
      int last_t = _frame_hypos.size() - 2;
      bool is_first = true;
      float ret = 0.0;
      
      // expand finals
      for (const hypo& hyp : *last_hypos()) {
        if (is_first) {
          ret = hyp.weight;
          is_first = false;
        }
        
        auto ait = hyp.trans.arcs.crbegin();
        assert (ait->olabel == 0 && ait->ilabel == 0 && ait->nextstate == -1);
        // ^ Check that the final arc is dummy containing final weight
        int final = plattice->AddState();
        LatticeWeight finalweight(ait->weight.Value(), 0.0,
                                  TimingWeight<int>(last_t, last_t));
        plattice->SetFinal(final, finalweight);

        if (hyp.trans.arcs.size() > 1) {
          int headst = find_lattice_state(&hypo_to_latst, plattice,
                                          hyp.prev_hypo);
          ++ ait;
          generate_eps_path(plattice, isymtab, osymtab,
                            ait, hyp.trans.arcs.crend(),
                            headst, final, last_t);
          heads.push(std::make_pair(hyp.prev_hypo, last_t));
        } else {
          hypo_to_latst.insert(std::make_pair(hyp.prev_hypo, final));
          heads.push(std::make_pair(hyp.prev_hypo, last_t));
        }
      }

      while (! heads.empty()) {
        auto pair = heads.front();
        const hypo* head = pair.first;
        int tail_t = pair.second;
        heads.pop();
        if (visited.find(head) != visited.cend()) continue;
        assert(hypo_to_latst.find(head) != hypo_to_latst.end());
        
        int tailst = hypo_to_latst[head];
        visited.insert(head);

        for (int n = 0; n < nbranch; ++ n) {
          if (n > 0 && (n - 1) >= (int) head->merged_branch.size()) break;
          const hypo& br = (n == 0) ? *head : head->merged_branch[n - 1];

          auto npair = generate_path(plattice,
                                     &hypo_to_latst,
                                     isymtab, osymtab, &br, tail_t, tailst);
          const hypo* nhead = npair.first;
          int nhead_t = npair.second;
          if (nhead_t == 0) {
          } else {
            heads.push(npair);
          }
          ++ n;
        }
      }

      plattice->SetInputSymbols(&isymtab);
      plattice->SetOutputSymbols(&osymtab);

      fst::TopSort(plattice);
      return ret;
    }
      
  };
}

#endif

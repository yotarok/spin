#include <spin/hmm/tree.hpp>
#include <spin/io/yaml.hpp>
#include <spin/utils.hpp>

#include <gear/io/logging.hpp>

namespace spin {

  std::string HMM_context::to_string() const {
    return boost::algorithm::join(label, " ")
      + " " + (boost::format("%d") % loc).str();
  }

  void HMM_context::parse_string(const std::string& s) {
    boost::algorithm::split(label, s, boost::is_any_of(" \t"));
    loc = boost::lexical_cast<int>(label.back());
    label.pop_back();
  }
  
  tree_question_ptr
  tree_question::create_question(const variant_t source, 
                                 const context_decision_tree& tree) {
    const variant_map& vmap = boost::get<variant_map>(source);
    variant_map::const_iterator tyit = vmap.find("qtype");
    if (tyit == vmap.end()) 
      throw std::runtime_error("qtype field is not found in question");
    std::string typname = boost::get<std::string>(tyit->second);
    if (typname == "context") {
      return tree_question_ptr(new context_question(source, tree));
    } 
    else if (typname == "location") {
      return tree_question_ptr(new location_question(source, tree));
    }
    else {
      throw std::runtime_error("Unknown question type: " + typname);
    }
  } 

  void context_question::read(const variant_t& source) {
    const variant_map& vmap = boost::get<variant_map>(source);
    
    variant_map::const_iterator cit = vmap.find("category");
    if (cit == vmap.end()) 
      throw std::runtime_error("category not found in context Q");
    _category_name = boost::get<std::string>(cit->second);
    
    variant_map::const_iterator ctxit = vmap.find("context");
    if (ctxit == vmap.end()) 
      throw std::runtime_error("context not found in context Q");
    _target_id = boost::get<int>(ctxit->second);      
  }
  
  void context_question::write(variant_t* dest) const {
    variant_map prop;
    prop["qtype"] = std::string("context");
    prop["category"] = _category_name;
    prop["context"] = _target_id;
    *dest = prop;
  }

  bool context_question::question(const HMM_context& ctx) const {
    int idx = tree().left_context_length() + _target_id;
    if (idx < 0 || idx > ctx.label.size()) {
      throw std::runtime_error("question error");
    }
    
    const std::set<std::string>& category = tree().category(_category_name);
    
    return category.find(ctx.label[idx]) != category.end();
  }

  void location_question::read(const variant_t& source) {
    const variant_map& vmap = boost::get<variant_map>(source);
    
    variant_map::const_iterator lit = vmap.find("value");
    if (lit == vmap.end()) 
      throw std::runtime_error("location not found in location Q");
      _location = boost::get<int>(lit->second);
  }

  void location_question::write(variant_t* dest) const {
    *dest = variant_map();
    variant_map& prop = boost::get<variant_map>(*dest);
    prop["qtype"] = std::string("location");
    prop["value"] = _location;
  }

  bool location_question::question(const HMM_context& ctx) const {
    return ctx.loc == _location;
  }

  context_decision_tree_node::~context_decision_tree_node() {
    try {
      if (is_true) delete is_true;
      if (is_false) delete is_false;
    } catch (...) { }
  }

  void context_decision_tree_node::read_question_and_true(const variant_t& source){
    const variant_map& vmap = boost::get<variant_map>(source);
    variant_map::const_iterator qit = vmap.find("question");
    if (qit == vmap.end()) 
      throw std::runtime_error("Question not found");
    question = tree_question::create_question(qit->second, tree);
    
    variant_map::const_iterator trit = vmap.find("isTrue");
    if (trit == vmap.end()) 
      throw std::runtime_error("IsTrue not found");
    is_true = new context_decision_tree_node(trit->second, tree);
  }


  void context_decision_tree_node::read_false(const variant_t& source) {
    const variant_map& vmap = boost::get<variant_map>(source);
    
    variant_map::const_iterator fait = vmap.find("isFalse");
    if (fait == vmap.end()) {
      //is_false = new context_decision_tree_node(-1, tree);
      is_false = 0; 
    } else {
      is_false = new context_decision_tree_node(fait->second, tree);
    }
  }

  void context_decision_tree_node::read(const variant_t& source) {
    /*
    if (source.which() == VARIANT_INT) {
      is_leaf = true;
      value = boost::get<int>(source);
      return;
    }
    is_leaf = false;
    */
    if (source.which() == VARIANT_VECTOR) { // switch-like representation
      is_leaf = false;
      const variant_vector& swtch = 
        boost::get<variant_vector>(source);
      read_question_and_true(swtch[0]);
      variant_vector false_list(boost::next(swtch.begin()), swtch.end());
      if (false_list.size() == 0) {
        read_false(swtch[0]);
      } else {
        is_false = new context_decision_tree_node(false_list, tree);
      }
    } else if (source.which() == VARIANT_MAP) { // usual question
      const variant_map& vmap = boost::get<variant_map>(source);
      variant_map::const_iterator vit = vmap.find("leaf");
      if (vit == vmap.end()) {
        is_leaf = false;
        read_question_and_true(source);
        read_false(source);
      } else {
        is_leaf = true;
        value = boost::get<int>(vit->second);
        nosplit = boost::get<bool>(get_or_default(vmap, "nosplit", false));
      }
    } else {
      std::runtime_error("Cannot parse tree file: Node must be a vector or map");
    }
  }
  void context_decision_tree_node::write(variant_t* dest) {
    if (is_leaf) {
      *dest = variant_map();
      variant_map& destmap = boost::get<variant_map>(*dest);
      destmap["leaf"] = value;
      destmap["nosplit"] = nosplit;
    } else {
      if (is_false && ! is_false->is_leaf) { // vector-style output
        *dest = variant_vector();
        variant_vector& qnodes = boost::get<variant_vector>(*dest);
        context_decision_tree_node* cur = this;
        int n = 0;
        while (cur) {
          variant_map elem;
          variant_t q, t;
          cur->question->write(&(elem["question"]));
          cur->is_true->write(&(elem["isTrue"]));
          if (cur->is_false && cur->is_false->is_leaf) {
            cur->is_false->write(&(elem["isFalse"]));
            qnodes.push_back(elem);
            cur = 0;
          } else {
            qnodes.push_back(elem);
            cur = cur->is_false;
          }
        }
      } else {
        *dest = variant_map();
        variant_map& destmap = boost::get<variant_map>(*dest);
        question->write(&(destmap["question"]));
        is_true->write(&(destmap["isTrue"]));
        is_false->write(&(destmap["isFalse"]));
      }
    }
  }

  int 
  context_decision_tree_node::resolve(const HMM_context& ctx) const {
    const context_decision_tree_node* node = this; 
    while (! node->is_leaf) {
      if (node->question->question(ctx)) {
        node = node->is_true;
      } else {
        if (! node->is_false) {
          throw std::runtime_error("Falled into unspecified decision tree node");
          return -1;
        }
        node = node->is_false;
      }
    }
    return node->value;
  }

  int context_decision_tree_node::max_state_id() const {
    if (is_leaf) return value;
    else return std::max(is_true ? is_true->max_state_id() : 0,
                         is_false ? is_false->max_state_id() : 0);
  }

  context_decision_tree::~context_decision_tree() {
    if (_root) delete _root;
  }

  void context_decision_tree::read(const variant_t& source) {
    const variant_map& map = boost::get<variant_map>(source);

    variant_map::const_iterator cat_it = map.find("categories");
    if (cat_it == map.end()) 
      throw std::runtime_error("No categories found");
    read_categories(cat_it->second);
    
    variant_map::const_iterator clen_it = map.find("contextLengths");
    if (clen_it == map.end()) {
      _left_context = _right_context = 0;
    } else {
      const variant_vector& lenpair = 
        boost::get<variant_vector>(clen_it->second);
      _left_context = boost::get<int>(lenpair[0]);
      _right_context = boost::get<int>(lenpair[1]);
    }

    variant_map::const_iterator root_it = map.find("root");
    if (root_it == map.end()) 
      throw std::runtime_error("No root found");
    _root = new context_decision_tree_node(root_it->second, *this);
  }

  void context_decision_tree::write(variant_t* dest) {
    *dest = variant_map();
    variant_map& props = boost::get<variant_map>(*dest);
    variant_map cats;
    for (auto it = _categories.cbegin(), last = _categories.cend();
         it != last; ++ it) {
      variant_vector vec;
      for (auto eit = it->second.cbegin(), elast = it->second.cend();
           eit != elast; ++ eit) vec.push_back(*eit);
      cats.insert(std::make_pair(it->first, vec));
    }
    props["categories"] = cats;
    variant_vector clen;
    clen.push_back(variant_t(static_cast<int>(_left_context)));
    clen.push_back(variant_t(static_cast<int>(_right_context)));
    props["contextLengths"] = clen;

    variant_t root;
    _root->write(&(props["root"]));
  }
  
  void context_decision_tree::read_categories(const variant_t& source) {
    const variant_map& cats = boost::get<variant_map>(source);
    for (auto it = cats.cbegin(), last = cats.cend();
         it != last; ++ it) {
      std::set<std::string> labs;
      const variant_vector& vec = boost::get<variant_vector>(it->second);
      for (auto vit = vec.cbegin(), vlast = vec.cend(); vit != vlast; ++ vit) {
        labs.insert(boost::get<std::string>(*vit));
      }
      _categories.insert(std::make_pair(it->first, labs));
    }
  }

  const std::set<std::string>&
  context_decision_tree::category(const std::string& category_name) const {
    std::map<std::string, std::set<std::string> >::const_iterator
      it = _categories.find(category_name);
    if (it == _categories.end()) {
      throw std::runtime_error("Invalid category name [" + category_name + "]");
    }
    return it->second;
  }

  void 
  context_decision_tree::obtain_label_set(std::set<std::string>* dest) const {
    
    for (std::map<std::string, std::set<std::string> >::const_iterator
           it = _categories.begin(), last = _categories.end();
         it != last; ++ it) {
      dest->insert(it->second.begin(), it->second.end());
    }
  }

  int context_decision_tree::nstates() const {
    return _root->max_state_id() + 1;
  }

  
  fst::script::VectorFstClass
  context_decision_tree::create_HC_FST(int nstate,
                                       const std::vector<std::string>& disamb)
    const {
    
    std::set<std::string> labs;
    obtain_label_set(&labs);
    labs.insert("");

    fst::StdVectorFst fst;
    fst::SymbolTable isym, osym;
    isym.AddSymbol("<eps>", 0);
    //int boundary = isym.AddSymbol("##");
    
    osym.AddSymbol("<eps>", 0);
    int start_state = fst.AddState();
    fst.SetStart(start_state);
    int final_state = fst.AddState();
    fst.SetFinal(final_state, fst::TropicalWeight::One());
    
    std::map<std::string, int> statenames;

    int clen = left_context_length() + 1 + right_context_length();

    std::set<int> disamb_loop_added;
    
    for (auto cit = make_combinatorial_iterator(labs, clen);
         ! cit.done(); cit.next()) {
      auto labs = cit.value();
      std::string centlabel = labs[left_context_length()];
      if (centlabel.size() == 0) continue;

      std::cout << boost::algorithm::join(labs, "\t") << std::endl;
      
      int lablabel = prepare_label(&osym, centlabel);

      std::vector<std::string> prevstlabs(labs.begin(), boost::prior(labs.end()));
      std::string prevstname = boost::algorithm::join(prevstlabs, "\t");
      if (statenames.find(prevstname) == statenames.end()) {
        statenames.insert(std::make_pair(prevstname, fst.AddState()));
      }
      int prevst = statenames[prevstname];

      std::vector<std::string> nextstlabs(boost::next(labs.begin()), labs.end());
      std::string nextstname = boost::algorithm::join(nextstlabs, "\t");
      if (statenames.find(nextstname) == statenames.end()) {
        statenames.insert(std::make_pair(nextstname, fst.AddState()));
      }
      int nextst = statenames[nextstname];

      std::cout << prevstname << " ---> " << nextstname << std::endl;
      int prev = prevst;
      for (int loc = 0; loc < nstate; ++ loc) {
        int next = fst.AddState();
        int olabel = (loc == 0) ? lablabel : 0;

        HMM_context ctx;
        ctx.label = labs;
        ctx.loc = loc;
        int st = resolve(ctx);
        int ilabel = prepare_label(&isym,
                                   "S" + boost::lexical_cast<std::string>(st)
                                   + ";" + centlabel + ";" + boost::lexical_cast<std::string>(loc));
        std::string os = (loc == 0) ? centlabel : "";
        std::cout << ("S" + boost::lexical_cast<std::string>(st)) << ":" << os << "\t";
        fst.AddArc(prev,
                   fst::StdArc(ilabel, olabel, 
                               fst::TropicalWeight::One(), next));
        prev = next;
      }
      std::cout << std::endl;

      // Phone boundary
      int boundary = prepare_label(&isym,
                                   "#]" + centlabel);
      fst.AddArc(prev,
                 fst::StdArc(boundary, 0,
                             fst::TropicalWeight::One(), nextst));
      prev = nextst;

      // Consume disambiguators
      if (disamb_loop_added.find(prev) == disamb_loop_added.end()) {

        for (auto it = disamb.cbegin(), last = disamb.cend(); it != last; ++ it) {
          if (it->size() == 0) {
            ERROR("Empty disambiguation symbol is not allowed");
            throw std::runtime_error("Empty disambiguation symbol is not allowed");
          }
          int ilabel = prepare_label(&isym, *it);
          // ^ we decided to preserve disambiguator for further optimization
          int olabel = prepare_label(&osym, *it);
          fst.AddArc(prev,
                     fst::StdArc(ilabel, olabel,
                                 fst::TropicalWeight::One(), prev));
        }
        disamb_loop_added.insert(prev);
      }
    }
    std::cout << "# states = " << statenames.size() << std::endl;

    std::string begin_pred;
    for (int i = 0; i < left_context_length(); ++ i) begin_pred += "\t";
    std::string end_pred;
    for (int i = 0; i < right_context_length(); ++ i) end_pred += "\t";
    for (auto it = statenames.cbegin(), last = statenames.cend();
         it != last; ++ it) {
      if (boost::starts_with(it->first, begin_pred)) {
        fst.AddArc(start_state,
                   fst::StdArc(0, 0, 
                               fst::TropicalWeight::One(), it->second));
        
      }
      if (boost::ends_with(it->first, end_pred)) {
        fst.AddArc(it->second,
                   fst::StdArc(0, 0, 
                               fst::TropicalWeight::One(), final_state));
        
      }
    }
    fst.SetInputSymbols(&isym);
    fst.SetOutputSymbols(&osym);

    return fst::script::VectorFstClass(fst);
  }

}

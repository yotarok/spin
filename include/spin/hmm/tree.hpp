#ifndef spin_hmm_tree_hpp_
#define spin_hmm_tree_hpp_

#include <spin/types.hpp>
#include <boost/utility.hpp>
#include <spin/io/yaml.hpp>

// Deserializing tree is bit tricky for readability
// If question node is list, the it's red as case-statement and expanded to 
// binary tree, even though it seems like multiway tree in file format

namespace spin {
  class context_decision_tree;

  struct HMM_context {
    std::vector<std::string> label;
    int loc;

    std::string to_string() const;
    void parse_string(const std::string& s);

    bool operator < (const HMM_context& oth) const {
      if (label == oth.label) {
        return loc < oth.loc;
      }
      return label < oth.label;
    }
    bool operator== (const HMM_context& oth) const {
      return label == oth.label && loc == oth.loc;
    }
    bool operator != (const HMM_context& oth) const {
      return ! (*this == oth);
    }
  };

  class tree_question {
    const context_decision_tree& _tree;

  protected:
    tree_question(const context_decision_tree& tree) : _tree(tree) { }
    const context_decision_tree& tree() const { return _tree; }

  public:
    virtual ~tree_question() { }
    virtual bool question(const HMM_context& ctx) const = 0;
    virtual void write(variant_t* dest) const = 0;
    static boost::shared_ptr<tree_question>
    create_question(const variant_t source, 
                    const context_decision_tree& tree);
  };
  typedef boost::shared_ptr<tree_question> tree_question_ptr;

  class context_question : public tree_question {
    std::string _category_name;
    int _target_id;

  public:
    context_question(const variant_t& source, 
                     const context_decision_tree& tree) : tree_question(tree) {
      read(source);
    }
    context_question(const context_decision_tree& tree, const std::string& cn, int target)
      : tree_question(tree), _category_name(cn), _target_id(target) {
    }
    virtual ~context_question() { }

    void read(const variant_t& source);
    void write(variant_t* dest) const;

    virtual bool 
    question(const HMM_context& ctx) const;
  };

  class location_question : public tree_question {
    int _location;
  public:
    location_question(const variant_t& source,
                      const context_decision_tree& tree) : tree_question(tree) {
      read(source);
    }
    location_question(const context_decision_tree& tree, int loc)
      : tree_question(tree), _location(loc) { }
    virtual ~location_question() { }

    void read(const variant_t& source);
    void write(variant_t* dest) const;

    virtual bool 
    question(const HMM_context& ctx) const;
  };

  struct context_decision_tree_node {
    const context_decision_tree& tree;
    tree_question_ptr question;
    context_decision_tree_node* is_true;
    context_decision_tree_node* is_false;
    bool is_leaf;
    bool nosplit;
    int value;

    context_decision_tree_node(int v, const context_decision_tree& tr)
      : tree(tr), question(), is_true(0), is_false(0), 
        is_leaf(true), value(v), nosplit(false) {
    }

    context_decision_tree_node(const variant_t& source, 
                               const context_decision_tree& tr)
      : tree(tr), question(), is_true(0), is_false(0), is_leaf(false), value(-1) {
      read(source);
    }
    
    virtual ~context_decision_tree_node();

    void read_question_and_true(const variant_t& source);

    void read_false(const variant_t& source);

    void read(const variant_t& source);
    void write(variant_t* dest);

    int resolve(const HMM_context& ctx) const;
                //const std::vector<std::string>& labels, int state_location) const;

    int max_state_id() const;
  };

  class context_decision_tree {
    size_t _left_context, _right_context;
    std::map<std::string, std::set<std::string> > _categories;
    context_decision_tree_node* _root;

  public:
    context_decision_tree(const variant_t& source) : _root(0) {
      read(source);
    }
    virtual ~context_decision_tree();

    void read(const variant_t& source);
    void write(variant_t* dest);

    void read_categories(const variant_t& source);

    int resolve(const HMM_context& ctx) const {
      return _root->resolve(ctx);
    }
    const context_decision_tree_node* root() const { return _root; }
    context_decision_tree_node* root() { return _root; }

    int left_context_length() const { return _left_context; }
    int right_context_length() const { return _right_context; }
    void set_left_context_length(int n) { _left_context = n; }
    void set_right_context_length(int n) { _right_context = n; }

    const std::set<std::string>&
    category(const std::string& category_name) const;

    const std::map<std::string, std::set<std::string> >&
    categories() const { return _categories; }

    std::map<std::string, std::set<std::string> >&
    categories() { return _categories; }
    
    void obtain_label_set(std::set<std::string>* dest) const;
    int nstates() const;

    fst::script::VectorFstClass create_HC_FST(int nstate,
                                              const std::vector<std::string>&
                                              disamb) const;
  };

}
#endif


#include <gear/io/logging.hpp>
#include <gear/tool/args.hpp>
#include <gear/tool/main.hpp>
#include <gear/io/matrix.hpp>

#include <tclap/CmdLine.h>
#include <iostream>
#include <streambuf>
#include <fstream>

#include <spin/corpus/corpus.hpp>
#include <spin/io/variant.hpp>
#include <spin/fst/linear.hpp>
#include <spin/fscorer/diaggmm.hpp>
#include <spin/hmm/tree.hpp>
#include <spin/hmm/treestat.hpp>

#include <boost/random.hpp>

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, tree, 
                   ("t", "tree", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<size_t>, nfeature,
                   ("d", "nfeature", "", true, 0, "D")),
                  (TCLAP::ValueArg<size_t>, nmix,
                   ("m", "nmix", "", false, 1, "M")),
                  (TCLAP::ValueArg<std::string>, treestat,
                   ("", "treestat", "Used to determine initial value if specified",
                    false, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, gmm,
                   ("", "gmm", "GMMs used for computing the initial statistics",
                    false, "", "FILE")),
                  (TCLAP::ValueArg<float>, delta,
                   ("", "delta", "Deviation factor",
                    false, 0.01, "delta")),
                  (TCLAP::ValueArg<int>, seed,
                   ("", "seed", "Random seed",
                    false, 41, "seed")),
                  (TCLAP::SwitchArg, write_text,
                   ("", "write-text", ""))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t tree_src;
    load_variant(&tree_src, arg.tree.getValue());
    context_decision_tree tree(tree_src);

    size_t S = tree.nstates(), M = arg.nmix.getValue(),
      D = arg.nfeature.getValue();

    boost::optional<tree_statistics> treestat;
    if (arg.treestat.isSet()) {
      variant_t treestat_src;
      load_variant(&treestat_src, arg.treestat.getValue());
      treestat = tree_statistics(treestat_src);
    }


    diagonal_GMM_parameter gmm;
    typedef diagonal_GMM_parameter::statscalar statscalar;
    typedef diagonal_GMM_parameter::statvector statvector;
    typedef diagonal_GMM_parameter::statmatrix statmatrix;
   

    gmm.means() = fmatrix::Zero(D, S * M);
    gmm.sqrtprecs() = fmatrix::Constant(D, S * M, 1.0);
    if (treestat) {
      tree_membership membership;
      treestat->compute_membership(tree, &membership);
      INFO("Tree statistics are given."
           " Compute initial value based on %d tree stats", membership.size());
      
      boost::random::variate_generator<
        boost::random::mt19937,
        boost::random::normal_distribution<float> >
        rng(boost::random::mt19937(arg.seed.getValue()),
            boost::random::normal_distribution<float>());

      std::set<int> initialized; // just for checking 

      tree_Gaussian_statistics glob_acc;
      for (auto it = membership.cbegin(), last = membership.cend();
           it != last; ++ it) {
        for (auto stit = it->second.cbegin(), stlast = it->second.cend();
             stit != stlast; ++ stit) {
          glob_acc.accumulate(*(stit->get<1>()));
        }
      }
      dvector glob_mean = glob_acc.first / glob_acc.zero;
      dvector glob_var = glob_acc.second / glob_acc.zero
        - glob_mean.array().square().matrix();

      for (auto it = membership.cbegin(), last = membership.cend();
           it != last; ++ it) {
        int s = it->first->value;

        tree_Gaussian_statistics gacc;
        for (auto stit = it->second.cbegin(), stlast = it->second.cend();
             stit != stlast; ++ stit) {
          gacc.accumulate(*(stit->get<1>()));
          std::cout << "first statistics = " << std::endl
                    << stit->get<1>()->first << std::endl;
        }
        INFO("Zero for %d-th state = %f from %d statistics",
             s, gacc.zero, it->second.size());
        dvector mean = gacc.first / gacc.zero;
        dvector var = gacc.second / gacc.zero - mean.array().square().matrix();
        //var = var.cwiseMax(glob_var * 0.01);
        for (int m = 0; m < M; ++ m) {
          int gid = s * M + m;
          gmm.means().col(gid) = mean.cast<float>();
          for (int d = 0; d < D; ++ d) {
            gmm.means()(d, gid) = gmm.means()(d, gid) +
              rng() * arg.delta.getValue() * std::sqrt(var(d));
          }
          gmm.sqrtprecs().col(gid) = var.array().sqrt().inverse().matrix().cast<float>();
        }
        initialized.insert(s);
      }

      for (int s = 0; s < S; ++ s) {
        if (initialized.find(s) == initialized.end()) {
          ERROR("State %s is not initialized");
          throw std::runtime_error("Tree statistics is incomplete");
        }
      }
    }
    
    int gid = 0;
    for (int s = 0; s < S; ++ s) {
      diagonal_GMM_statedef state;
      for (int m = 0; m < M; ++ m, ++ gid) {
        state.mean_ids.push_back(gid);
        state.var_ids.push_back(gid);
      }
      state.logweights = fvector::Constant(M,
                                           - std::log(static_cast<float>(M)));
      gmm.statedefs().push_back(state);
      // Do not need to set logzs since it's not written in a file
    }

    variant_t proto_src;
    gmm.write(&proto_src);
    write_variant(proto_src, arg.output.getValue(),
                  arg.write_text.isSet(), "StacDGMM");
    return 0;
  }
  

}

int main(int argc, char* argv[]) {
  return gear::wrap_main("GMM initializer",
                          argc, argv, spin::tool_main);
}


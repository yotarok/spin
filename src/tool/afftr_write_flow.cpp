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

namespace spin {
  DEFINE_ARGCLASS(Arg, (gear::common_args),
                  (TCLAP::ValueArg<std::string>, input, 
                   ("i", "input", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, output,
                   ("o", "output", "", true, "", "FILE")),
                  (TCLAP::ValueArg<std::string>, nodename,
                   ("n", "nodename", "", false, "transform", "NAME"))
                  );

  int tool_main(Arg& arg, int argc, char* argv[]) {
    variant_t trans_src;
    load_variant(&trans_src, arg.input.getValue());
    variant_map trans = boost::get<variant_map>(trans_src);

    std::string nodename = arg.nodename.getValue();
    std::ofstream ofs(arg.output.getValue().c_str());


    // No emitter, so far
    ofs << "node:" << std::endl
        << "  - name: " << nodename << std::endl
        << "    type: affine_transform" << std::endl;
      
    if (trans.find("weight") != trans.end()) {
      ofs << "    weight:" << std::endl;
      ofs << "      data:" << std::endl;
      fmatrix w = boost::get<fmatrix>(trans["weight"]);
      for (int row = 0; row < w.rows(); ++ row) {
        ofs << "        - ";
        for (int col = 0; col < w.cols(); ++ col) {
          ofs << ((col == 0) ? '[' : ',') << w(row, col);
        }
        ofs << " ]" << std::endl;
      }
    }

    if (trans.find("bias") != trans.end()) {
      ofs << "    bias: " << std::endl;
      ofs << "      transpose: true" << std::endl;
      ofs << "      data: ";
      fmatrix b = boost::get<fmatrix>(trans["bias"]);
      for (int row = 0; row < b.rows(); ++ row) {
        ofs << ((row == 0) ? '[' : ',') << b(row, 0);
      }
      ofs << " ]" << std::endl;
    }

    if (trans.find("scale") != trans.end()) {
      ofs << "    scale: " << std::endl;
      ofs << "      transpose: true" << std::endl;
      ofs << "      data: ";
      fmatrix s = boost::get<fmatrix>(trans["scale"]);
      for (int row = 0; row < s.rows(); ++ row) {
        ofs << ((row == 0) ? '[' : ',') << s(row, 0);
      }
      ofs << " ]" << std::endl;
    }
    ofs << "connection:" << std::endl
        << "  - { from: _input, to: " << nodename << " }" << std::endl
        << "  - { from: " << nodename << ", to: _output }" << std::endl;
    return 0;
  }
}

int main(int argc, char* argv[]) {
  return gear::wrap_main("Write Gear flow file based on the provided affine transform",
                          argc, argv, spin::tool_main);
}

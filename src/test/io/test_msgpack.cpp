#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>

#include <spin/io/msgpack.hpp>
#include <msgpack.hpp>
#include "../testutil.hpp"
#include <fst/script/print.h>

namespace {
  using namespace spin;

  TEST(msgpack_io_test, serialize_nil) {
    variant_t obj = nil_t();
    std::ostringstream oss;
    msgpack::pack(oss, obj);
      
    msgpack::unpacked msg;
    msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
    msgpack::object red_obj = msg.get();
    std::cout << red_obj << std::endl;
    variant_t loaded;
    red_obj.convert(&loaded);
    ASSERT_NO_THROW(boost::get<nil_t>(loaded));
  }

  TEST(msgpack_io_test, serialize_bool) {
    const bool bs[] = { true, false };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      ASSERT_NO_THROW(boost::get<bool>(loaded));
      ASSERT_EQ(bs[i], boost::get<bool>(loaded));
    }
  }

  TEST(msgpack_io_test, serialize_int) {
    const int bs[] = { 1, -1, 10000, -10000, 1000000000, -1000000000 };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      ASSERT_NO_THROW(boost::get<int>(loaded));
      ASSERT_EQ(bs[i], boost::get<int>(loaded));
    }
  }

  TEST(msgpack_io_test, serialize_double) {
    const double bs[] = { M_PI, M_E, M_SQRT2 };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      ASSERT_NO_THROW(boost::get<double>(loaded));
      ASSERT_EQ(bs[i], boost::get<double>(loaded)); // can we assume exact identity?
    }
  }

  TEST(msgpack_io_test, serialize_string) {
    const char* bs[] = {"I mistook password", "3 times, and", "I cannot login anymore"};
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = std::string(bs[i]);
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      ASSERT_NO_THROW(boost::get<std::string>(loaded));
      ASSERT_EQ(bs[i], boost::get<std::string>(loaded)); // can we assume exact identity?
    }
  }
  
  TEST(msgpack_io_test, serialize_fmatrix) {
    int rows[] = { 4, 267, 2019 };
    int cols[] = { 1, 236, 2018 };
    for (int i = 0; i < const_array_size(rows); ++ i) {
      fmatrix mat1 = fmatrix::Random(rows[i], cols[i]);
      variant_t obj = mat1;
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      //std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      fmatrix mat2 = boost::get<fmatrix>(loaded);
      ASSERT_MATRIX_NEAR(mat1, mat2, 0.0001);
    }
  }

  TEST(msgpack_io_test, serialize_int_matrix) {
    int rows[] = { 4, 267, 2019 };
    int cols[] = { 1, 236, 2018 };
    for (int i = 0; i < const_array_size(rows); ++ i) {
      intmatrix mat1 = intmatrix::Random(rows[i], cols[i]);
      variant_t obj = mat1;
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      //std::cout << red_obj << std::endl;
      variant_t loaded;
      red_obj.convert(&loaded);
      intmatrix mat2 = boost::get<intmatrix>(loaded);
      ASSERT_MATRIX_NEAR(mat1, mat2, 0.0001);
    }
  }

  TEST(msgpack_io_test, serialize_ext_ref) {
    const char* uris[] = { 
      "s3://hoge/huge",
      "http://aaa/bbb",
      "/home/yotaro/foobar.baz"
    };
    const char* formats[] = {
      "text/plain",
      "audio/wave",
      "application/spin-super-format"
    };
    for (int i = 0; i < const_array_size(uris); ++ i) {
      ext_ref ref1(uris[i], formats[i]);
      std::cout << ref1 << std::endl;
      variant_t obj = ref1;
      std::ostringstream oss;
      msgpack::pack(oss, obj);
      
      msgpack::unpacked msg;
      msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
      msgpack::object red_obj = msg.get();
      variant_t loaded;
      red_obj.convert(&loaded);
      ext_ref ref2 = boost::get<ext_ref>(loaded);
      std::cout << ref2 << std::endl;
      ASSERT_EQ(ref1, ref2);
    }
  }

  TEST(msgpack_io_test, serialize_fst_basic) {
    fst::StdVectorFst fst1_raw;
    fst1_raw.AddState();
    fst1_raw.SetStart(0);
    fst1_raw.AddState();
    fst1_raw.AddArc(0, fst::StdArc(1, 2, 123.4, 1));
    fst1_raw.SetFinal(1, 5.6);

    fst::script::VectorFstClass fst1(fst1_raw);
    variant_t obj = fst1;

    std::ostringstream oss;
    msgpack::pack(oss, obj);
    
    msgpack::unpacked msg;
    msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
    msgpack::object red_obj = msg.get();
    variant_t loaded;
    red_obj.convert(&loaded);
    fst::script::VectorFstClass fst2 = boost::get<fst::script::VectorFstClass>(loaded);
    fst::script::PrintFst(fst2, std::cout, "", 0, 0, 0, false, true);
    const fst::Fst<fst::StdArc>* fst2_raw = fst2.GetFst<fst::StdArc>(); //dynamic_cast<fst::StdVectorFst*>();
    ASSERT_TRUE(0 != fst2_raw);

    //std::cout << "RETRIEVED" << std::endl;
    //fst2_raw->Write(std::cout, fst::FstWriteOptions());
    //std::cout << "=========" << std::endl;
    
    int nstate = 0;
    for (fst::StateIterator<fst::Fst<fst::StdArc> > 
           stit(*fst2_raw); !stit.Done(); stit.Next()) {
      ASSERT_LT(nstate, 2);
      int narc = 0;
      for (fst::ArcIterator<fst::Fst<fst::StdArc> > ait(*fst2_raw, stit.Value()); 
           ! ait.Done(); ait.Next()) {
        ASSERT_LE(narc, 0);
        ASSERT_EQ(0, stit.Value());
        ASSERT_EQ(1, ait.Value().nextstate);
        ASSERT_EQ(1, ait.Value().ilabel);
        ASSERT_EQ(2, ait.Value().olabel);
        ASSERT_NEAR(123.4, ait.Value().weight.Value(), 0.00001);
        narc += 1;
      }
      nstate += 1;
    }
    ASSERT_EQ(2, nstate);
  }

  TEST(msgpack_io_test, serialize_int_vector) {
    std::vector<variant_t> v1;
    v1.push_back(0);
    v1.push_back(1);
    v1.push_back(2);
    variant_t obj = v1;

    std::ostringstream oss;
    msgpack::pack(oss, obj);
    
    msgpack::unpacked msg;
    msgpack::unpack(msg, oss.str().c_str(), oss.str().size());
    msgpack::object red_obj = msg.get();
    std::cout << red_obj << std::endl;
    variant_t loaded;
    red_obj.convert(&loaded);
    std::vector<variant_t> v2 = boost::get<std::vector<variant_t> >(loaded);
    ASSERT_EQ(3, v2.size());
    for (int i = 0; i < 3; ++ i) {
      ASSERT_EQ(i, boost::get<int>(v2[i]));
    }
  }
  
}

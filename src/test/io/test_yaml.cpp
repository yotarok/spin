#include <gtest/gtest.h>

#include <spin/types.hpp>
#include <spin/utils.hpp>
#include <spin/corpus/corpus.hpp>

#include <spin/io/yaml.hpp>
#include "../testutil.hpp"
#include <fst/script/print.h>

namespace {
  using namespace spin;

  TEST(yaml_io_test, serialize_nil) {
    variant_t obj = nil_t();
    std::ostringstream oss;
    oss << YAML::Dump(make_node_from_variant(obj));

    std::cout << "TEST:" << std::endl << oss.str() << std::endl;
    variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
    std::cout << obj2.which() << std::endl;
    ASSERT_NO_THROW(boost::get<nil_t>(obj2));
  }

  TEST(yaml_io_test, serialize_bool) {
    const bool bs[] = { true, false };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));
      
      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      ASSERT_EQ(bs[i], boost::get<bool>(obj2));
    }
  }

  TEST(yaml_io_test, serialize_int) {
    const int bs[] = { 1, -1, 10000, -10000, 1000000000, -1000000000 };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));
      std::cout << "TEST:" << YAML::Dump(make_node_from_variant(obj)) << std::endl;
      
      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      std::cout << "which:" << obj2.which() << std::endl;
      ASSERT_EQ(bs[i], boost::get<int>(obj2));
    }
  }

  TEST(yaml_io_test, serialize_double) {
    const double bs[] = { M_PI, M_E, M_SQRT2, 1.0 };
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = bs[i];
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));
      std::cout << "TEST:" << YAML::Dump(make_node_from_variant(obj)) << std::endl;
      
      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      std::cout << "which:" << obj2.which() << std::endl;
      ASSERT_NEAR(bs[i], boost::get<double>(obj2), 0.0001);
    }
  }

  TEST(yaml_io_test, serialize_string) {
    const char* bs[] = {"- I mistook \"password\"", 
                        "3 ``times'', and", "I cannot login { anymore }",
                        "n"};
    for (int i = 0; i < const_array_size(bs); ++ i) {
      variant_t obj = std::string(bs[i]);
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));
      std::cout << "TEST:" << YAML::Dump(make_node_from_variant(obj)) << std::endl;
      
      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      std::cout << "which:" << obj2.which() << std::endl;
      ASSERT_EQ(bs[i], boost::get<std::string>(obj2));
    }
  }

  TEST(yaml_io_test, tag) {
    std::cout << YAML::Load("!!str true") << std::endl;
    std::cout << "tag of quoted string = " << YAML::Load("\"n\"").Tag() << std::endl;
  }

  TEST(yaml_io_test, serialize_fmatrix) {
    int rows[] = { 4, 26, 5, 15};
    int cols[] = { 1, 23, 3, 16};
    char type[] = { 'R', 'R', '1', '1'};
    for (int i = 0; i < const_array_size(rows); ++ i) {
      fmatrix mat1;
      if (type[i] == 'R')
        mat1 = fmatrix::Random(rows[i], cols[i]);
      else if (type[i] == '1') 
        mat1 = fmatrix::Constant(rows[i], cols[i], 1.0);
      
      variant_t obj = mat1;
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));
      std::cout << YAML::Dump(make_node_from_variant(obj)) << std::endl;

      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      ASSERT_MATRIX_NEAR(mat1, boost::get<fmatrix>(obj2), 0.0001);
    }
  }

  TEST(yaml_io_test, serialize_intmatrix) {
    int rows[] = { 4, 26};
    int cols[] = { 1, 23};
    for (int i = 0; i < const_array_size(rows); ++ i) {
      intmatrix mat1 = intmatrix::Random(rows[i], cols[i]);
      variant_t obj = mat1;
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));

      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      ASSERT_MATRIX_NEAR(mat1, boost::get<intmatrix>(obj2), 0.0001);
    }
  }

  TEST(yaml_io_test, serialize_ext_ref) {
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
      variant_t obj = ref1;
      std::ostringstream oss;
      oss << YAML::Dump(make_node_from_variant(obj));

      variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
      ASSERT_EQ(ref1, boost::get<ext_ref>(obj2));
    }
  }

  TEST(yaml_io_test, serialize_fst_basic) {
    fst::StdVectorFst fst1_raw;
    fst1_raw.AddState();
    fst1_raw.SetStart(0);
    fst1_raw.AddState();
    fst1_raw.AddArc(0, fst::StdArc(1, 2, 123.4, 1));
    fst1_raw.SetFinal(1, 5.6);

    fst::SymbolTable isym;
    fst::SymbolTable osym;
    isym.AddSymbol("TheInput", 1);
    osym.AddSymbol("TheOutput", 2);
    fst1_raw.SetInputSymbols(&isym);
    fst1_raw.SetOutputSymbols(&osym);

    fst::script::VectorFstClass fst1(fst1_raw);
    variant_t obj = fst1;

    std::ostringstream oss;
    oss << YAML::Dump(make_node_from_variant(obj));

    std::cout << oss.str() << std::endl;
    variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
    fst::script::VectorFstClass fst2 = boost::get<fst::script::VectorFstClass>(obj2);
    fst::script::PrintFst(fst2, std::cout, "", 
                          fst2.InputSymbols(), fst2.OutputSymbols(), 
                          0, false, true);
    const fst::Fst<fst::StdArc>* fst2_raw = fst2.GetFst<fst::StdArc>();
    int nstate = 0;
    for (fst::StateIterator<fst::Fst<fst::StdArc> > 
           stit(*fst2_raw); ! stit.Done(); stit.Next()) {
      ASSERT_LT(nstate, 2);
      int narc = 0;
      for (fst::ArcIterator<fst::Fst<fst::StdArc> > ait(*fst2_raw, stit.Value()); 
           ! ait.Done(); ait.Next()) {
        ASSERT_LE(narc, 0);
        ASSERT_EQ(0, stit.Value());
        ASSERT_EQ(1, ait.Value().nextstate);
        ASSERT_EQ("TheInput", fst2_raw->InputSymbols()->Find(ait.Value().ilabel));
        ASSERT_EQ("TheOutput", fst2_raw->OutputSymbols()->Find(ait.Value().olabel));
        ASSERT_NEAR(123.4, ait.Value().weight.Value(), 0.00001);
        narc += 1;
      }
      nstate += 1;
    }
    ASSERT_EQ(2, nstate);
  }

  TEST(yaml_io_test, serialize_int_vector) {
    std::vector<variant_t> v1;
    v1.push_back(0);
    v1.push_back(1);
    v1.push_back(2);
    variant_t obj = v1;

    std::ostringstream oss;
    oss << YAML::Dump(make_node_from_variant(obj));

    variant_t obj2 = convert_to_variant(YAML::Load(oss.str()));
    std::vector<variant_t> v2 = boost::get<std::vector<variant_t> >(obj2);
    ASSERT_EQ(3, v2.size());
    for (int i = 0; i < 3; ++ i) {
      ASSERT_EQ(i, boost::get<int>(v2[i]));
    }
  }

  TEST(yaml_io_test, load_short_corpus) {
    const char* tmpname = ::tmpnam(0);
    {
      std::ofstream ofs(tmpname);
      ofs <<
        "---\n"
        "+key: CbS-20120222-tqn_wav_a0501_wav\n"
        "waveform:\n"
        "  type: ref\n"
        "  loc: CbS-20120222-tqn/wav/a0501.wav\n"
        "  format: audio/wav\n"
        "---\n"
        "+key: CbS-20120222-tqn_wav_a0505_wav\n"
        "waveform:\n"
        "  type: ref\n"
        "  loc: CbS-20120222-tqn/wav/a0505.wav\n"
        "  format: audio/wav\n";
    }
    {
      const char* keys[] = {
        "CbS-20120222-tqn_wav_a0501_wav",
        "CbS-20120222-tqn_wav_a0505_wav"
      };
      corpus_iterator_ptr cit = make_corpus_iterator(tmpname);
      int i = 0;
      for (i = 0; ! cit->done() ; cit->next(), ++ i) {
        std::cout << keys[i] << std::endl;
        ASSERT_EQ(keys[i], cit->get_key());
      }
      ASSERT_EQ(2, i);
    }
    ::remove(tmpname);
  }

}

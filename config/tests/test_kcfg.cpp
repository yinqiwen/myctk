#include <gtest/gtest.h>
#include <kcfg_json.h>
#include <vector>
#include "kcfg.hpp"

struct TestA {
  int64_t a = 11;
  float b = 0;
  bool c = true;
  std::string d;
  std::vector<int64_t> e = {1, 2, 0, 5};

  KCFG_DEFINE_FIELDS(a, b, c, d, e)
};

TEST(Searialize, JsonTest) {
  TestA t;
  std::string s;
  kcfg::WriteToJsonString(t, s);
  printf("%s\n", s.c_str());

  s.clear();
  kcfg::WriteToJsonString(t, s, false, false);
  printf("%s\n", s.c_str());
}

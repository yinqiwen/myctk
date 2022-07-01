#include <gtest/gtest.h>
#include <kcfg_json.h>
#include <vector>
#include "kcfg.hpp"
#include "kcfg_yaml.h"

struct TestA {
  int64_t a = 11;
  float b = 0;
  bool c = true;
  std::string d;
  std::vector<int64_t> e = {1, 2, 0, 5};

  KCFG_DEFINE_FIELDS(a, b, c, d, e)

  KCFG_YAML_DEFINE_FIELDS(a, b, c, d, e)
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

TEST(Searialize, YamlTest) {
  std::string content = R"(
---
 a: 101
 b: 3.14
 c: false
 d: hello,world
 e: [1,2,3,4,5]
  )";
  TestA t;
  bool rc = kcfg::ParseFromYamlString(content, t);
  EXPECT_EQ(true, rc);
  EXPECT_EQ(101, t.a);
  EXPECT_FLOAT_EQ(3.14, t.b);
  EXPECT_EQ(false, t.c);
  EXPECT_EQ("hello,world", t.d);
  EXPECT_EQ(5, t.e.size());
  EXPECT_EQ(1, t.e[0]);
  EXPECT_EQ(5, t.e[4]);
}

#include <gtest/gtest.h>
#include "spirit_expression.h"
using namespace ssexpr;
DEFINE_EXPR_STRUCT(SubUser, (int32_t)age, (std::string)id)
DEFINE_EXPR_STRUCT(User, (double)score, (std::string)id, (SubUser*)sub)
DEFINE_EXPR_STRUCT(SubItem, (int64_t)uv, (std::string)id)
DEFINE_EXPR_STRUCT(Item, (double)score, (std::string)id, (SubItem)sub)
DEFINE_EXPR_STRUCT(Combine, (User*)user, (Item*)item, (double)score, (std::string)name)

TEST(ExprTest, StringEq) {
  SpiritExpression expr;
  ExprOptions opt;
  EXPECT_EQ(0, expr.Init(R"("hello"=="hello")", opt));
  auto val = expr.Eval();
  EXPECT_EQ(true, std::get<bool>(val));
}

TEST(ExprTest, IntNeq) {
  SpiritExpression expr;
  ExprOptions opt;
  EXPECT_EQ(0, expr.Init(R"(3!=4)", opt));
  auto val = expr.Eval();
  EXPECT_EQ(true, std::get<bool>(val));
}

TEST(ExprTest, Logic) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Combine>();
  //                         true                true                  false
  int rc = expr.Init("user.id == \"uid123\" && (item.score > 1 || item.id == \"item101\") ", opt);
  EXPECT_EQ(0, rc);
  SubUser su = {101, "random"};
  User user = {1.21, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine root = {&user, &item, 100.2, "combine"};  // root object
  auto val = expr.Eval(root);
  EXPECT_EQ(true, std::get<bool>(val));
}

TEST(ExprTest, Calc) {
  SpiritExpression expr;
  ExprOptions opt;
  // custom functions
  opt.functions["hash"] = [](const std::vector<ssexpr::Value>& args) {
    int64_t v1 = std::get<int64_t>(args[0]);
    int64_t v2 = std::get<int64_t>(args[1]);
    Value v = v1 % v2;
    return v;
  };
  opt.Init<Combine>();
  int rc =
      expr.Init("1 + 2.11 + hash(1712, 100) + user.score + (user.id == \"uid123\"?10:20)", opt);
  EXPECT_EQ(0, rc);
  SubUser su = {101, "random"};
  User user = {1.21, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine root = {&user, &item, 100.2, "combine"};  // root object
  auto val = expr.Eval(root);
  EXPECT_DOUBLE_EQ(26.32, std::get<double>(val));
}
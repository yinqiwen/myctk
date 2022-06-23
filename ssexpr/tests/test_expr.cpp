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

TEST(ExprTest, Bool) {
  SpiritExpression expr;
  ExprOptions opt;
  EXPECT_EQ(0, expr.Init(R"(true)", opt));
  auto val = expr.Eval();
  EXPECT_EQ(true, std::get<bool>(val));

  EXPECT_EQ(0, expr.Init(R"(false)", opt));
  val = expr.Eval();
  EXPECT_EQ(false, std::get<bool>(val));
}

TEST(ExprTest, Logic) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Combine>();
  // 1. Parse & Validate
  //                         true                true                  false
  int rc = expr.Init("user.id == \"uid123\" && (item.score > 1 || item.id == \"item101\") ", opt);
  EXPECT_EQ(0, rc);
  SubUser su = {101, "random"};
  User user = {1.21, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine root = {&user, &item, 100.2, "combine"};  // root object

  // 2. Eval expr
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

struct TestNoExpr : public ssexpr::NoExpr {
  std::string val = "test";
};
DEFINE_EXPR_STRUCT(Item4, (double)score, (const TestNoExpr*)test)
TEST(ExprTest, NoExpr) {
  Item4::InitExpr();
  ExprOptions opt;
  opt.Init<Item4>();
  opt.functions["func1"] = [](const std::vector<ssexpr::Value>& args) {
    const void* v1 = std::get<const void*>(args[0]);
    const TestNoExpr* test = (const TestNoExpr*)v1;
    Value v = test->val;
    return v;
  };
  SpiritExpression expr;
  int rc = expr.Init("func1(test)", opt);
  EXPECT_EQ(0, rc);

  Item4 item;
  TestNoExpr test;
  test.val = "world";
  item.test = &test;
  auto val = expr.Eval(item);
  EXPECT_EQ("world", std::get<std::string_view>(val));
}

TEST(ExprTest, DynamicVar) {
  Item4::InitExpr();
  ExprOptions opt;
  std::map<std::string, int64_t> dynamic_vars;
  dynamic_vars["v1"] = 101;
  opt.dynamic_var_access = [](const void* root,
                              const std::vector<std::string>& args) -> ssexpr::Value {
    std::map<std::string, int64_t>& dynamic_vars = *((std::map<std::string, int64_t>*)root);
    int64_t v = dynamic_vars[args[0]];
    ssexpr::Value r = v;
    return r;
  };
  opt.functions["func1"] = [](const std::vector<ssexpr::Value>& args) {
    int64_t v1 = std::get<int64_t>(args[0]);
    v1 += 100;
    Value v = v1;
    return v;
  };
  SpiritExpression expr;
  int rc = expr.Init("func1($v1)", opt);
  EXPECT_EQ(0, rc);

  auto val = expr.EvalDynamic(dynamic_vars);
  EXPECT_EQ(201, std::get<int64_t>(val));
}
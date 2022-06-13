#include <gtest/gtest.h>
#include "spirit_jit_expression.h"

using namespace ssexpr2;
TEST(ExprTest, StringEq) {
  SpiritExpression expr;
  ExprOptions opt;
  EXPECT_EQ(0, expr.Init(R"("hello"=="hello")", opt));
  auto val = expr.Eval();
  EXPECT_EQ(true, val.Get<bool>());
}

TEST(ExprTest, IntNeq) {
  SpiritExpression expr;
  ExprOptions opt;
  EXPECT_EQ(0, expr.Init(R"(3!=4)", opt));
  auto val = expr.Eval();
  EXPECT_EQ(true, val.Get<bool>());
}

DEFINE_JIT_STRUCT(SubItem1, (double)score, (std::string)id, (int64_t)vv, (int64_t)uv)
DEFINE_JIT_STRUCT(Item1, (double)score, (std::string)id, (int64_t)vv, (SubItem1)sub)

TEST(ExprTest, PODVar) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  EXPECT_EQ(0, expr.Init(R"(vv > 100 ? 101:201)", opt));
  Item1 item;
  item.vv = 102;
  auto val = expr.Eval(item);
  EXPECT_EQ(101, val.Get<int64_t>());

  item.vv = 100;
  val = expr.Eval(item);
  EXPECT_EQ(201, val.Get<int64_t>());
}

TEST(ExprTest, SubPODVar) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  EXPECT_EQ(0, expr.Init(R"(sub.vv > 100 ? 1000:500)", opt));
  Item1 item;
  item.sub.vv = 102;
  auto val = expr.Eval(item);
  EXPECT_EQ(1000, val.Get<int64_t>());

  item.sub.vv = 100;
  val = expr.Eval(item);
  EXPECT_EQ(500, val.Get<int64_t>());
}

static Value cfunc0() {
  Value v;
  v.Set<int64_t>(100);
  return v;
}
static Value cfunc1(Value arg0) {
  Value v;
  v.Set<int64_t>(arg0.Get<int64_t>() + 100);
  return v;
}

TEST(ExprTest, CustomFunc01) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  opt.functions["cfunc0"] = (ExprFunction)cfunc0;
  opt.functions["cfunc1"] = (ExprFunction)cfunc1;
  EXPECT_EQ(0, expr.Init(R"(cfunc0() + cfunc1(sub.vv) - 100)", opt));
  Item1 item;
  item.sub.vv = 102;
  item.sub.uv = 1000;
  auto val = expr.Eval(item);
  EXPECT_EQ(202, val.Get<int64_t>());

  item.sub.vv = 100;
  val = expr.Eval(item);
  EXPECT_EQ(200, val.Get<int64_t>());
}
static Value cfunc2(Value arg0, Value arg1) {
  Value v;
  v.Set<int64_t>(arg0.Get<int64_t>() + 100 + arg1.Get<int64_t>());
  return v;
}
static Value cfunc3(Value arg0, Value arg1, Value arg2) {
  Value v;
  v.Set<int64_t>(arg0.Get<int64_t>() + 100 + arg1.Get<int64_t>() + arg2.Get<int64_t>());
  return v;
}
TEST(ExprTest, CustomFunc23) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  opt.functions["cfunc2"] = (ExprFunction)cfunc2;
  opt.functions["cfunc3"] = (ExprFunction)cfunc3;
  EXPECT_EQ(0, expr.Init(R"(cfunc2(sub.vv, sub.uv) + cfunc3(vv, sub.vv, sub.uv) - 100)", opt));
  Item1 item;
  item.vv = 500;
  item.sub.vv = 102;
  item.sub.uv = 1000;
  auto val = expr.Eval(item);
  EXPECT_EQ(2804, val.Get<int64_t>());
}

static bool test_bool_func_invoked = false;
static Value testBoolFunc() {
  Value v;
  v.Set<bool>(true);
  test_bool_func_invoked = false;
  return v;
}

TEST(ExprTest, FastAnd) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  opt.functions["testBoolFunc"] = (ExprFunction)testBoolFunc;
  EXPECT_EQ(0, expr.Init(R"( vv > 100 && testBoolFunc())", opt));
  Item1 item;
  item.vv = 50;
  auto val = expr.Eval(item);
  EXPECT_EQ(false, val.Get<bool>());
  EXPECT_EQ(false, test_bool_func_invoked);
}

TEST(ExprTest, FastOr) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item1>();
  opt.functions["testBoolFunc"] = (ExprFunction)testBoolFunc;
  EXPECT_EQ(0, expr.Init(R"( vv < 100 || testBoolFunc())", opt));
  Item1 item;
  item.vv = 50;
  auto val = expr.Eval(item);
  EXPECT_EQ(true, val.Get<bool>());
  EXPECT_EQ(false, test_bool_func_invoked);
}

struct NoExptStruct {
  std::string val = "jel";
};
static Value custom_func(Value arg0) {
  const void* p = (const void*)arg0.Get<const void*>();
  const NoExptStruct* t = (const NoExptStruct*)p;
  Value v;
  v.Set(t->val);
  return v;
}
DEFINE_JIT_STRUCT(Item2, (double)score, (NoExptStruct*)test)
TEST(ExprTest, NoExpr) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item2>();
  opt.functions["custom_func"] = (ExprFunction)custom_func;
  EXPECT_EQ(0, expr.Init(R"(custom_func(test))", opt));
  Item2 item;
  item.test = new NoExptStruct;
  item.test->val = "world";
  auto val = expr.Eval(item);
  EXPECT_EQ("world", val.Get<std::string_view>());
}

struct StringExprStruct {
  std::string val = "jel";
};
static Value get_string_func(Value arg0) {
  const void* p = (const void*)arg0.Get<const void*>();
  const StringExprStruct* t = (const StringExprStruct*)p;
  Value v;
  std::string_view s = t->val;
  v.Set(s);
  return v;
}
DEFINE_JIT_STRUCT(Item3, (double)score, (StringExprStruct*)test)
TEST(ExprTest, StringExpr) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item3>();
  opt.functions["get_str"] = (ExprFunction)get_string_func;
  EXPECT_EQ(0, expr.Init(R"(get_str(test)=="world")", opt));
  Item3 item;
  item.test = new StringExprStruct;
  item.test->val = "world";
  auto val = expr.Eval(item);
  // EXPECT_EQ("world", val.Get<std::string_view>());
  EXPECT_EQ(true, val.Get<bool>());
}

TEST(ExprTest, VarExpr) {
  SpiritExpression expr;
  ExprOptions opt;
  opt.Init<Item3>();
  std::string_view s = "hello,world";
  opt.vars["test_var"].Set(s);
  opt.vars["PI"].Set<double>(3.14);
  opt.vars["test_int"].Set<int64_t>(101);
  EXPECT_EQ(0, expr.Init(R"($test_var=="hello,world" && $PI==3.14 && ($test_int%100==1))", opt));
  // EXPECT_EQ(0, expr.Init(R"($PI==3.14 && $test_var == "hello,world")", opt));
  Item3 item;
  auto val = expr.Eval(item);
  // EXPECT_EQ("world", val.Get<std::string_view>());
  EXPECT_EQ(true, val.Get<bool>());
}
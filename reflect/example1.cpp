#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv)
void test1() {
  Item1::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item1::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    Item1 item = {1.0, "item" + std::to_string(i), i};
    auto val = item.GetFieldValue(accessors);
    std::string_view v = GetValue<std::string_view, FieldValue>(val);
    printf("id=%s\n", v.data());
  }
}

DEFINE_EXPR_STRUCT(SubItem2, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv, (SubItem2*)sub)
void test2() {
  Item2::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"sub", "score"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item2::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    SubItem2 sub = {3.14 + i, "subitem"};
    Item2 item = {1.0, "item" + std::to_string(i), i, &sub};
    auto val = item.GetFieldValue(accessors);
    double v = GetValue<double, FieldValue>(val);
    printf("sub2.score=%.2f\n", v);
  }
}

DEFINE_EXPR_STRUCT(SubItem3, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item3, (double)score, (std::string)id, (int32_t)vv, (SubItem3)sub)
void test3() {
  Item3::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"sub", "score"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item3::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    Item3 item = {1.0, "item" + std::to_string(i), i, {3.14 + i, "subitem"}};
    auto val = item.GetFieldValue(accessors);
    double v = GetValue<double, FieldValue>(val);
    printf("sub3.score=%.2f\n", v);
  }
}

int main() {
  test1();
  test2();
  test3();

  return 0;
}

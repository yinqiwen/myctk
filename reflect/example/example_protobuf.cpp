#include "data.pb.h"
#include "expr_struct_helper.h"

using namespace test_proto;
using namespace expr_struct;

DEFINE_EXPR_STRUCT_HELPER(SubData, feedid, rank, score)
DEFINE_EXPR_STRUCT_HELPER(Data, id, model_id, unit)

int main() {
  ExprStructHelper<Data>::InitExpr();
  std::vector<std::string> names = {"unit", "feedid"};
  std::vector<expr_struct::FieldAccessor> accessors;
  ExprStructHelper<Data>::GetFieldAccessors(names, accessors);

  Data test;
  test.mutable_unit()->set_feedid("name123");
  test.mutable_unit()->set_score(1.23);

  auto val = expr_struct::GetFieldValue(&test, accessors);
  std::string_view sv = expr_struct::GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());

  names = {"unit", "score"};
  ExprStructHelper<Data>::GetFieldAccessors(names, accessors);
  val = expr_struct::GetFieldValue(&test, accessors);
  double dv = expr_struct::GetValue<double, FieldValue>(val);
  printf("%.2f\n", dv);
  return 0;
}
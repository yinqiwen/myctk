#include "data.pb.h"
#include "expr_protobuf.h"

using namespace test_proto;
using namespace expr_struct;

DEFINE_PB_EXPR_HELPER(SubData, feedid, rank, score)
DEFINE_PB_EXPR_HELPER(Data, id, model_id, unit)

int main() {
  Data test;
  test.mutable_unit()->set_feedid("name123");
  test.mutable_unit()->set_score(1.23);
  ExprProtoHelper<Data>::InitExpr();
  std::vector<std::string> names = {"unit", "feedid"};
  std::vector<expr_struct::FieldAccessor> accessors;
  ExprProtoHelper<Data>::GetFieldAccessors(names, accessors);

  auto val = expr_struct::GetFieldValue(&test, accessors);
  std::string_view sv = expr_struct::GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());

  names = {"unit", "score"};
  ExprProtoHelper<Data>::GetFieldAccessors(names, accessors);
  val = expr_struct::GetFieldValue(&test, accessors);
  double dv = expr_struct::GetValue<double, FieldValue>(val);
  printf("%.2f\n", dv);
  return 0;
}
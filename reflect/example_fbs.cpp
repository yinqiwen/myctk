#include "data_generated.h"
#include "expr_struct_helper.h"
using namespace test;
using namespace expr_struct;
DEFINE_EXPR_STRUCT_HELPER(SubData, id, iid, name, score)
DEFINE_EXPR_STRUCT_HELPER(Data, name, score, unit)
int main() {
  ExprStructHelper<Data>::InitExpr();
  std::vector<std::string> names = {"unit", "id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  ExprStructHelper<Data>::GetFieldAccessors(names, accessors);

  DataT test;
  test.unit.reset(new SubDataT);
  test.name = "test_name";
  test.score = 1.23;
  test.unit->id = 100;
  test.unit->name = "sub_name";
  flatbuffers::FlatBufferBuilder builder;
  auto offset = Data::Pack(builder, &test);
  builder.Finish(offset);
  const Data* t = GetData(builder.GetBufferPointer());

  auto val = expr_struct::GetFieldValue(t, accessors);
  uint32_t v = expr_struct::GetValue<uint32_t, FieldValue>(val);
  printf("%d\n", v);

  names = {"unit", "name"};
  ExprStructHelper<Data>::GetFieldAccessors(names, accessors);
  val = expr_struct::GetFieldValue(t, accessors);
  std::string_view sv = expr_struct::GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());
  return 0;
}
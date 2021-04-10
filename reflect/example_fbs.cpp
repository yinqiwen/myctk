#include "data_generated.h"
using namespace test;
using namespace expr_struct;
int main() {
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
  printf("%.2f\n", t->score());

  Data::InitExpr();
  std::vector<std::string> names = {"unit", "id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Data::GetFieldAccessors(names, accessors);
  auto val = t->GetFieldValue(accessors);
  uint32_t v = GetValue<uint32_t, FieldValue>(val);
  printf("%d\n", v);

  names = {"unit", "name"};
  Data::GetFieldAccessors(names, accessors);
  val = t->GetFieldValue(accessors);
  std::string_view sv = GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());
  return 0;
}
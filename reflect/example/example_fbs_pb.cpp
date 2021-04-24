#include "data.pb.h"
#include "data_generated.h"
#include "expr_struct_helper.h"

using namespace expr_struct;

DEFINE_EXPR_STRUCT_HELPER(test::SubData, id, iid, name, score)
DEFINE_EXPR_STRUCT_HELPER(test::Data, name, score, unit)

DEFINE_EXPR_STRUCT_HELPER(test_proto::SubData, feedid, rank, score)
DEFINE_EXPR_STRUCT_HELPER(test_proto::Data, id, model_id, unit)

DEFINE_EXPR_STRUCT(PbFbsData, (const test_proto::Data*)pb, (const test::Data*)fbs)

int main() {
  PbFbsData::InitExpr();

  test::DataT test;
  test.unit.reset(new test::SubDataT);
  test.name = "test_fbs_name";
  test.score = 1.23;
  test.unit->id = 100;
  test.unit->name = "sub_name";
  flatbuffers::FlatBufferBuilder builder;
  auto offset = test::Data::Pack(builder, &test);
  builder.Finish(offset);
  const test::Data* fbs = test::GetData(builder.GetBufferPointer());

  test_proto::Data pb;
  pb.mutable_unit()->set_feedid("name123");
  pb.mutable_unit()->set_score(1.23);

  PbFbsData data;
  data.fbs = fbs;
  data.pb = &pb;

  std::vector<std::string> names = {"fbs", "unit", "id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  PbFbsData::GetFieldAccessors(names, accessors);
  auto val = expr_struct::GetFieldValue(&data, accessors);
  uint32_t v = expr_struct::GetValue<uint32_t, FieldValue>(val);
  printf("%d\n", v);

  names = {"pb", "unit", "feedid"};
  PbFbsData::GetFieldAccessors(names, accessors);
  val = expr_struct::GetFieldValue(&data, accessors);
  std::string_view sv = expr_struct::GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());
  return 0;
}
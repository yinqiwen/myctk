#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include "expr_struct.h"
#include "expr_struct_helper.h"
#include "ssexpr/tests/test_data.pb.h"
#include "ssexpr/tests/test_data_generated.h"
using namespace ssexpr;
DEFINE_EXPR_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv)

TEST(ReflectTest, POD) {
  Item1::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"id"};
  std::vector<FieldAccessor> accessors;
  Item1::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    Item1 item = {1.0, "item" + std::to_string(i), i};
    auto val = item.GetFieldValue(accessors);
    std::string_view v = std::get<std::string_view>(val);
    EXPECT_EQ("item" + std::to_string(i), v);
    // printf("id=%s\n", v.data());
  }
}

DEFINE_EXPR_STRUCT(SubItem2, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv, (SubItem2*)sub)
TEST(ReflectTest, SubPOD) {
  Item2::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"sub", "score"};
  std::vector<FieldAccessor> accessors;
  Item2::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    SubItem2 sub = {3.14 + i, "subitem"};
    Item2 item = {1.0, "item" + std::to_string(i), i, &sub};
    auto val = item.GetFieldValue(accessors);
    EXPECT_DOUBLE_EQ(3.14 + i, std::get<double>(val));
  }
}

DEFINE_EXPR_STRUCT(SubItem3, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item3, (double)score, (std::string)id, (int32_t)vv, (SubItem3)sub)
TEST(ReflectTest, SubPOD2) {
  Item3::InitExpr();  // root class init, 只用调一次, 可以重复调用
  std::vector<std::string> names = {"sub", "score"};
  std::vector<FieldAccessor> accessors;
  Item3::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    Item3 item = {1.0, "item" + std::to_string(i), i, {3.14 + i, "subitem"}};
    auto val = item.GetFieldValue(accessors);
    EXPECT_DOUBLE_EQ(3.14 + i, std::get<double>(val));
  }
}

DEFINE_EXPR_STRUCT_HELPER(test_fbs::SubData, id, iid, name, score)
DEFINE_EXPR_STRUCT_HELPER(test_fbs::Data, name, score, unit, value)
TEST(ReflectTest, FlatBuffers) {
  ExprStructHelper<test_fbs::Data>::InitExpr();
  std::vector<std::string> names = {"unit", "id"};
  std::vector<FieldAccessor> accessors;
  ExprStructHelper<test_fbs::Data>::GetFieldAccessors(names, accessors);

  test_fbs::DataT test;
  test.unit.reset(new test_fbs::SubDataT);
  test.name = "test_name";
  test.score = 1.23;
  test.unit->id = 100;
  test.unit->name = "sub_name";
  flatbuffers::FlatBufferBuilder builder;
  auto offset = test_fbs::Data::Pack(builder, &test);
  builder.Finish(offset);
  const test_fbs::Data* t = test_fbs::GetData(builder.GetBufferPointer());

  auto val = GetFieldValue(t, accessors);
  EXPECT_EQ(100, std::get<uint32_t>(val));

  names = {"unit", "name"};
  ExprStructHelper<test_fbs::Data>::GetFieldAccessors(names, accessors);
  val = GetFieldValue(t, accessors);
  EXPECT_EQ("sub_name", std::get<std::string_view>(val));

  // test empty string
  names = {"value"};
  ExprStructHelper<test_fbs::Data>::GetFieldAccessors(names, accessors);
  val = GetFieldValue(t, accessors);
  EXPECT_EQ("", std::get<std::string_view>(val));
}

DEFINE_EXPR_STRUCT_HELPER(test_proto::SubData, feedid, rank, score)
DEFINE_EXPR_STRUCT_HELPER(test_proto::Data, id, model_id, unit)
TEST(ReflectTest, ProtoBuffers) {
  ExprStructHelper<test_proto::Data>::InitExpr();
  std::vector<std::string> names = {"unit", "feedid"};
  std::vector<FieldAccessor> accessors;
  ExprStructHelper<test_proto::Data>::GetFieldAccessors(names, accessors);

  test_proto::Data test;
  test.mutable_unit()->set_feedid("name123");
  test.mutable_unit()->set_score(1.23);

  auto val = GetFieldValue(&test, accessors);
  EXPECT_EQ("name123", std::get<std::string_view>(val));

  names = {"unit", "score"};
  ExprStructHelper<test_proto::Data>::GetFieldAccessors(names, accessors);
  val = GetFieldValue(&test, accessors);
  EXPECT_DOUBLE_EQ(1.23, std::get<double>(val));
}

DEFINE_EXPR_STRUCT(PbFbsData, (const test_proto::Data*)pb, (const test_fbs::Data*)fbs)
TEST(ReflectTest, PbAndFbs) {
  PbFbsData::InitExpr();
  test_fbs::DataT test;
  test.unit.reset(new test_fbs::SubDataT);
  test.name = "test_fbs_name";
  test.score = 1.23;
  test.unit->id = 100;
  test.unit->name = "sub_name";
  flatbuffers::FlatBufferBuilder builder;
  auto offset = test_fbs::Data::Pack(builder, &test);
  builder.Finish(offset);
  const test_fbs::Data* fbs = test_fbs::GetData(builder.GetBufferPointer());

  test_proto::Data pb;
  pb.mutable_unit()->set_feedid("name123");
  pb.mutable_unit()->set_score(1.23);

  PbFbsData data;
  data.fbs = fbs;
  data.pb = &pb;

  std::vector<std::string> names = {"fbs", "unit", "id"};
  std::vector<FieldAccessor> accessors;
  PbFbsData::GetFieldAccessors(names, accessors);
  auto val = GetFieldValue(&data, accessors);
  EXPECT_EQ(100, std::get<uint32_t>(val));

  names = {"pb", "unit", "feedid"};
  PbFbsData::GetFieldAccessors(names, accessors);
  val = GetFieldValue(&data, accessors);
  EXPECT_EQ("name123", std::get<std::string_view>(val));
}

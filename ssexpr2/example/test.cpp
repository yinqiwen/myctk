#include <stdint.h>
#include <string>
#include "jit_struct.h"
#include "jit_struct_helper.h"
#include "ssexpr2/example/test_data.pb.h"
#include "ssexpr2/example/test_data_generated.h"

using namespace ssexpr2;
DEFINE_JIT_STRUCT(SubItem1, (double)score, (std::string)id, (int32_t)vv)
DEFINE_JIT_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv, (const SubItem1*)sub)

void test1() {
  Item1 item;
  item.score = 1.234;
  item.id = "234";
  auto sub = new SubItem1;
  sub->id = "abcs";
  sub->vv = 101;
  item.sub = sub;
  Item1::InitJitBuilder();
  std::vector<std::string> n = {"sub", "id"};
  ssexpr2::ValueAccessor access = ssexpr2::GetFieldAccessors<Item1>(n);
  ssexpr2::Value v = access(&item);
  printf("###%d, %s\n", v.type, v.Get<std::string_view>().data());
  n = {"sub", "vv"};
  access = ssexpr2::GetFieldAccessors<Item1>(n);
  v = access(&item);
  printf("###%d, %d\n", v.type, v.Get<int32_t>());
}
struct SubMessage {
  int _a = 156;
  std::string _b = "hello,world_sub";
  double _c = 3.1415926;
  float _d = 1.23345;
  int a() const { return _a; }
  const std::string& b() const { return _b; }
  double c() const { return _c; }
  float d() const { return _d; }
};
struct Message {
  int _a = 156;
  std::string _b = "hello,world";
  SubMessage _sub;
  int a() const { return _a; }
  const std::string& b() const { return _b; }
  const SubMessage& sub() const { return _sub; }
};
DEFINE_JIT_STRUCT_HELPER(SubMessage, a, b, c, d)
DEFINE_JIT_STRUCT_HELPER(Message, a, b, sub)
void test2() {
  Message msg;
  ssexpr2::JitStructHelper<Message>::InitJitBuilder();
  std::vector<std::string> n = {"sub", "b"};
  ssexpr2::ValueAccessor access = ssexpr2::GetFieldAccessors<ssexpr2::JitStructHelper<Message>>(n);
  ssexpr2::Value v = access(&msg);
  printf("###%d,  %s\n", v.type, v.Get<std::string_view>().data());
  n = {"sub", "a"};
  access = ssexpr2::GetFieldAccessors<ssexpr2::JitStructHelper<Message>>(n);
  v = access(&msg);
  printf("###%d, %d\n", v.type, v.Get<int32_t>());
}

DEFINE_JIT_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv, (const Message*)msg)

void test3() {
  Item2::InitJitBuilder();
  Message msg;
  Item2 item2;
  item2.msg = &msg;
  msg._sub._b = "test3:msg";
  msg._sub._a = 98765;
  std::vector<std::string> n = {"msg", "sub", "b"};
  ssexpr2::ValueAccessor access = ssexpr2::GetFieldAccessors<Item2>(n);
  ssexpr2::Value v = access(&item2);
  printf("###%d, %s\n", v.type, v.Get<std::string_view>().data());
  n = {"msg", "sub", "a"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d, %d\n", v.type, v.Get<int32_t>());
  n = {"msg", "sub", "c"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d %d,  %.10f\n", access.Valid(), v.type, v.Get<double>());
  n = {"msg", "sub", "d"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d %d, %.5f\n", access.Valid(), v.type, v.Get<float>());
}

DEFINE_JIT_STRUCT_HELPER(test_fbs::SubData, id, iid, name, score)
DEFINE_JIT_STRUCT_HELPER(test_fbs::Data, name, score, unit)
static void test4() {
  JitStructHelper<test_fbs::Data>::InitJitBuilder();
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

  std::vector<std::string> names = {"unit", "id"};
  ssexpr2::ValueAccessor access =
      ssexpr2::GetFieldAccessors<JitStructHelper<test_fbs::Data>>(names);
  auto v = access(t);
  printf("###%d %u\n", v.type, v.Get<uint32_t>());

  names = {"unit", "name"};
  access = GetFieldAccessors<JitStructHelper<test_fbs::Data>>(names);
  v = access(t);
  printf("###%d %s\n", v.type, v.Get<std::string_view>().data());
}

DEFINE_JIT_STRUCT_HELPER(test_proto::SubData, feedid, rank, score)
DEFINE_JIT_STRUCT_HELPER(test_proto::Data, id, model_id, unit)
static void test5() {
  JitStructHelper<test_proto::Data>::InitJitBuilder();
  test_proto::Data test;
  test.mutable_unit()->set_feedid("name123");
  test.mutable_unit()->set_score(1.23);

  std::vector<std::string> names = {"unit", "feedid"};
  ssexpr2::ValueAccessor access =
      ssexpr2::GetFieldAccessors<JitStructHelper<test_proto::Data>>(names);
  auto v = access(&test);
  printf("###%d %s\n", v.type, v.Get<std::string_view>().data());

  names = {"unit", "score"};
  access = GetFieldAccessors<JitStructHelper<test_proto::Data>>(names);
  v = access(&test);
  printf("###%d %f\n", v.type, v.Get<double>());
}

DEFINE_JIT_STRUCT(Combine, (const test_proto::Data*)pb, (const test_fbs::Data*)fbs)
static void test6() {
  Combine::InitJitBuilder();
  Combine c;
  test_proto::Data test;
  {
    test.mutable_unit()->set_feedid("name123");
    test.mutable_unit()->set_score(1.23);
    c.pb = &test;
  }
  flatbuffers::FlatBufferBuilder builder;
  {
    test_fbs::DataT test;
    test.unit.reset(new test_fbs::SubDataT);
    test.name = "test_name";
    test.score = 1.23;
    test.unit->id = 100;
    test.unit->name = "sub_name";

    auto offset = test_fbs::Data::Pack(builder, &test);
    builder.Finish(offset);
    const test_fbs::Data* t = test_fbs::GetData(builder.GetBufferPointer());
    c.fbs = t;
  }

  std::vector<std::string> names = {"pb", "unit", "feedid"};
  ssexpr2::ValueAccessor access = ssexpr2::GetFieldAccessors<Combine>(names);
  auto v = access(&c);
  printf("###%d %s\n", v.type, v.Get<std::string_view>().data());

  names = {"fbs", "unit", "name"};
  access = GetFieldAccessors<Combine>(names);
  v = access(&c);
  printf("###%d %s\n", v.type, v.Get<std::string_view>().data());
}

int main() {
  /*
   ** nested POD pointer
   */
  test1();
  /*
  ** nested POD object
  */
  test2();
  /**
   * struct with access methods
   *
   */
  test3();
  /*
   ** flatbuffers test
   */
  test4();
  /*
  ** protobuffers test
  */
  test5();

  /**
   * combine pb & fbs in a POD
   *
   */
  test6();
  return 0;
}

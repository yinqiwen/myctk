# expr_struct
用于字符串expr表达形式快速访问c++ struct成员变量
- 支持POD结构多层结构嵌套（指针和非指针）
- 支持ProtoBuffers
- 支持Flatbuffers（修改flatc）

## Example

### Single Struct
```cpp
#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv)
void test1() {
  Item1::InitExpr();  //全局可只用调一次
  std::vector<std::string> names = {"id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item1::GetFieldAccessors(names, accessors);  // get access functions

  for (size_t i = 0; i < 10; i++) {
    Item1 item = {1.0, "item" + std::to_string(i), i};
    auto val = item.GetFieldValue(accessors);
    std::string v = GetValue<std::string, FieldValue>(val);
    printf("id=%s\n", v.c_str());
  }
}
```

### Nested Struct Pointer 
```cpp
#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(SubItem2, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv, (SubItem2*)sub)
void test2() {
  Item2::InitExpr();  //全局可只用调一次
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
```

### Nested Struct Object 
```cpp
#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(SubItem3, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item3, (double)score, (std::string)id, (int32_t)vv, (SubItem3)sub)
void test3() {
  Item3::InitExpr();  //全局可只用调一次
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
```

### ProtoBuffers 
```cpp
#include "data.pb.h"
#include "expr_protobuf.h"
using namespace expr_struct;
using namespace test_proto;
DEFINE_PB_EXPR_HELPER(SubData, feedid, rank, score)
DEFINE_PB_EXPR_HELPER(Data, id, model_id, unit)
void test_pb() {
  ExprProtoHelper<Data>::InitExpr();
  std::vector<std::string> names = {"unit", "feedid"};
  std::vector<expr_struct::FieldAccessor> accessors;
  ExprProtoHelper<Data>::GetFieldAccessors(names, accessors);

  Data test;
  test.mutable_unit()->set_feedid("name123");
  test.mutable_unit()->set_score(1.23);

  auto val = expr_struct::GetFieldValue(&test, accessors);
  std::string_view sv = expr_struct::GetValue<std::string_view, FieldValue>(val);
  printf("%s\n", sv.data());
}
```

### FlatBuffers 
```cpp
#include "data_generated.h"
using namespace expr_struct;
using namespace test;
void test_fbs() {
  Data::InitExpr();
  std::vector<std::string> names = {"unit", "id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Data::GetFieldAccessors(names, accessors);

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

  auto val = t->GetFieldValue(accessors);
  uint32_t v = GetValue<uint32_t, FieldValue>(val);
  printf("%d\n", v);
}
```


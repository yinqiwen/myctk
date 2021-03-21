# expr_struct
用于字符串expr表达形式快速访问c++ struct成员变量，支持多层结构嵌套（指针和非指针）

## Example

### Single Struct
```cpp
#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv)
void test1(){
  Item1::Init();  //全局可只用调一次
  std::vector<std::string> names = {"id"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item1::GetFieldAccessors(names, accessors);  // get access functions

  for(size_t i =0; i< 100; i++){
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
DEFINE_EXPR_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv,（SubItem2*)sub)
void test2(){
  Item2::Init();  //全局可只用调一次
  std::vector<std::string> names = {"sub", "score"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item2::GetFieldAccessors(names, accessors);  // get access functions

  for(size_t i =0; i< 100; i++){
      SubItem2 sub = {3.14 + i, "subitem"};
      Item2 item = {1.0, "item" + std::to_string(i), i, &sub};
      auto val = item.GetFieldValue(accessors);
      double v = GetValue<double, FieldValue>(val);
      printf("sub.score=%.2f\n", v);
  }
}
```

### Nested Struct Object 
```cpp
#include "expr_struct.h"
using namespace expr_struct;
DEFINE_EXPR_STRUCT(SubItem2, (double)score, (std::string)id)
DEFINE_EXPR_STRUCT(Item2, (double)score, (std::string)id, (int32_t)vv,（SubItem2)sub)
void test2(){
  Item2::Init();  //全局可只用调一次
  std::vector<std::string> names = {"sub", "score"};
  std::vector<expr_struct::FieldAccessor> accessors;
  Item2::GetFieldAccessors(names, accessors);  // get access functions

  for(size_t i =0; i< 100; i++){
      Item2 item = {1.0, "item" + std::to_string(i), i, {3.14 + i, "subitem"}};
      auto val = item.GetFieldValue(accessors);
      double v = GetValue<double, FieldValue>(val);
      printf("sub.score=%.2f\n", v);
  }
}
```


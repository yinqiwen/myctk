#include <stdint.h>
#include <string>
#include "jit_struct.h"
#include "jit_struct_helper.h"

DEFINE_JIT_STRUCT(SubItem1, (double)score, (std::string)id, (int32_t)vv)
DEFINE_JIT_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv, (SubItem1*)sub)

void test1() {
  Item1 item;
  item.score = 1.234;
  item.id = "234";
  item.sub = new SubItem1;
  item.sub->id = "abcs";
  item.sub->vv = 101;
  Item1::InitJitBuilder();
  std::vector<std::string> n = {"sub", "id"};
  ssexpr2::ValueAccessor access = ssexpr2::GetFieldAccessors<Item1>(n);
  ssexpr2::Value v = access(&item);
  printf("###%d, %p, %s\n", v.type, v.val, v.Get<std::string_view>().data());
  n = {"sub", "vv"};
  access = ssexpr2::GetFieldAccessors<Item1>(n);
  v = access(&item);
  printf("###%d, %p, %d\n", v.type, v.val, v.Get<int32_t>());
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
  printf("###%d, %p, %s\n", v.type, v.val, v.Get<std::string_view>().data());
  n = {"sub", "a"};
  access = ssexpr2::GetFieldAccessors<ssexpr2::JitStructHelper<Message>>(n);
  v = access(&msg);
  printf("###%d, %p, %d\n", v.type, v.val, v.Get<int32_t>());
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
  printf("###%d, %p, %s\n", v.type, v.val, v.Get<std::string_view>().data());
  n = {"msg", "sub", "a"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d, %p, %d\n", v.type, v.val, v.Get<int32_t>());
  n = {"msg", "sub", "c"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d %d, %p, %.10f\n", access.Valid(), v.type, v.val, v.Get<double>());
  n = {"msg", "sub", "d"};
  access = ssexpr2::GetFieldAccessors<Item2>(n);
  v = access(&item2);
  printf("###%d %d, %p, %.5\n", access.Valid(), v.type, v.val, v.Get<float>());
}

int main() {
  test1();
  test2();
  test3();

  return 0;
}

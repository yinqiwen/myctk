#include <sys/time.h>
#include "spirit_jit_expression.h"
using namespace ssexpr2;
static int64_t ustime(void) {
  struct timeval tv;
  int64_t ust;

  gettimeofday(&tv, NULL);
  ust = ((long)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}
static Value cfunc1() {
  Value v;
  v.Set<int64_t>(101);
  // printf("Invoke cfunc1\n");
  return v;
}
static Value cfunc2(Value arg0) {
  Value v;
  v.Set<double>(arg0.Get<int64_t>() + 2.0);
  // printf("Invoke cfunc2\n");
  return v;
}

DEFINE_JIT_STRUCT(SubItem1, (double)score, (std::string)id, (int32_t)vv)
DEFINE_JIT_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv, (SubItem1)sub)

int main() {
  // 5.2 + 101 - 5.0
  std::string str = "1 + 2*3.1 - 6/3 + cfunc1() - cfunc2(3) + sub.score";
  ssexpr2::SpiritExpression expr;
  ssexpr2::ExprOptions options;
  options.Init<Item1>();
  options.functions["cfunc1"] = (ExprFunction)cfunc1;
  options.functions["cfunc2"] = (ExprFunction)cfunc2;
  int rc = expr.Init(str, options);
  if (0 != rc) {
    printf("Init %s err:%d\n", str.c_str(), rc);
    return rc;
  }
  expr.DumpAsmCode("./asm.bin");
  Item1 item;
  item.sub.score = 99.2;
  ssexpr2::Value rv;
  int64_t start_us = ustime();
  int loop_count = 100000;
  for (int i = 0; i < loop_count; i++) {
    rv = expr.Eval(item);
  }
  int64_t end_us = ustime();
  printf("##benchReflect cost %lldus to loop count %lld\n", end_us - start_us, loop_count);
  printf("Eval result:%.2f\n", rv.Get<double>());
  return 0;
}
#include <benchmark/benchmark.h>
#include <sys/time.h>
#include "spirit_jit_expression.h"
using namespace ssexpr2;

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

static void BM_ssexpr_eval(benchmark::State& state) {
  std::string str = "1 + 2*3.1 - 6/3 + cfunc1() - cfunc2(3) + sub.score";
  ssexpr2::SpiritExpression expr;
  ssexpr2::ExprOptions options;
  options.Init<Item1>();
  options.functions["cfunc1"] = (ExprFunction)cfunc1;
  options.functions["cfunc2"] = (ExprFunction)cfunc2;
  int rc = expr.Init(str, options);
  if (0 != rc) {
    printf("Init %s err:%d\n", str.c_str(), rc);
    return;
  }
  ssexpr2::Value rv;
  for (auto _ : state) {
    Item1 item;
    item.sub.score = 99.2;
    rv = expr.Eval(item);
  }
  printf("Eval result:%.2f\n", rv.Get<double>());
}
// Register the function as a benchmark
BENCHMARK(BM_ssexpr_eval);
// Run the benchmark
BENCHMARK_MAIN();

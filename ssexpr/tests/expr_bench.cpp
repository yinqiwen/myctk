#include <benchmark/benchmark.h>
#include <sys/time.h>
#include "spirit_expression.h"
using namespace ssexpr;

DEFINE_EXPR_STRUCT(SubItem1, (double)score, (std::string)id, (int32_t)vv)
DEFINE_EXPR_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv, (SubItem1)sub)

static void BM_ssexpr_eval(benchmark::State& state) {
  std::string str = "1 + 2*3.1 - 6/3 + cfunc1() - cfunc2(3) + sub.score";
  ssexpr::SpiritExpression expr;
  ssexpr::ExprOptions opt;
  opt.Init<Item1>();
  opt.functions["cfunc1"] = [](const std::vector<ssexpr::Value>& args) {
    int64_t v1 = 101;
    ssexpr::Value v = v1;
    return v;
  };
  opt.functions["cfunc2"] = [](const std::vector<ssexpr::Value>& args) {
    double v1 = 2.0 + std::get<int64_t>(args[0]);
    ssexpr::Value v = v1;
    return v;
  };
  int rc = expr.Init(str, opt);
  if (0 != rc) {
    printf("Init %s err:%d\n", str.c_str(), rc);
    return;
  }
  ssexpr::Value rv;
  for (auto _ : state) {
    Item1 item;
    item.sub.score = 99.2;
    rv = expr.Eval(item);
  }
  printf("Eval result:%.2f\n", std::get<double>(rv));
}
// Register the function as a benchmark
BENCHMARK(BM_ssexpr_eval);
// Run the benchmark
BENCHMARK_MAIN();

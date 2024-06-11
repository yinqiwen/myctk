// Copyright (c) 2021, Tencent Inc.
// All rights reserved.

#include <benchmark/benchmark.h>
#include <sys/time.h>
#include "didagle/graph_processor.h"
#include "didagle/graph_processor_api.h"
using namespace didagle;

GRAPH_OP_BEGIN(test_phase)
GRAPH_OP_EXTERN_INPUT(int, v0)
GRAPH_OP_OUTPUT(std::string, v1)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v2)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  // if (nullptr != v0) {
  //   DIDAGLE_DEBUG("phase0 input v0 = {}", *v0);
  // } else {
  //   DIDAGLE_DEBUG("phase0 input v0 empty");
  // }
  v1 = "val" + std::to_string(*v0);
  v2["key1"] = "val1";
  v2["key2"] = "val2";
  return 0;
}
GRAPH_OP_END

static void BM_test_proc_run(benchmark::State& state) {
  auto ctx = GraphDataContext::New();
  int tmp = 101;
  ctx->Set("v0", &tmp);
  for (auto _ : state) {
    run_processor(*ctx, "test_phase");
  }
}
// Register the function as a benchmark
BENCHMARK(BM_test_proc_run);
// Run the benchmark
BENCHMARK_MAIN();
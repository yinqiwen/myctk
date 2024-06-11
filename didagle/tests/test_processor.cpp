// Copyright (c) 2021, Tencent Inc.
// All rights reserved.

#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include "didagle/graph_processor.h"
#include "didagle/graph_processor_api.h"
using namespace didagle;

GRAPH_OP_BEGIN(test_phase)
GRAPH_OP_INPUT(int, v0)
GRAPH_OP_MAP_INPUT(std::string, v100)
GRAPH_OP_OUTPUT(std::string, v1)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v2)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr != v0) {
    DIDAGLE_DEBUG("phase0 input v0 = {}", *v0);
  } else {
    DIDAGLE_DEBUG("phase0 input v0 empty");
  }
  v1 = "val" + std::to_string(*v0);
  v2["key1"] = "val1";
  v2["key2"] = "val2";
  for (auto& pair : v100) {
    DIDAGLE_DEBUG("v100 {}:{}", pair.first, *pair.second);
  }
  DIDAGLE_DEBUG("v100 size:{}", v100.size());
  return 0;
}
GRAPH_OP_END

TEST(ProcessorUT, Porcessor) {
  auto ctx = GraphDataContext::New();
  int tmp = 101;
  ctx->Set("v0", &tmp);

  ProcessorRunOptions opts;
  opts.map_aggregate_ids["v100"] = {"a", "b", "c"};
  std::string v1000 = "a";
  ctx->Set("a", &v1000);
  std::string v1001 = "b";
  ctx->Set("b", &v1001);
  std::string v1002 = "c";
  ctx->Set("c", &v1002);
  ProcessorRunResult result = run_processor(*ctx, "test_phase", opts);
  EXPECT_EQ(result.rc, 0);

  EXPECT_EQ("val101", *(ctx->Get<std::string>("v1")));
  const std::map<std::string, std::string>* map = ctx->Get<std::map<std::string, std::string>>("v2");
  EXPECT_EQ("val1", map->at("key1"));
  EXPECT_EQ("val2", map->at("key2"));
}

#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include "graph_processor.h"
using namespace didagle;

GRAPH_PROC_BEGIN(test_phase)
DEF_IN_FIELD(int, v0)
DEF_OUT_FIELD(std::string, v1)
DEF_OUT_FIELD((std::map<std::string, std::string>), v2)
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
  return 0;
}
GRAPH_PROC_END

TEST(ProcessorUT, Porcessor) {
  GraphDataContext ctx;
  int tmp = 101;
  ctx.Set<int>("v0", &tmp);
  ProcessorRunResult result = run_processor(ctx, "test_phase");
  EXPECT_EQ(result.rc, 0);
  EXPECT_EQ("val101", *(ctx.Get<std::string>("v1")));
  const std::map<std::string, std::string>* map = ctx.Get<std::map<std::string, std::string>>("v2");
  EXPECT_EQ("val1", map->at("key1"));
  EXPECT_EQ("val2", map->at("key2"));
}

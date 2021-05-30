// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "di_container.h"
#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_PROC_BEGIN(phase0)
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
  v1 = "from phase 0, hello";
  v2["key1"] = "val1";
  v2["key2"] = "val2";
  DIDAGLE_DEBUG("Run {} with abc={}", Name(), args["abc"].String());
  DIDAGLE_DEBUG("Type id1={}", DIContainer::GetTypeId<std::map<std::string, std::string>>());
  return 0;
}
GRAPH_PROC_END

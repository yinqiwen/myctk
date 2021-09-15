// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "di_container.h"
#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(phase0)
GRAPH_OP_INPUT(int, v0)
GRAPH_OP_EXTERN_IN_OUT(std::string, vx)
GRAPH_OP_OUTPUT(std::string, v1)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v2)
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
  if (nullptr != vx) {
    DIDAGLE_ERROR("###vx={}", *vx);
  } else {
    DIDAGLE_ERROR("###empty vx");
  }
  return 0;
}
GRAPH_OP_END

// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "log.h"
#include "processor_api.h"

GRAPH_PROC_BEGIN(phase0)
DEF_IN_FIELD(int, v0)
DEF_OUT_FIELD(std::string, v1)
DEF_OUT_FIELD((std::map<std::string, std::string>), v2)
int OnSetup(const Params& args) { return 0; }
int OnExecute(const Params& args) {
  if (nullptr != v0) {
    WRDK_GRAPH_DEBUG("phase0 input v0 = {}", *v0);
  } else {
    WRDK_GRAPH_DEBUG("phase0 input v0 empty");
  }
  v1 = "from phase 0, hello";
  v2["key1"] = "val1";
  v2["key2"] = "val2";
  WRDK_GRAPH_DEBUG("Run {} with abc={}", Name(), args["abc"].String());
  return 0;
}
GRAPH_PROC_END

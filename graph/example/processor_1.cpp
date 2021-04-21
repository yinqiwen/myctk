// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "log.h"
#include "processor_api.h"

GRAPH_PROC_BEGIN(phase1)
DEF_IN_FIELD((std::map<std::string, std::string>), v2)
DEF_OUT_FIELD(std::string, v3)
DEF_OUT_FIELD((std::map<std::string, std::string>), v4)
int OnSetup(const Params& args) { return 0; }
int OnExecute(const Params& args) {
  WRDK_GRAPH_DEBUG("Run {}", Name());
  if (nullptr != v2) {
    WRDK_GRAPH_DEBUG("phase1 input v2 size = {}", v2->size());
  }
  v3 = "from phase 1, hello";
  v4["key001"] = "001";
  v4["key002"] = "003";
  v4["key003"] = "004";
  return 0;
}
GRAPH_PROC_END

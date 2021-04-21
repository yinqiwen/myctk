// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "log.h"
#include "processor_api.h"

GRAPH_PROC_BEGIN(phase2)
DEF_IN_FIELD(std::string, v1)
DEF_OUT_FIELD(std::string, v5)
DEF_OUT_FIELD((std::map<std::string, std::string>), v6)
int OnSetup(const Params& args) { return 0; }
int OnExecute(const Params& args) {
  WRDK_GRAPH_DEBUG("Run {}", Name());
  if (nullptr != v1) {
    WRDK_GRAPH_DEBUG("phase2 input v1 = {}", *v1);
  }
  v5 = "from phase 2, hello";
  v6["key001"] = "001";
  v6["key002"] = "003";
  v6["key003"] = "004";
  v6["key004"] = "004";
  return 0;
}
GRAPH_PROC_END

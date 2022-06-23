// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_OP_BEGIN(phase2)
GRAPH_OP_INPUT(std::string, v1)
GRAPH_OP_OUTPUT(std::string, v5)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v6)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {}", Name());
  if (nullptr != v1) {
    DIDAGLE_DEBUG("phase2 input v1 = {}", *v1);
  }
  v5 = "from phase 2, hello";
  v6["key001"] = "001";
  v6["key002"] = "003";
  v6["key003"] = "004";
  v6["key004"] = "004";
  return 0;
}
GRAPH_OP_END

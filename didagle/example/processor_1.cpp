// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_OP_BEGIN(phase1)
GRAPH_OP_INPUT((std::map<std::string, std::string>), v2)
GRAPH_OP_OUTPUT(std::string, v3)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v4)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {}", Name());
  if (nullptr != v2) {
    DIDAGLE_DEBUG("phase1 input v2 size = {}", v2->size());
  }
  v3 = "from phase 1, hello";
  v4["key001"] = "001";
  v4["key002"] = "003";
  v4["key003"] = "004";
  return 0;
}
GRAPH_OP_END

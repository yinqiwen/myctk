// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_OP_BEGIN(phase3)
GRAPH_OP_INPUT(std::string, v1)
GRAPH_OP_INPUT((std::map<std::string, std::string>), v2)
GRAPH_OP_INPUT(std::string, v3)
GRAPH_OP_INPUT((std::map<std::string, std::string>), v4)
GRAPH_OP_INPUT(std::string, v5)
GRAPH_OP_INPUT((std::map<std::string, std::string>), v6)
GRAPH_OP_OUTPUT(std::string, v100)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {}", Name());
  if (nullptr != v1) {
    DIDAGLE_DEBUG("phase3 input v1 = {}", *v1);
  }
  if (nullptr != v2) {
    DIDAGLE_DEBUG("phase3 input v2 size = {}", v2->size());
  }
  if (nullptr != v3) {
    DIDAGLE_DEBUG("phase3 input v3 = {}", *v3);
  }
  if (nullptr != v4) {
    DIDAGLE_DEBUG("phase3 input v4 size = {}", v4->size());
  }
  if (nullptr != v5) {
    DIDAGLE_DEBUG("phase3 input v5  = {}", *v5);
  }
  if (nullptr != v6) {
    DIDAGLE_DEBUG("phase3 input v6 size = {}", v6->size());
  }
  v100 = "from phase 4, hello";
  return 0;
}
GRAPH_OP_END

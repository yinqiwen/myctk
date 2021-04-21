// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "log.h"
#include "processor_api.h"

GRAPH_PROC_BEGIN(phase3)
DEF_IN_FIELD(std::string, v1)
DEF_IN_FIELD((std::map<std::string, std::string>), v2)
DEF_IN_FIELD(std::string, v3)
DEF_IN_FIELD((std::map<std::string, std::string>), v4)
DEF_IN_FIELD(std::string, v5)
DEF_IN_FIELD((std::map<std::string, std::string>), v6)
DEF_OUT_FIELD(std::string, v100)
int OnSetup(const Params& args) { return 0; }
int OnExecute(const Params& args) {
  WRDK_GRAPH_DEBUG("Run {}", Name());
  if (nullptr != v1) {
    WRDK_GRAPH_DEBUG("phase3 input v1 = {}", *v1);
  }
  if (nullptr != v2) {
    WRDK_GRAPH_DEBUG("phase3 input v2 size = {}", v2->size());
  }
  if (nullptr != v3) {
    WRDK_GRAPH_DEBUG("phase3 input v3 = {}", *v3);
  }
  if (nullptr != v4) {
    WRDK_GRAPH_DEBUG("phase3 input v4 size = {}", v4->size());
  }
  if (nullptr != v5) {
    WRDK_GRAPH_DEBUG("phase3 input v5  = {}", *v5);
  }
  if (nullptr != v6) {
    WRDK_GRAPH_DEBUG("phase3 input v6 size = {}", v6->size());
  }
  v100 = "from phase 4, hello";
  return 0;
}
GRAPH_PROC_END

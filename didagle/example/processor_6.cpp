// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "log.h"
#include "processor_api.h"

GRAPH_PROC_BEGIN(phase6)
DEF_IN_OUT_FIELD((std::map<std::string, const std::string*>), v100)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  DIDAGLE_DEBUG("Run {} v100 size = {}", Name(), v100->size());
  for (auto& pair : *v100) {
    DIDAGLE_DEBUG("{}->{}", pair.first, *(pair.second));
  }
  return 0;
}
GRAPH_PROC_END
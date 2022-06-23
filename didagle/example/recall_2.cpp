// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_OP_BEGIN(recall_2)
GRAPH_OP_OUTPUT((std::string), r2)
GRAPH_OP_IN_OUT((std::string), abc)
int OnSetup(const didagle::Params& args) override { return 0; }
int OnExecute(const didagle::Params& args) override {
  r2 = "recall2";
  if (nullptr != abc) {
    DIDAGLE_DEBUG("abc = {}", *abc);
    *abc = "new val + recall2";
  }
  return 0;
}
GRAPH_OP_END
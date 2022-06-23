// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/di_container.h"
#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_OP_BEGIN(phase0, "this is test phase")
GRAPH_OP_INPUT(int, v0)
GRAPH_OP_EXTERN_IN_OUT(std::string, vx)
GRAPH_OP_OUTPUT(std::string, v1)
GRAPH_OP_OUTPUT((std::map<std::string, std::string>), v2)
GRAPH_PARAMS_bool(barg, false, "e");

int OnExecute(const didagle::Params& args) override { return 0; }
GRAPH_OP_END

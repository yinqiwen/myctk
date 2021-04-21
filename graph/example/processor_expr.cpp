// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "processor_api.h"

GRAPH_PROC_BEGIN(phase0)
DEF_IN_FIELD(int, a)
DEF_OUT_FIELD(std::string, b)
int OnSetup(const Params& args) { return 0; }
GRAPH_PROC_END

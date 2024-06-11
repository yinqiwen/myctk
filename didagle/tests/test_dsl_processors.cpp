// Copyright (c) 2024, Tencent Inc.
// All rights reserved.

#include <stdint.h>
#include <string>
#include "didagle/graph_processor.h"
#include "didagle/graph_processor_api.h"
using namespace didagle;

GRAPH_OP_BEGIN(test0)
// GRAPH_OP_EXTERN_INPUT(int, i0)
GRAPH_OP_OUTPUT(std::string, test0)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override { return 0; }
GRAPH_OP_END

GRAPH_OP_BEGIN(test1)
GRAPH_OP_INPUT(std::string, test0)
GRAPH_OP_OUTPUT(std::string, test1)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override { return 0; }
GRAPH_OP_END

GRAPH_OP_BEGIN(test2)
GRAPH_OP_INPUT(std::string, test1)
GRAPH_OP_OUTPUT(std::string, test2)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override { return 0; }
GRAPH_OP_END

GRAPH_OP_BEGIN(test3)
GRAPH_OP_INPUT(std::string, test2)
GRAPH_OP_OUTPUT(std::string, test3)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override { return 0; }
GRAPH_OP_END

GRAPH_OP_BEGIN(test4)
GRAPH_OP_INPUT(std::string, test3)
GRAPH_OP_OUTPUT(std::string, test4)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override { return 0; }
GRAPH_OP_END
// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include <thread>
#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_CORO_OP_BEGIN(coro_phase2, "this is test coro phase")
GRAPH_OP_INPUT(std::string, v0)
GRAPH_OP_INPUT(std::string, v1)

int OnSetup(const didagle::Params& args) override { return 0; }
#if ISPINE_HAS_COROUTINES
ispine::Awaitable<int> OnCoroExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {} at thread:{}", Name(), std::this_thread::get_id());
  DIDAGLE_DEBUG("Recv v0:{}", nullptr != v0 ? *v0 : "");
  DIDAGLE_DEBUG("Recv v1:{}", nullptr != v1 ? *v1 : "");
  co_return 0;
}
#endif
GRAPH_OP_END

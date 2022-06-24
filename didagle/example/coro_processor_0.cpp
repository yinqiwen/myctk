// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include <thread>
#include "didagle/di_container.h"
#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"

GRAPH_CORO_OP_BEGIN(coro_phase0, "this is test coro phase")
GRAPH_OP_OUTPUT(std::string, v0)

#if ISPINE_HAS_COROUTINES
ispine::Awaitable<int> OnCoroExecute(const didagle::Params& args) override {
  DIDAGLE_DEBUG("Run {} at thread:{}", Name(), std::this_thread::get_id());
  v0 = "coro_phase0";
  co_return 0;
}
#endif
GRAPH_OP_END

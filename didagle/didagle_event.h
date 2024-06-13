/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "folly/AtomicIntrusiveLinkedList.h"

namespace didagle {
enum class PhaseType {
  DAG_PHASE_UNKNOWN = 0,
  DAG_PHASE_CONCURRENT_SCHED,
  DAG_PHASE_OP_PREPARE_EXECUTE,
  DAG_PHASE_OP_POST_EXECUTE,
  DAG_PHASE_GRAPH_ASYNC_RESET,
  DAG_GRAPH_GRAPH_PREPARE_EXECUTE,
};

struct DAGEvent {
  std::string_view processor;
  std::string_view cluster;
  std::string_view graph;
  std::string_view full_graph_name;
  std::string_view matched_cond;

  PhaseType phase{PhaseType::DAG_PHASE_UNKNOWN};
  uint64_t start_ustime = 0;
  uint64_t end_ustime = 0;
  int rc = -1;

  folly::AtomicIntrusiveLinkedListHook<DAGEvent> _hook;
  using List = folly::AtomicIntrusiveLinkedList<DAGEvent, &DAGEvent::_hook>;
};

struct DAGEventTracker {
  using SweepFunc = std::function<void(const DAGEvent*)>;
  DAGEvent::List events;
  DAGEventTracker() {}
  void Add(std::unique_ptr<DAGEvent>&& event) { events.insertHead(event.release()); }
  void Sweep(SweepFunc&& f) {
    events.sweep([f = std::move(f)](DAGEvent* event) {
      f(event);
      delete event;
    });
  }
  ~DAGEventTracker() {
    events.sweep([](DAGEvent* event) { delete event; });
  }
};

std::string_view get_dag_phase_name(PhaseType phase);
}  // namespace didagle
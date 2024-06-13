// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include <fmt/core.h>
#include <gflags/gflags.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>

#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"
#include "didagle/didagle_event.h"
#include "folly/Singleton.h"
#include "folly/synchronization/HazptrThreadPoolExecutor.h"
#include "folly/synchronization/Latch.h"
#include "spdlog/spdlog.h"

#include "didagle/didagle_background.h"
#include "didagle/didagle_log.h"
#include "didagle/graph.h"
#include "didagle/graph_processor.h"

DEFINE_string(script, "", "script name");
DEFINE_string(graph, "", "graph name");
DEFINE_int32(test_count, 100, "test count");

static int64_t gettimeofday_us() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((int64_t)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

using namespace didagle;
static std::string get_basename(const std::string& filename) {
#if defined(_WIN32)
  char dir_sep('\\');
#else
  char dir_sep('/');
#endif

  std::string::size_type pos = filename.rfind(dir_sep);
  if (pos != std::string::npos)
    return filename.substr(pos + 1);
  else
    return filename;
}
int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, false);
  folly::SingletonVault::singleton()->registrationComplete();
  // Set the default hazard pointer domain to use a thread pool executor
  // for asynchronous reclamation
  folly::enable_hazptr_thread_pool_executor();

  spdlog::set_level(spdlog::level::err);

  boost::asio::thread_pool pool(8);
  GraphExecuteOptions exec_opt;
  exec_opt.concurrent_executor = [&pool](AnyClosure&& r) {
    boost::asio::post(pool, r);
    // r();
  };
  int64_t proc_run_total_us = 0;
  exec_opt.event_reporter = [&](DAGEvent event) {
    // fmt::print("##phase:{}\n", static_cast<int>(event.phase));
    // if (event.phase == PhaseType::DAG_PHASE_UNKNOWN) {
    //   proc_run_total_us += (event.end_ustime - event.start_ustime);
    // }
  };
  {
    GraphManager graphs(exec_opt);
    std::string config = FLAGS_script;
    std::string graph = FLAGS_graph;

    // std::string png_file = config + ".png";
    auto cluster = graphs.Load(config);
    if (!cluster) {
      printf("Failed to parse toml\n");
      return -1;
    }
    std::string cluster_name = get_basename(config);

    exec_opt.params.reset(new Params);

    Params paras;
    paras["x"].SetInt(1);
    paras["y"].SetInt(1);
    paras["A"].SetBool(true);
    paras["myid"].SetString("xyz");
    paras["myid0"].SetString("output");
    paras["expid"].SetInt(1000);
    paras["EXP"]["field1"].SetInt(1221);

    auto root = GraphDataContext::New();

    // set extern data value for dsl
    int v = 101;
    root->Set("v0", &v);

    std::string vx = "hello";
    root->Set("vx", &vx);

    std::string r99 = "hello";
    std::string r100 = "world";
    const std::string* r99_ptr = &r99;
    std::string* r100_ptr = &r100;
    root->Set("r99", r99_ptr);
    root->Set("r100", r100_ptr);
    std::shared_ptr<std::string> sss(new std::string("hello, shread!"));
    root->Set("tstr", &sss);
    {
      std::unique_ptr<std::string> uuu(new std::string("hello, unique!"));
      root->Set("ustr", &uuu);
    }

    int64_t graph_run_us = 0;
    int64_t framework_run_us = 0;
    int64_t prpare_run_us = 0;
    int64_t post_run_us = 0;
    int64_t graph_prepare_run_us = 0;
    fmt::print("##Start run graph!\n");
    for (int i = 0; i < FLAGS_test_count; i++) {
      root->EnableEventTracker();
      folly::Latch latch(1);
      int64_t exec_start_us = gettimeofday_us();
      graphs.Execute(root, cluster_name, graph, &paras, [&, root, exec_start_us](int c) {
        // DIDAGLE_ERROR("Graph done with {}", c);
        graph_run_us += (gettimeofday_us() - exec_start_us);

        auto event_tracker = root->GetEventTracker();
        event_tracker->Sweep([&](const DAGEvent* event) {
          if (!event->processor.empty()) {
            proc_run_total_us += (event->end_ustime - event->start_ustime);
          } else {
            switch (event->phase) {
              case didagle::PhaseType::DAG_PHASE_OP_PREPARE_EXECUTE: {
                framework_run_us += (event->end_ustime - event->start_ustime);
                prpare_run_us += (event->end_ustime - event->start_ustime);
                break;
              }
              case didagle::PhaseType::DAG_PHASE_OP_POST_EXECUTE: {
                framework_run_us += (event->end_ustime - event->start_ustime);
                post_run_us += (event->end_ustime - event->start_ustime);
                break;
              }
              case didagle::PhaseType::DAG_GRAPH_GRAPH_PREPARE_EXECUTE: {
                framework_run_us += (event->end_ustime - event->start_ustime);
                graph_prepare_run_us += (event->end_ustime - event->start_ustime);
                break;
              }
              default: {
                fmt::print("#####{}\n", static_cast<int>(event->phase));
                break;
              }
            }
          }
        });

        root->Reset();
        latch.count_down();
      });
      latch.wait();
    }
    fmt::print("graph run cost {}us\n", graph_run_us);
    fmt::print("proc run cost {}us\n", proc_run_total_us);
    fmt::print("framework run cost {}us\n", framework_run_us);
    fmt::print("prepare run cost {}us\n", prpare_run_us);
    fmt::print("post run cost {}us\n", post_run_us);
    fmt::print("graph prepare run cost {}us\n", graph_prepare_run_us);

    fmt::print("gap {}us\n", graph_run_us - proc_run_total_us);
    // for (int i = 0; i < 1; i++) {
    //   graphs.Execute(root, cluster_name, graph, &paras,
    //                  [](int c) { DIDAGLE_ERROR("Graph done with {}", c); });
    //   sleep(1);
    // }
  }
  pool.join();

  return 0;
}

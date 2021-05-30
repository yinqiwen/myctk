// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include <stdlib.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "didagle_log.h"
#include "expr.h"
#include "graph.h"
#include "graph_processor.h"
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
  boost::asio::thread_pool pool(8);
  GraphExecuteOptions exec_opt;
  exec_opt.concurrent_executor = [&pool](AnyClosure&& r) {
    // boost::asio::post(pool, r);
    r();
  };
  GraphManager graphs(exec_opt);
  std::string config = "./graph.toml";
  std::string graph = "sub_graph0";
  if (argc > 1) {
    config = argv[1];
  }
  if (argc > 2) {
    graph = argv[2];
  }
  std::string png_file = config + ".png";
  auto cluster = graphs.Load(config);
  if (!cluster) {
    printf("Failed to parse toml\n");
    return -1;
  };
  if (0 != cluster->Build()) {
    printf("Failed to build graph cluster\n");
    return -1;
  }

  exec_opt.params.reset(new Params);
  std::shared_ptr<GraphDataContext> root(new GraphDataContext);
  std::string cluster_name = get_basename(config);
  // set extern data value for dsl
  int v = 101;
  root->Set<int>("v0", &v);
  RecmdEnv env;
  env.expid = 1001;
  root->Set<RecmdEnv>("env", &env);
  graphs.Execute(root, cluster_name, graph, nullptr,
                 [](int c) { DIDAGLE_ERROR("Graph done with {}", c); });

  pool.join();
  return 0;
}

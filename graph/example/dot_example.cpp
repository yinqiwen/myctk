// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include "graph.h"
using namespace wrdk::graph;
int main(int argc, char** argv) {
  GraphManager grpahs;
  std::string config = "./graph.toml";
  if (argc > 1) {
    config = argv[1];
  }
  std::string png_file = config + ".png";
  auto cluster = grpahs.Load(config);
  if (!cluster) {
    printf("Failed to parse toml\n");
    return -1;
  };
  if (0 != cluster->Build()) {
    printf("Failed to build graph cluster\n");
    return -1;
  }

  return 0;
}
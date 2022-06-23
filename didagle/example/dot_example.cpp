// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include "didagle/graph.h"
using namespace didagle;
int main(int argc, char** argv) {
  GraphExecuteOptions exec_opt;
  GraphManager grpahs(exec_opt);
  std::string config = "./graph.toml";
  if (argc > 1) {
    config = argv[1];
  }
  std::string png_file = config + ".png";
  auto cluster = grpahs.Load(config);
  if (!cluster) {
    printf("Failed to parse toml\n");
    return -1;
  }
  if (0 != cluster->Build()) {
    printf("Failed to build graph cluster\n");
    return -1;
  }
  std::string dot;
  cluster->DumpDot(dot);
  std::string dot_file = config + ".dot";
  std::ofstream out(dot_file);
  out << dot;
  out.close();
  std::string cmd = "dot -Tpng ";
  cmd.append(dot_file).append(" -o ").append(png_file);
  printf("Exec %s\n", cmd.c_str());
  if (0 == system(cmd.c_str())) {
    printf("Generate png:%s\n", png_file.c_str());
  }
  return 0;
}
// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/19
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#ifndef WRDK_GRAPH_LOG
#include <fmt/core.h>
#include <stdio.h>
#define WRDK_GRAPH_ERROR(...) \
  fmt::print(__VA_ARGS__);    \
  printf("\n")
#define WRDK_GRAPH_INFO(...) \
  fmt::print(__VA_ARGS__);   \
  printf("\n")
#define WRDK_GRAPH_DEBUG(...) \
  fmt::print(__VA_ARGS__);    \
  printf("\n")
#endif
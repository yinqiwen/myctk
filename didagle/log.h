// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/19
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#ifndef DIDAGLE_LOG
#include <fmt/core.h>
#include <stdio.h>
#define DIDAGLE_ERROR(...) \
  fmt::print(__VA_ARGS__); \
  printf("\n")
#define DIDAGLE_INFO(...)  \
  fmt::print(__VA_ARGS__); \
  printf("\n")
#define DIDAGLE_DEBUG(...) \
  fmt::print(__VA_ARGS__); \
  printf("\n")
#endif
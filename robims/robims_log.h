// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/19
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <fmt/core.h>
#include <stdio.h>
#include <string_view>
#include "spdlog/logger.h"

namespace robims {

class Logger {
 private:
 public:
  virtual bool ShouldLog(spdlog::level::level_enum log_level) = 0;
  virtual void Log(spdlog::source_loc loc, spdlog::level::level_enum lvl, std::string_view msg) = 0;
  static Logger& GetDidagleLogger();
  static void SetLogger(std::shared_ptr<Logger> logger);
  virtual ~Logger() {}
};

class Spdlogger : public Logger {
 public:
  Spdlogger();
  virtual bool ShouldLog(spdlog::level::level_enum log_level);
  virtual void Log(spdlog::source_loc loc, spdlog::level::level_enum lvl, std::string_view msg);
};

}  // namespace robims

#define ROBIMS_ERROR(...)                                                                  \
  do {                                                                                     \
    if (robims::Logger::GetDidagleLogger().ShouldLog(spdlog::level::err)) {                \
      std::string s = fmt::format(__VA_ARGS__);                                            \
      robims::Logger::GetDidagleLogger().Log(                                              \
          spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, s); \
    }                                                                                      \
  } while (0)

#define ROBIMS_INFO(...)                                                                    \
  do {                                                                                      \
    if (robims::Logger::GetDidagleLogger().ShouldLog(spdlog::level::info)) {                \
      std::string s = fmt::format(__VA_ARGS__);                                             \
      robims::Logger::GetDidagleLogger().Log(                                               \
          spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, s); \
    }                                                                                       \
  } while (0)
#define ROBIMS_DEBUG(...)                                                                    \
  do {                                                                                       \
    if (robims::Logger::GetDidagleLogger().ShouldLog(spdlog::level::debug)) {                \
      std::string s = fmt::format(__VA_ARGS__);                                              \
      robims::Logger::GetDidagleLogger().Log(                                                \
          spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, s); \
    }                                                                                        \
  } while (0)

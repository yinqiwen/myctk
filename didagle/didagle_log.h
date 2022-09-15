/*
 *Copyright (c) 2021, yinqiwen <yinqiwen@gmail.com>
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
#include <fmt/core.h>
#include <stdio.h>
#include <memory>
#include <string>
#include <string_view>
#include "spdlog/logger.h"

#include "fmt/ostream.h"  // do NOT put this line before spdlog

namespace didagle {

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

}  // namespace didagle

#define DIDAGLE_ERROR(...)                                                                             \
  do {                                                                                                 \
    if (didagle::Logger::GetDidagleLogger().ShouldLog(spdlog::level::err)) {                           \
      std::string s = fmt::format(__VA_ARGS__);                                                        \
      didagle::Logger::GetDidagleLogger().Log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
                                              spdlog::level::err, s);                                  \
    }                                                                                                  \
  } while (0)

#define DIDAGLE_INFO(...)                                                                              \
  do {                                                                                                 \
    if (didagle::Logger::GetDidagleLogger().ShouldLog(spdlog::level::info)) {                          \
      std::string s = fmt::format(__VA_ARGS__);                                                        \
      didagle::Logger::GetDidagleLogger().Log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
                                              spdlog::level::info, s);                                 \
    }                                                                                                  \
  } while (0)
#define DIDAGLE_DEBUG(...)                                                                             \
  do {                                                                                                 \
    if (didagle::Logger::GetDidagleLogger().ShouldLog(spdlog::level::debug)) {                         \
      std::string s = fmt::format(__VA_ARGS__);                                                        \
      didagle::Logger::GetDidagleLogger().Log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
                                              spdlog::level::debug, s);                                \
    }                                                                                                  \
  } while (0)

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
#include "didagle_log.h"
#include <memory>
#include "spdlog/spdlog.h"

namespace didagle {
static std::shared_ptr<Logger> g_logger;
Logger& Logger::GetDidagleLogger() {
  if (g_logger) {
    return *g_logger;
  }
  static Spdlogger default_logger;
  return default_logger;
}
void Logger::SetLogger(std::shared_ptr<Logger> logger) { g_logger = logger; }

Spdlogger::Spdlogger() {
  auto logger = spdlog::default_logger_raw();
  if (nullptr != logger) {
    logger->set_level(spdlog::level::debug);
  }
}
bool Spdlogger::ShouldLog(spdlog::level::level_enum log_level) {
  auto logger = spdlog::default_logger_raw();
  if (nullptr == logger) {
    return false;
  }
  return logger->should_log(log_level);
}
void Spdlogger::Log(spdlog::source_loc loc, spdlog::level::level_enum lvl, std::string_view msg) {
  auto logger = spdlog::default_logger_raw();
  if (nullptr == logger) {
    return;
  }
  logger->log(loc, lvl, msg);
}
}  // namespace didagle

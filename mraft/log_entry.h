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
#include <memory>
#include <string_view>

#include "folly/io/IOBuf.h"
#include "mraft/c_raft.h"

namespace mraft {

class LogEntry {
 private:
  raft_entry_t* entry_ = nullptr;
  bool own_ = true;

 public:
  LogEntry() = default;
  LogEntry(LogEntry&) = delete;
  LogEntry& operator=(LogEntry& other) = delete;
  LogEntry(LogEntry&& other);
  LogEntry& operator=(LogEntry&& other);
  LogEntry Clone();
  bool Empty() const;
  int64_t Term() const;
  int64_t Index() const;
  uint8_t Type() const;
  size_t DataLength() const;
  uint8_t* WritableData();
  const uint8_t* Data() const;
  raft_entry_t* TakeRawEntry();

  ~LogEntry();

  static LogEntry Wrap(int64_t index, int64_t term, uint8_t type, const std::string_view& s);
  static LogEntry Wrap(raft_entry_t* entry, bool own);
  static LogEntry Create(int64_t index, int64_t term, uint8_t type, uint32_t data_len);
};

}  // namespace mraft
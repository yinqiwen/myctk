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
#include "mraft/log_entry.h"
#include <folly/io/IOBuf.h>
#include <cstring>

namespace mraft {
LogEntry::LogEntry(LogEntry&& other) {
  entry_ = other.entry_;
  other.entry_ = nullptr;
}
LogEntry& LogEntry::operator=(LogEntry&& other) {
  entry_ = other.entry_;
  other.entry_ = nullptr;
}
LogEntry LogEntry::Clone() {
  LogEntry e;
  if (nullptr != entry_) {
    raft_entry_hold(entry_);
    e.entry_ = entry_;
  }
  return e;
}
int64_t LogEntry::Term() const { return entry_->term; }
int64_t LogEntry::Index() const { return entry_->id; }
uint8_t LogEntry::Type() const { return static_cast<uint8_t>(entry_->type); }
size_t LogEntry::DataLength() const { return static_cast<size_t>(entry_->data_len); }
uint8_t* LogEntry::WritableData() { return reinterpret_cast<uint8_t*>(entry_->data); }
raft_entry_t* LogEntry::TakeRawEntry() {
  raft_entry_t* e = entry_;
  entry_ = nullptr;
  return e;
}

LogEntry::~LogEntry() {
  if (nullptr != entry_ && own_) {
    raft_entry_release(entry_);
  }
}
bool LogEntry::Empty() const { return nullptr == entry_; }
const uint8_t* LogEntry::Data() const { return reinterpret_cast<const uint8_t*>(entry_->data); }
LogEntry LogEntry::Wrap(raft_entry_t* entry, bool own) {
  LogEntry e;
  e.entry_ = entry;
  e.own_ = own;
  return e;
}
LogEntry LogEntry::Wrap(int64_t index, int64_t term, uint8_t type, const std::string_view& s) {
  LogEntry e = Create(index, term, type, s.size());
  if (s.size() > 0) {
    memcpy(e.entry_->data, s.data(), s.size());
  }
  return e;
}
LogEntry LogEntry::Create(int64_t index, int64_t term, uint8_t type, uint32_t data_len) {
  raft_entry_t* entry = raft_entry_new(data_len);
  entry->term = term;
  entry->id = index;
  entry->type = type;
  LogEntry e;
  e.entry_ = entry;
  return e;
}
}  // namespace mraft

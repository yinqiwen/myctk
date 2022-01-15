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
#include <gtest/gtest.h>
#include <memory>
#include <string_view>
#include <vector>

#include "folly/portability/Filesystem.h"
#include "mraft/logger.h"
#include "mraft/segment_log_storage.h"

using namespace mraft;

class SegmentLogStorageTest : public testing::Test {
 protected:
  virtual void SetUp() { log_store.reset(new SegmentLogStorage("./segments")); }
  virtual void TearDown() {
    //
    folly::fs::remove_all("./segments");
  }

  std::unique_ptr<LogStorage> log_store;
};

TEST_F(SegmentLogStorageTest, simple) {
  int rc = log_store->Init();
  int64_t first_index = log_store->LastLogIndex();
  int64_t last_index = log_store->LastLogIndex();
  MRAFT_INFO("first_index:{},last_index:{}", first_index, last_index);
  EXPECT_EQ(0, rc);
  for (int i = last_index + 1; i < last_index + 1; i++) {
    LogEntry entry = LogEntry::Wrap(
        i, 1, 1, "hello,worldsadsdasfjkdsfjsdajkfdsafjhsdjkfjsdfjsdajfsdajkfksdajfjksdakjfsadfasdjkfsadjfasdjjk");
    std::string_view s0(reinterpret_cast<const char*>(entry.Data()), entry.DataLength());
    rc = log_store->AppendEntry(entry);
    EXPECT_EQ(0, rc);
    auto e = log_store->GetEntry(i);
    EXPECT_TRUE(e != nullptr);
    EXPECT_EQ(entry.Index(), e->Index());
    EXPECT_EQ(entry.Term(), e->Term());
    std::string_view s1(reinterpret_cast<const char*>(e->Data()), e->DataLength());
    EXPECT_EQ(s0, s1);
  }
}

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

#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/portability/Filesystem.h"
#include "mraft/logger.h"
#include "mraft/snapshot.h"

using namespace mraft;

class SnapshotTest : public testing::Test {
 protected:
  virtual void SetUp() { folly::fs::create_directories("./snapshots"); }
  virtual void TearDown() { folly::fs::remove_all("./snapshots"); }
};

TEST_F(SnapshotTest, Read) {
  Snapshot s("./snapshots", 1);
  int n1 = 1024;
  int n2 = 4098;
  {
    auto a = s.GetWritableFile("a");
    auto b = s.GetWritableFile("b");
    folly::ftruncateNoInt(a->GetFD(), n1);
    folly::ftruncateNoInt(b->GetFD(), n2);
  }
  EXPECT_EQ(s.Size(), n1 + n2);
  SnapshotChunk chunk;
  int64_t offset = 0;
  while (1) {
    void* chunk = nullptr;
    int64_t chunk_len = 0;
    bool last_chunk = false;

    int rc = s.Read(offset, chunk, chunk_len, last_chunk);
    EXPECT_EQ(rc, 0);
    offset += chunk_len;
    if (!last_chunk) {
      break;
    }
  }
  EXPECT_EQ(offset, s.Size());
}

TEST_F(SnapshotTest, Write) {}

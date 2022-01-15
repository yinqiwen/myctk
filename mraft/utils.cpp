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

#include "mraft/utils.h"
#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/String.h"
#include "folly/io/IOBuf.h"
#include "mraft/iobuf.h"
#include "mraft/logger.h"

namespace mraft {

bool has_prefix(const std::string_view& str, const std::string_view& prefix) {
  if (prefix.empty()) {
    return true;
  }
  if (str.size() < prefix.size()) {
    return false;
  }
  return str.find(prefix) == 0;
}
bool has_suffix(const std::string_view& fullString, const std::string_view& ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
  } else {
    return false;
  }
}
int pb_write_file(const std::string& file_path, const ::google::protobuf::Message& msg) {
  std::unique_ptr<folly::File> file;
  try {
    file = std::make_unique<folly::File>(file_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
  } catch (const std::exception& ex) {
    MRAFT_ERROR("Failed to open file:{} with msg:{}", file_path, ex.what());
    return -1;
  }
  IOBufAsZeroCopyOutputStream os;
  if (!msg.SerializeToZeroCopyStream(&os)) {
    return -1;
  }
  auto& data_buf = os.GetBuf();
  auto head_buf = folly::IOBuf::create(sizeof(uint32_t));
  uint32_t len = data_buf->length();
  if (data_buf->isChained()) {
    len = data_buf->computeChainDataLength();
  }
  uint8_t* head_data = head_buf->writableData();
  LocalInt<uint32_t>::Write(head_data, len);
  head_buf->append(sizeof(uint32_t));
  head_buf->appendChain(std::move(data_buf));
  const size_t to_write = len + head_buf->length();
  auto iov = head_buf->getIov();
  ssize_t n = folly::writevFull(file->fd(), iov.data(), iov.size());
  if (n != to_write) {
    int err = errno;
    MRAFT_ERROR("Fail to write to fd={}, path:{}, error:{}", file->fd(), file_path, folly::errnoStr(err));
    return -1;
  }
  return 0;
}
int pb_read_file(const std::string& file_path, ::google::protobuf::Message& msg) {
  std::unique_ptr<folly::File> file;
  try {
    file = std::make_unique<folly::File>(file_path);
  } catch (const std::exception& ex) {
    MRAFT_ERROR("Failed to open file:{} with msg:{}", file_path, ex.what());
    return -1;
  }
  auto head_buf = folly::IOBuf::create(sizeof(uint32_t));
  // get file size
  struct stat st_buf;
  if (fstat(file->fd(), &st_buf) != 0) {
    int save_err = errno;
    MRAFT_ERROR("Failed to get file stat:{} with error:{}", file_path, folly::errnoStr(save_err));
    return -1;
  }
  int64_t file_size = st_buf.st_size;
  uint32_t pb_len = 0;
  const ssize_t n = folly::readNoInt(file->fd(), &pb_len, sizeof(uint32_t));
  if (n != sizeof(uint32_t)) {
    int save_err = errno;
    MRAFT_ERROR("Failed to read len from file:{} with error:{}", file_path, folly::errnoStr(save_err));
    return -1;
  }
  pb_len = LocalInt<uint32_t>::ToLocalValue(pb_len);
  auto body_buf = folly::IOBuf::create(pb_len);
  const ssize_t msg_n = folly::readFull(file->fd(), body_buf->writableData(), pb_len);
  if (msg_n != pb_len) {
    int save_err = errno;
    MRAFT_ERROR("Failed to read {} bytes msg content from file:{} with error:{}", pb_len, file_path,
                folly::errnoStr(save_err));
    return -1;
  }
  body_buf->append(pb_len);
  IOBufAsZeroCopyInputStream is(body_buf.get());
  if (!msg.ParseFromZeroCopyStream(&is)) {
    MRAFT_ERROR("Failed to parse pb:{} from file", msg.GetTypeName());
    return -1;
  }
  return 0;
}

}  // namespace mraft
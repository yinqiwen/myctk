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
#include "ecache_common.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
namespace ecache {
uint64_t htonll(uint64_t val) { return (((uint64_t)htonl(val)) << 32) + htonl(val >> 32); }
uint64_t ntohll(uint64_t val) { return (((uint64_t)ntohl(val)) << 32) + ntohl(val >> 32); }
int64_t gettimeofday_us() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((long long)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}
int64_t gettimeofday_ms() { return gettimeofday_us() / 1000; }
int64_t gettimeofday_s() { return gettimeofday_us() / 1000000; }
int file_write_uint64(FILE* fp, uint64_t n) {
  n = htonll(n);
  int rc = fwrite(&n, sizeof(n), 1, fp);
  if (rc != 1) {
    return -1;
  }
  return 0;
}
int file_read_uint64(FILE* fp, uint64_t& n) {
  int rc = fread(&n, sizeof(n), 1, fp);
  if (rc != 1) {
    return -1;
  }
  n = ntohll(n);
  return 0;
}

int file_write_uint32(FILE* fp, uint32_t n) {
  n = htonl(n);
  int rc = fwrite(&n, sizeof(n), 1, fp);
  if (rc != 1) {
    return -1;
  }
  return 0;
}
int file_read_uint32(FILE* fp, uint32_t& n) {
  int rc = fread(&n, sizeof(n), 1, fp);
  if (rc != 1) {
    return -1;
  }
  n = ntohl(n);
  return 0;
}

int file_write_string(FILE* fp, folly::StringPiece s) {
  int rc = file_write_uint32(fp, s.size());
  if (0 != rc) {
    return -1;
  }
  rc = fwrite(s.data(), s.size(), 1, fp);
  if (rc != 1) {
    return -1;
  }
  return 0;
}
int file_read_string(FILE* fp, std::string& s) {
  uint32_t n;
  if (0 != file_read_uint32(fp, n)) {
    return -1;
  }
  s.resize(n);
  int rc = fread(&(s[0]), s.size(), 1, fp);
  if (rc != 1) {
    return -1;
  }
  return 0;
}
}  // namespace ecache
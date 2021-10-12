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
#include "robims_common.h"
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include "robims_cache.h"
#include "robims_log.h"

namespace robims {

uint64_t htonll(uint64_t val) { return (((uint64_t)htonl(val)) << 32) + htonl(val >> 32); }

uint64_t ntohll(uint64_t val) { return (((uint64_t)ntohl(val)) << 32) + ntohl(val >> 32); }

void bitmap_extract_ids(const roaring_bitmap_t* out, std::vector<uint32_t>& ids) {
  uint32_t n = roaring_bitmap_get_cardinality(out);
  ids.resize(n);
  roaring_bitmap_to_uint32_array(out, &ids[0]);
}
void bimap_get_ids(const BitmapOperation& op, std::vector<uint32_t>& ids) {
  BitMapCacheGuard guard;
  auto bitmap = acquire_bitmap();
  guard.Add(bitmap);
  op(bitmap);
  bitmap_extract_ids(bitmap, ids);
}

static bool roaring_iterator_func(uint32_t value, void* param) {
  CRoaringBitmapIterateFunc* func = (CRoaringBitmapIterateFunc*)param;
  return (*func)(value);
}

void iterate_bitmap(const CRoaringBitmapIterateFunc& func, const roaring_bitmap_t* bitmap) {
  void* data = (void*)(&func);
  roaring_iterate(bitmap, roaring_iterator_func, data);
}

void RoaringBitmap::SetCRoaringBitmap(roaring_bitmap_t* b) {
  CRoaringBitmapPtr tmp(b);
  bitmap = std::move(tmp);
}
void RoaringBitmap::NewCRoaringBitmap() {
  roaring_bitmap_t* b = roaring_bitmap_create();
  roaring_bitmap_init_cleared(b);
  // roaring_bitmap_set_copy_on_write(b, true);
  CRoaringBitmapPtr tmp(b);
  bitmap = std::move(tmp);
}
bool RoaringBitmap::Put(uint32_t id) { return roaring_bitmap_add_checked(bitmap.get(), id); }
bool RoaringBitmap::Remove(uint32_t id) { return roaring_bitmap_remove_checked(bitmap.get(), id); }
int RoaringBitmap::Save(FILE* fp, bool readonly) {
  // off_t cur = ::lseek(fd, 0, SEEK_CUR);
  roaring_bitmap_run_optimize(bitmap.get());
  roaring_bitmap_shrink_to_fit(bitmap.get());
  char* mbuf = nullptr;
  size_t data_len = 0;
  if (readonly) {
    size_t nbytes = roaring_bitmap_frozen_size_in_bytes(bitmap.get());
    mbuf = (char*)malloc(nbytes);
    roaring_bitmap_frozen_serialize(bitmap.get(), mbuf);
    data_len = nbytes;
  } else {
    size_t nbytes = roaring_bitmap_size_in_bytes(bitmap.get());
    mbuf = (char*)malloc(nbytes);
    data_len = roaring_bitmap_serialize(bitmap.get(), mbuf);
  }
  int rc = file_write_uint32(fp, data_len);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to write bitmap buf len.");
    free(mbuf);
    return rc;
  }
  rc = fwrite(&readonly, sizeof(bool), 1, fp);
  if (rc != 1) {
    ROBIMS_ERROR("Failed to write bitmap readonly flag.");
    free(mbuf);
    return -1;
  }
  rc = fwrite(mbuf, data_len, 1, fp);
  if (rc != 1) {
    ROBIMS_ERROR("Failed to write bitmap buf");
    free(mbuf);
    return -1;
  }
  free(mbuf);
  return 0;
}

int RoaringBitmap::Load(FILE* fp) {
  uint32_t n = 0;
  int rc = file_read_uint32(fp, n);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to read bitmap buf len");
    return -1;
  }
  bool readonly = false;
  rc = fread(&readonly, sizeof(bool), 1, fp);
  if (rc != 1) {
    ROBIMS_ERROR("Failed to read bitmap readonly flag");
    return -1;
  }
  char* mbuf = (char*)(std::aligned_alloc(32, n));
  rc = fread(mbuf, n, 1, fp);
  if (rc != 1) {
    ROBIMS_ERROR("Failed to read bitmap buf data");
    std::free(mbuf);
    return -1;
  }
  if (!readonly) {
    CRoaringBitmapPtr tmp(roaring_bitmap_deserialize(mbuf));
    bitmap = std::move(tmp);
    std::free(mbuf);
  } else {
    const roaring_bitmap_t* b = roaring_bitmap_frozen_view(mbuf, n);
    if (nullptr == b) {
      ROBIMS_ERROR("roaring_bitmap_frozen_view failed with {}", n);
    }
    CRoaringBitmapDeleter deleter(true);
    CRoaringBitmapPtr tmp((const_cast<roaring_bitmap_t*>(b)), deleter);
    bitmap = std::move(tmp);
    _underly_buf = mbuf;
  }
  if (!bitmap) {
    ROBIMS_ERROR("bitmap init failed");
    return -1;
  }
  return 0;
}

RoaringBitmap::~RoaringBitmap() {
  if (nullptr != _underly_buf) {
    std::free(_underly_buf);
  }
}

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

int file_write_string(FILE* fp, std::string_view s) {
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

}  // namespace robims

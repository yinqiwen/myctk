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
#include <stdio.h>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include "roaring/roaring.h"
#include "robims_cache.h"

namespace robims {
uint64_t htonll(uint64_t val);
uint64_t ntohll(uint64_t val);

struct CRoaringBitmapDeleter {
  bool ignore_free = false;
  CRoaringBitmapDeleter(bool ignore = false) : ignore_free(ignore) {}
  void operator()(roaring_bitmap_t* ptr) const {
    if (!ignore_free) {
      release_bitmap(ptr);
    }
  }
};
typedef std::unique_ptr<roaring_bitmap_t, CRoaringBitmapDeleter> CRoaringBitmapPtr;
struct RoaringBitmap {
  CRoaringBitmapPtr bitmap;
  char* _underly_buf = nullptr;
  bool _readonly = false;
  RoaringBitmap() = default;
  RoaringBitmap(const RoaringBitmap&) = delete;
  RoaringBitmap& operator=(const RoaringBitmap&) = delete;

  void SetCRoaringBitmap(roaring_bitmap_t* b);
  void NewCRoaringBitmap();
  int Save(FILE* fp, bool readonly);
  int Load(FILE* fp);
  bool Put(uint32_t id);
  bool Remove(uint32_t id);
  bool IsReadonly() const { return _readonly; }
  ~RoaringBitmap();
};
typedef std::unique_ptr<RoaringBitmap> RoaringBitmapPtr;
RoaringBitmapPtr fd_read_roaring_bitmap(int fd);

typedef std::function<bool(uint32_t)> CRoaringBitmapIterateFunc;
void iterate_bitmap(const CRoaringBitmapIterateFunc& func, const roaring_bitmap_t* bitmap);

void bitmap_extract_ids(const roaring_bitmap_t* out, std::vector<uint32_t>& ids);

typedef std::function<void(roaring_bitmap_t*)> BitmapOperation;
void bimap_get_ids(const BitmapOperation& op, std::vector<uint32_t>& ids);

int file_write_string(FILE* fp, std::string_view s);
int file_read_string(FILE* fp, std::string& s);
int file_write_uint32(FILE* fp, uint32_t n);
int file_read_uint32(FILE* fp, uint32_t& n);
int file_write_uint64(FILE* fp, uint64_t n);
int file_read_uint64(FILE* fp, uint64_t& n);

}  // namespace robims
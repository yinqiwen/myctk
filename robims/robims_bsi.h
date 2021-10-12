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
#include <vector>
#include "roaring/roaring.h"
#include "robims.pb.h"
#include "robims_common.h"

namespace robims {

uint32_t float_to_uint32(float v);
float uint32_to_float(uint32_t v);

class BitSliceIndexCreateOptions {
 private:
  int64_t _min_int = 0;
  int64_t _max_int = 0;
  uint64_t _min = 0;
  uint64_t _max = 0;
  uint32_t _limit_topk = 0;

 public:
  BitSliceIndexCreateOptions();
  BitSliceIndexCreateOptions& SetMinMax(float min_d, float max_d);
  BitSliceIndexCreateOptions& SetMinMax(int64_t min_i, int64_t max_i);
  uint64_t Distance();
  int64_t GetActualIntVal(uint64_t v);
  float GetActualFloatVal(uint64_t v);
  uint64_t ToLocalVal(int64_t v);
  uint64_t ToLocalVal(float v);
  uint32_t LimitTopK() { return _limit_topk; }
  void SetLimitTopK(uint32_t v) { _limit_topk = v; }
};
class BitSliceIndex {
 protected:
  BitSliceIndex(BitSliceIndex&) = delete;
  BitSliceIndex& operator=(const BitSliceIndex&) = delete;
  BitSliceIndexCreateOptions _options;
  std::vector<RoaringBitmapPtr> _bitmaps;
  uint8_t _bit_depth = 0;
  int DoGetMax(roaring_bitmap_t* filter, uint64_t& max, int64_t& count);
  int DoGetMin(roaring_bitmap_t* filter, uint64_t& min, int64_t& count);
  int DoRangeLT(uint64_t expect, bool allow_eq, roaring_bitmap_t* out);
  int DoRangeGT(uint64_t expect, bool allow_eq, roaring_bitmap_t* out);
  int DoRangeEQ(uint64_t expect, roaring_bitmap_t* out);
  int DoRangeNEQ(uint64_t expect, roaring_bitmap_t* out);
  int DoRangeBetween(uint64_t min, uint64_t max, roaring_bitmap_t* out);
  uint64_t DoRemoveMin();
  void DoRemove(uint32_t id, uint64_t val);
  bool DoPut(uint32_t id, uint64_t val, uint64_t& old_val);
  bool DoGet(uint32_t id, uint64_t& val);

  int DoInit(const FieldMeta& meta);

 public:
  BitSliceIndex() = default;
  int Init(const FieldMeta& meta);
  int TopK(uint32_t k, roaring_bitmap_t* out);
  int Load(FILE* fp);
  int Save(FILE* fp, bool readonly);
  const roaring_bitmap_t* GetExistBitmap();
};

class BitSliceIntIndex : public BitSliceIndex {
 public:
  int GetMin(roaring_bitmap_t* filter, int64_t& min, int64_t& count);
  int GetMax(roaring_bitmap_t* filter, int64_t& max, int64_t& count);
  int RangeBetween(int64_t min, int64_t max, roaring_bitmap_t* out);
  int RangeLT(int64_t expect, bool allow_eq, roaring_bitmap_t* out);
  int RangeGT(int64_t expect, bool allow_eq, roaring_bitmap_t* out);
  int RangeEQ(int64_t expect, roaring_bitmap_t* out);
  int RangeNEQ(int64_t expect, roaring_bitmap_t* out);
  void Remove(uint32_t id, int64_t val);
  void Put(uint32_t id, int64_t val);
  bool Get(uint32_t id, int64_t& val);
  int64_t RemoveMin();
};

typedef std::unique_ptr<BitSliceIntIndex> BitSliceIntIndexPtr;

class BitSliceFloatIndex : public BitSliceIndex {
 private:
 public:
  int GetMin(roaring_bitmap_t* filter, float& min, int64_t& count);
  int GetMax(roaring_bitmap_t* filter, float& max, int64_t& count);
  int RangeBetween(float min, float max, roaring_bitmap_t* out);
  int RangeLT(float expect, bool allow_eq, roaring_bitmap_t* out);
  int RangeGT(float expect, bool allow_eq, roaring_bitmap_t* out);
  int RangeEQ(float expect, roaring_bitmap_t* out);
  int RangeNEQ(float expect, roaring_bitmap_t* out);
  double RemoveMin();
  void Remove(uint32_t id, float val);
  bool Put(uint32_t id, float val, float& old_val);
  bool Get(uint32_t id, float& val);
};
typedef std::unique_ptr<BitSliceFloatIndex> BitSliceFloatIndexPtr;

}  // namespace robims
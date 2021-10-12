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
#include "robims_bsi.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include "robims_cache.h"
#include "robims_common.h"
#include "robims_log.h"
namespace robims {

uint32_t float_to_uint32(float v) {
  uint32_t iv = 0;
  memcpy(&iv, &v, sizeof(v));
  if (v >= 0) {
    iv |= (0x80000000);
  } else {
    iv = (~iv);
  }
  return iv;
}

float uint32_to_float(uint32_t v) {
  if ((v & 0x80000000) > 0) {
    v &= (~0x80000000);
  } else {
    v = (~v);
  }
  float d;
  memcpy(&d, &v, sizeof(v));
  return d;
}

static uint64_t double_to_uint64(double v) {
  uint64_t iv = 0;
  memcpy(&iv, &v, sizeof(v));
  if (v >= 0) {
    iv |= (0x8000000000000000ull);
  } else {
    iv = (~iv);
  }
  return iv;
}

static double uint64_to_double(uint64_t v) {
  if ((v & 0x8000000000000000ull) > 0) {
    v &= (~0x8000000000000000ull);
  } else {
    v = (~v);
  }
  double d;
  memcpy(&d, &v, sizeof(v));
  return d;
}

BitSliceIndexCreateOptions::BitSliceIndexCreateOptions() { SetMinMax((int64_t)0, INT_MAX); }

uint64_t BitSliceIndexCreateOptions::Distance() { return _max - _min; }

BitSliceIndexCreateOptions& BitSliceIndexCreateOptions::SetMinMax(int64_t min_i, int64_t max_i) {
  _min_int = min_i;
  _max_int = max_i;
  if (0 == _min_int && 0 == _max_int) {
    _max_int = INT_MAX;
  }
  _min = 0;
  _max = _max_int - _min_int;

  return *this;
}

BitSliceIndexCreateOptions& BitSliceIndexCreateOptions::SetMinMax(float min_d, float max_d) {
  if (0 == min_d && 0 == max_d) {
    max_d = INT_MAX;
  }
  _min = float_to_uint32(min_d);
  _max = float_to_uint32(max_d);
  return *this;
}
int64_t BitSliceIndexCreateOptions::GetActualIntVal(uint64_t v) { return (int64_t)v + _min_int; }
float BitSliceIndexCreateOptions::GetActualFloatVal(uint64_t v) {
  return uint32_to_float(v + _min);
}
uint64_t BitSliceIndexCreateOptions::ToLocalVal(int64_t v) { return (uint64_t)(v - _min_int); }
uint64_t BitSliceIndexCreateOptions::ToLocalVal(float v) { return float_to_uint32(v) - _min; }
int BitSliceIndex::DoInit(const FieldMeta& meta) {
  if (meta.index_type() == FLOAT_INDEX) {
    _options.SetMinMax((float)meta.min(), (float)meta.max());
  } else if (meta.index_type() == SET_INDEX || meta.index_type() == WEIGHT_SET_INDEX) {
    _options.SetMinMax((int64_t)0, (int64_t)(meta.max_set_num()));
  } else {
    _options.SetMinMax((int64_t)(meta.min()), (int64_t)(meta.max()));
  }
  _options.SetLimitTopK((uint32_t)meta.topk_limit());

  uint64_t distance = _options.Distance();
  for (uint8_t i = 0; i < 64; i++) {
    if (distance < (1uLL << i)) {
      _bit_depth = i;
      break;
    }
  }
  ROBIMS_INFO("BitSliceIndex bit_depth:{}", _bit_depth);
  return 0;
}
int BitSliceIndex::Init(const FieldMeta& meta) {
  //_options = opt;
  DoInit(meta);
  for (int32_t i = 0; i < _bit_depth + 1; i++) {
    RoaringBitmapPtr p(new RoaringBitmap);
    p->NewCRoaringBitmap();
    _bitmaps.push_back(std::move(p));
  }
  return 0;
}
const roaring_bitmap_t* BitSliceIndex::GetExistBitmap() { return _bitmaps[0]->bitmap.get(); }
int BitSliceIndex::DoGetMax(roaring_bitmap_t* input_filter, uint64_t& max_val, int64_t& count) {
  max_val = 0;
  count = 0;
  roaring_bitmap_t* filter = input_filter;
  BitMapCacheGuard guard;
  if (nullptr == filter) {
    filter = acquire_bitmap();
    guard.Add(filter);
  }
  if (roaring_bitmap_is_empty(filter)) {
    roaring_bitmap_overwrite(filter, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(filter, _bitmaps[0]->bitmap.get());
  }
  int64_t last_non_zero_filter_count = roaring_bitmap_get_cardinality(filter);
  uint32_t tmp_cursor = 0;
  roaring_bitmap_t* tmp[2];
  tmp[0] = acquire_bitmap();
  tmp[1] = acquire_bitmap();
  guard.Add(tmp[0]);
  guard.Add(tmp[1]);

  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    roaring_bitmap_overwrite(tmp[tmp_cursor], filter);
    roaring_bitmap_and_inplace(tmp[tmp_cursor], _bitmaps[1 + i]->bitmap.get());
    int64_t and_count = roaring_bitmap_get_cardinality(tmp[tmp_cursor]);
    if (and_count > 0) {
      max_val += (1 << i);
      count = and_count;
      filter = tmp[tmp_cursor];
      tmp_cursor = 1 - tmp_cursor;
    } else {
      if (i == 0) {
        count = last_non_zero_filter_count;
      }
    }
    if (and_count > 0) {
      last_non_zero_filter_count = and_count;
    }
  }
  return 0;
}

int BitSliceIndex::DoGetMin(roaring_bitmap_t* input_filter, uint64_t& min_val, int64_t& count) {
  min_val = 0;
  count = 0;
  BitMapCacheGuard guard;
  roaring_bitmap_t* filter = input_filter;
  if (nullptr == filter) {
    filter = acquire_bitmap();
    guard.Add(filter);
  }
  if (roaring_bitmap_is_empty(filter)) {
    roaring_bitmap_overwrite(filter, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(filter, _bitmaps[0]->bitmap.get());
  }
  int64_t last_non_zero_filter_count = -1;
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    roaring_bitmap_andnot_inplace(filter, _bitmaps[1 + i]->bitmap.get());
    int64_t diff_count = roaring_bitmap_get_cardinality(filter);
    if (diff_count > 0) {
      count = diff_count;
    } else {
      min_val += (1 << i);
      if (0 == i) {
        count = last_non_zero_filter_count;
      }
    }
    if (diff_count > 0) {
      last_non_zero_filter_count = diff_count;
    }
  }
  return 0;
}

int BitSliceIndex::DoRangeLT(uint64_t expect_val, bool allow_eq, roaring_bitmap_t* out) {
  if (roaring_bitmap_is_empty(out)) {
    roaring_bitmap_overwrite(out, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(out, _bitmaps[0]->bitmap.get());
  }
  bool leading_zero = true;
  roaring_bitmap_t* filter = out;
  BitMapCacheGuard guard;
  roaring_bitmap_t* keep = acquire_bitmap();
  roaring_bitmap_t* tmp = acquire_bitmap();
  guard.Add(keep);
  guard.Add(tmp);
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    uint8_t bit = (expect_val >> i) & 0x1;
    if (leading_zero) {
      if (bit == 0) {
        roaring_bitmap_andnot_inplace(filter, _bitmaps[1 + i]->bitmap.get());
        continue;
      } else {
        leading_zero = false;
      }
    }

    if (i == 0 && !allow_eq) {
      if (bit == 0) {
        roaring_bitmap_overwrite(filter, keep);
        return 0;
      } else {
        // return filter.Difference(row.Difference(keep)), nil
        roaring_bitmap_xor_inplace(keep, _bitmaps[1 + i]->bitmap.get());
        roaring_bitmap_andnot_inplace(keep, _bitmaps[1 + i]->bitmap.get());
        roaring_bitmap_andnot_inplace(filter, keep);
        return 0;
      }
    }

    // If bit is zero then remove all set columns not in excluded bitmap.
    if (bit == 0) {
      roaring_bitmap_overwrite(tmp, _bitmaps[1 + i]->bitmap.get());
      roaring_bitmap_andnot_inplace(tmp, keep);
      // filter = filter.Difference(row.Difference(keep))
      roaring_bitmap_andnot_inplace(filter, tmp);
      continue;
    }

    // If bit is set then add columns for set bits to exclude.
    // Don't bother to compute this on the final iteration.
    if (i > 0) {
      roaring_bitmap_overwrite(tmp, filter);
      roaring_bitmap_andnot_inplace(tmp, _bitmaps[1 + i]->bitmap.get());
      roaring_bitmap_or_inplace(keep, tmp);
      // keep = keep.Union(filter.Difference(row));
    }
  }
  return 0;
}

int BitSliceIndex::DoRangeGT(uint64_t expect_val, bool allow_eq, roaring_bitmap_t* out) {
  BitMapCacheGuard guard;
  if (roaring_bitmap_is_empty(out)) {
    roaring_bitmap_overwrite(out, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(out, _bitmaps[0]->bitmap.get());
  }
  roaring_bitmap_t* filter = out;
  roaring_bitmap_t* keep = acquire_bitmap();
  roaring_bitmap_t* tmp = acquire_bitmap();
  guard.Add(keep);
  guard.Add(tmp);
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    uint8_t bit = (expect_val >> i) & 0x1;
    // Handle last bit differently.
    // If bit is one then return only already kept columns.
    // If bit is zero then remove any unset columns.
    if (i == 0 && !allow_eq) {
      if (bit == 1) {
        roaring_bitmap_overwrite(filter, keep);
        return 0;
      }
      roaring_bitmap_overwrite(tmp, filter);
      roaring_bitmap_andnot_inplace(tmp, _bitmaps[1 + i]->bitmap.get());
      roaring_bitmap_andnot_inplace(tmp, keep);
      roaring_bitmap_andnot_inplace(filter, tmp);
      return 0;
    }

    // If bit is set then remove all unset columns not already kept.
    if (bit == 1) {
      roaring_bitmap_overwrite(tmp, filter);
      roaring_bitmap_andnot_inplace(tmp, _bitmaps[1 + i]->bitmap.get());
      roaring_bitmap_andnot_inplace(tmp, keep);
      roaring_bitmap_andnot_inplace(filter, tmp);
      continue;
    }
    // If bit is unset then add columns with set bit to keep.
    // Don't bother to compute this on the final iteration.
    if (i > 0) {
      roaring_bitmap_overwrite(tmp, filter);
      roaring_bitmap_and_inplace(tmp, _bitmaps[1 + i]->bitmap.get());
      roaring_bitmap_or_inplace(keep, tmp);
    }
  }
  return 0;
}
int BitSliceIndex::DoRangeEQ(uint64_t expect_val, roaring_bitmap_t* out) {
  if (roaring_bitmap_is_empty(out)) {
    roaring_bitmap_overwrite(out, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(out, _bitmaps[0]->bitmap.get());
  }
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    uint8_t bit = (expect_val >> i) & 0x1;
    if (bit) {
      roaring_bitmap_and_inplace(out, _bitmaps[1 + i]->bitmap.get());
    } else {
      roaring_bitmap_andnot_inplace(out, _bitmaps[1 + i]->bitmap.get());
    }
  }
  return 0;
}

int BitSliceIndex::DoRangeNEQ(uint64_t expect, roaring_bitmap_t* out) {
  BitMapCacheGuard guard;
  roaring_bitmap_overwrite(out, _bitmaps[0]->bitmap.get());
  auto eq = acquire_bitmap();
  guard.Add(eq);
  DoRangeEQ(expect, eq);
  roaring_bitmap_andnot_inplace(out, eq);
  return 0;
}

int BitSliceIndex::DoRangeBetween(uint64_t expect_min, uint64_t expect_max,
                                  roaring_bitmap_t* filter) {
  if (roaring_bitmap_is_empty(filter)) {
    roaring_bitmap_overwrite(filter, _bitmaps[0]->bitmap.get());
  } else {
    roaring_bitmap_and_inplace(filter, _bitmaps[0]->bitmap.get());
  }
  BitMapCacheGuard guard;
  roaring_bitmap_t* keep1 = acquire_bitmap();
  roaring_bitmap_t* keep2 = acquire_bitmap();
  roaring_bitmap_t* tmp = acquire_bitmap();
  guard.Add(keep1);
  guard.Add(keep2);
  guard.Add(tmp);
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    auto row = _bitmaps[1 + i]->bitmap.get();
    uint8_t bit1 = (expect_min >> i) & 0x1;
    uint8_t bit2 = (expect_max >> i) & 0x1;
    // GTE predicateMin
    // If bit is set then remove all unset columns not already kept.
    if (bit1 == 1) {
      roaring_bitmap_overwrite(tmp, filter);
      roaring_bitmap_andnot_inplace(tmp, row);
      roaring_bitmap_andnot_inplace(tmp, keep1);
      roaring_bitmap_andnot_inplace(filter, tmp);
    } else {
      // If bit is unset then add columns with set bit to keep.
      // Don't bother to compute this on the final iteration.
      if (i > 0) {
        roaring_bitmap_overwrite(tmp, filter);
        roaring_bitmap_and_inplace(tmp, row);
        roaring_bitmap_or_inplace(keep1, tmp);
      }
    }

    // LTE predicateMax
    // If bit is zero then remove all set bits not in excluded bitmap.
    if (bit2 == 0) {
      roaring_bitmap_overwrite(tmp, row);
      roaring_bitmap_andnot_inplace(tmp, keep2);
      roaring_bitmap_andnot_inplace(filter, tmp);
    } else {
      // If bit is set then add columns for set bits to exclude.
      // Don't bother to compute this on the final iteration.
      if (i > 0) {
        roaring_bitmap_overwrite(tmp, filter);
        roaring_bitmap_andnot_inplace(tmp, row);
        roaring_bitmap_or_inplace(keep2, tmp);
      }
    }
  }
  return 0;
}

// int BitSliceIndex::Visit() {
//   BitMapCacheGuard guard;
//   auto rest_items = acquire_bitmap();
//   guard.Add(rest_items);
//   auto tmp = acquire_bitmap();
//   guard.Add(tmp);
//   roaring_bitmap_overwrite(rest_items, _bitmaps[0]->bitmap.get());
//   for (int32_t i = _bit_depth - 1; i >= 0; i--) {
//     auto row = _bitmaps[1 + i]->bitmap.get();
//     roaring_bitmap_overwrite(tmp, rest_items);
//     roaring_bitmap_and_inplace(tmp, row);
//     // iterate tmp
//     roaring_bitmap_andnot_inplace(rest_items, tmp);
//   }
//   return 0;
// }

int BitSliceIndex::TopK(uint32_t k, roaring_bitmap_t* out) {
  BitMapCacheGuard guard;
  auto filter = acquire_bitmap();
  guard.Add(filter);
  roaring_bitmap_overwrite(filter, _bitmaps[0]->bitmap.get());
  uint32_t rest_k = k;
  auto tmp = acquire_bitmap();
  guard.Add(tmp);
  for (int32_t i = _bit_depth - 1; i >= 0; i--) {
    auto row = _bitmaps[1 + i]->bitmap.get();
    roaring_bitmap_overwrite(tmp, filter);
    int n = roaring_bitmap_get_cardinality(filter);
    roaring_bitmap_and_inplace(filter, row);
    n = roaring_bitmap_get_cardinality(filter);
    if (n > rest_k) {
      continue;
    } else if (n < rest_k) {
      if (0 == n && i > 0) {
        roaring_bitmap_overwrite(filter, tmp);
        continue;
      }
      roaring_bitmap_or_inplace(out, filter);
      rest_k = k - roaring_bitmap_get_cardinality(out);
      roaring_bitmap_andnot_inplace(tmp, out);
      roaring_bitmap_overwrite(filter, tmp);
    } else {
      roaring_bitmap_or_inplace(out, filter);
      break;
    }
  }
  // int n = roaring_bitmap_get_cardinality(out);
  // ROBIMS_INFO("last match n:{}\n", n);
  // int n = roaring_bitmap_get_cardinality(g) - k;
  return 0;
}

uint64_t BitSliceIndex::DoRemoveMin() {
  int64_t min_count = 0;
  uint64_t min_val = 0;
  BitMapCacheGuard guard;
  auto bitmap = acquire_bitmap();
  DoGetMin(bitmap, min_val, min_count);
  if (min_count > 0) {
    roaring_bitmap_t* tmp = acquire_bitmap();
    guard.Add(tmp);
    DoRangeEQ(min_val, tmp);
    std::vector<uint32_t> ids;
    bitmap_extract_ids(tmp, ids);
    DoRemove(ids[0], min_val);
    return min_val;
  }
  return 0;
}
void BitSliceIndex::DoRemove(uint32_t id, uint64_t val) {
  roaring_bitmap_remove(_bitmaps[0]->bitmap.get(), id);
  uint64_t set_val = val;
  for (int32_t i = 0; i < _bit_depth; i++) {
    if (set_val & (1ull << i)) {
      roaring_bitmap_remove(_bitmaps[i + 1]->bitmap.get(), id);
    }
  }
}

bool BitSliceIndex::DoPut(uint32_t id, uint64_t val, uint64_t& old_val) {
  bool inserted = roaring_bitmap_add_checked(_bitmaps[0]->bitmap.get(), id);  // set Not NULL bitmap
  if (inserted) {
    uint64_t set_val = val;
    for (int32_t i = 0; i < _bit_depth; i++) {
      if (set_val & (1ull << i)) {
        roaring_bitmap_add(_bitmaps[i + 1]->bitmap.get(), id);
      } else {
        roaring_bitmap_remove(_bitmaps[i + 1]->bitmap.get(), id);
      }
    }
    if (_options.LimitTopK() > 0) {
      uint64_t count = roaring_bitmap_get_cardinality(_bitmaps[0]->bitmap.get());
      if (count > _options.LimitTopK()) {
        DoRemoveMin();
      }
    }
    return false;
  } else {
    old_val = 0;
    uint64_t set_val = val;
    for (int32_t i = 0; i < _bit_depth; i++) {
      if (roaring_bitmap_contains(_bitmaps[1 + i]->bitmap.get(), id)) {
        old_val += (1ull << i);
      }
      if (set_val & (1ull << i)) {
        roaring_bitmap_add(_bitmaps[i + 1]->bitmap.get(), id);
      } else {
        roaring_bitmap_remove(_bitmaps[i + 1]->bitmap.get(), id);
      }
    }
    return true;
  }
}

bool BitSliceIndex::DoGet(uint32_t id, uint64_t& val) {
  if (!roaring_bitmap_contains(_bitmaps[0]->bitmap.get(), id)) {
    return false;
  }
  val = 0;
  for (int32_t i = 0; i < _bit_depth; i++) {
    if (roaring_bitmap_contains(_bitmaps[1 + i]->bitmap.get(), id)) {
      val += (1ull << i);
    }
  }
  return true;
}

int BitSliceIndex::Load(FILE* fp) {
  uint32_t n = 0;
  if (0 != file_read_uint32(fp, n)) {
    return -1;
  }
  for (uint32_t i = 0; i < n; i++) {
    RoaringBitmapPtr p(new RoaringBitmap);
    _bitmaps.emplace_back(std::move(p));
    if (0 != _bitmaps[i]->Load(fp)) {
      return -1;
    }
    // ROBIMS_ERROR("[{}]Load n={}", i, roaring_bitmap_get_cardinality(_bitmaps[i]->bitmap.get()));
  }
  return 0;
}
int BitSliceIndex::Save(FILE* fp, bool readonly) {
  if (0 != file_write_uint32(fp, _bitmaps.size())) {
    return -1;
  }
  for (auto& b : _bitmaps) {
    if (0 != b->Save(fp, readonly)) {
      return -1;
    }
  }
  return 0;
}

int BitSliceIntIndex::GetMax(roaring_bitmap_t* input_filter, int64_t& max_val, int64_t& count) {
  uint64_t local_max_val = 0;
  DoGetMax(input_filter, local_max_val, count);
  max_val = _options.GetActualIntVal(local_max_val);
  return 0;
}
int BitSliceIntIndex::GetMin(roaring_bitmap_t* input_filter, int64_t& min_val, int64_t& count) {
  uint64_t local_min_val = 0;
  DoGetMin(input_filter, local_min_val, count);
  min_val = _options.GetActualIntVal(local_min_val);
  return 0;
}
int BitSliceIntIndex::RangeLT(int64_t expect, bool allow_eq, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeLT(expect_val, allow_eq, out);
  return 0;
}

int BitSliceIntIndex::RangeGT(int64_t expect, bool allow_eq, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);

  DoRangeGT(expect_val, allow_eq, out);
  return 0;
}
int BitSliceIntIndex::RangeEQ(int64_t expect, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeEQ(expect_val, out);
  return 0;
}
int BitSliceIntIndex::RangeNEQ(int64_t expect, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeNEQ(expect_val, out);
  return 0;
}

void BitSliceIntIndex::Remove(uint32_t id, int64_t val) { DoRemove(id, _options.ToLocalVal(val)); }
int BitSliceIntIndex::RangeBetween(int64_t min, int64_t max, roaring_bitmap_t* filter) {
  return DoRangeBetween(_options.ToLocalVal(min), _options.ToLocalVal(max), filter);
}

void BitSliceIntIndex::Put(uint32_t id, int64_t val) {
  uint64_t old_val;
  DoPut(id, _options.ToLocalVal(val), old_val);
}

bool BitSliceIntIndex::Get(uint32_t id, int64_t& val) {
  uint64_t local_val;
  if (!DoGet(id, local_val)) {
    return false;
  }
  val = _options.GetActualIntVal(local_val);
  return true;
}

int64_t BitSliceIntIndex::RemoveMin() {
  uint64_t local_Val = DoRemoveMin();
  return _options.GetActualIntVal(local_Val);
}

int BitSliceFloatIndex::GetMax(roaring_bitmap_t* input_filter, float& max_val, int64_t& count) {
  uint64_t local_max_val = 0;
  DoGetMax(input_filter, local_max_val, count);
  max_val = _options.GetActualFloatVal(local_max_val);
  return 0;
}
int BitSliceFloatIndex::GetMin(roaring_bitmap_t* input_filter, float& min_val, int64_t& count) {
  uint64_t local_min_val = 0;
  DoGetMin(input_filter, local_min_val, count);
  min_val = _options.GetActualFloatVal(local_min_val);
  return 0;
}

int BitSliceFloatIndex::RangeLT(float expect, bool allow_eq, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeLT(expect_val, allow_eq, out);
  return 0;
}

int BitSliceFloatIndex::RangeGT(float expect, bool allow_eq, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeGT(expect_val, allow_eq, out);
  return 0;
}
int BitSliceFloatIndex::RangeEQ(float expect, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeEQ(expect_val, out);
  return 0;
}
int BitSliceFloatIndex::RangeNEQ(float expect, roaring_bitmap_t* out) {
  uint64_t expect_val = _options.ToLocalVal(expect);
  DoRangeNEQ(expect_val, out);
  return 0;
}

int BitSliceFloatIndex::RangeBetween(float min, float max, roaring_bitmap_t* filter) {
  return DoRangeBetween(_options.ToLocalVal(min), _options.ToLocalVal(max), filter);
}

void BitSliceFloatIndex::Remove(uint32_t id, float val) { DoRemove(id, _options.ToLocalVal(val)); }

bool BitSliceFloatIndex::Put(uint32_t id, float val, float& old_val) {
  uint64_t old;
  if (DoPut(id, _options.ToLocalVal(val), old)) {
    old_val = uint32_to_float(old);
    return true;
  }
  return false;
}

bool BitSliceFloatIndex::Get(uint32_t id, float& val) {
  uint64_t local_val;
  if (!DoGet(id, local_val)) {
    return false;
  }
  val = _options.GetActualFloatVal(local_val);
  return true;
}

double BitSliceFloatIndex::RemoveMin() {
  uint64_t local_Val = DoRemoveMin();
  return _options.GetActualFloatVal(local_Val);
}

}  // namespace robims

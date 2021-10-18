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
#include <robims_bsi.h>
#include "robims_common.h"
#include "robims_err.h"
#include "robims_field.h"
#include "robims_log.h"
#include "robims_table.h"
namespace robims {
int RobimsWeightSetField::OnInit() { return 0; }
int RobimsWeightSetField::DoSave(FILE* fp, bool readonly) {
  int rc = file_write_uint32(fp, _bitmaps.size());
  if (0 != rc) {
    return rc;
  }
  for (auto& pair : _bitmaps) {
    rc = file_write_string(fp, pair.second->name);
    if (0 != rc) {
      return rc;
    }
    std::vector<uint64_t> tmp(pair.second->weight_ids.begin(), pair.second->weight_ids.end());
    int rc = file_write_uint32(fp, tmp.size());
    if (0 != rc) {
      return rc;
    }
    if (tmp.size() > 0) {
      rc = ::fwrite(tmp.data(), tmp.size() * sizeof(uint64_t), 1, fp);
      if (rc != 1) {
        return -1;
      }
    }
    rc = pair.second->bitmap.Save(fp, readonly);
    if (0 != rc) {
      return rc;
    }
  }

  return 0;
}
int RobimsWeightSetField::DoLoad(FILE* fp) {
  uint32_t n = 0;
  if (0 != file_read_uint32(fp, n)) {
    return -1;
  }
  for (uint32_t i = 0; i < n; i++) {
    NamedWeightRoaringBitmapPtr p(new NamedWeightRoaringBitmap);
    if (0 != file_read_string(fp, p->name)) {
      return -1;
    }
    uint32_t count = 0;
    if (0 != file_read_uint32(fp, count)) {
      return -1;
    }
    std::vector<uint64_t> tmp(count);
    if (count > 0) {
      int rc = fread((char*)(tmp.data()), tmp.size() * sizeof(uint64_t), 1, fp);
      if (rc != 1) {
        return -1;
      }
      p->weight_ids.insert(tmp.begin(), tmp.end());
      for (uint64_t v : tmp) {
        uint32_t id = v & 0xFFFFFFFFull;
        uint32_t weight = ((v >> 32) & 0xFFFFFFFFull);
        p->id_weights[id] = uint32_to_float(weight);
      }
    }
    if (0 != p->bitmap.Load(fp)) {
      return -1;
    }
    std::string_view name = p->name;
    _bitmaps[name].reset(p.release());
  }
  return 0;
}

int RobimsWeightSetField::Put(uint32_t id, const std::string_view& val, float weight) {
  NamedWeightRoaringBitmap* bitmap = nullptr;
  auto found = _bitmaps.find(val);
  if (found == _bitmaps.end()) {
    bitmap = new NamedWeightRoaringBitmap;
    bitmap->name.assign(val.data(), val.size());
    bitmap->bitmap.NewCRoaringBitmap();
    _bitmaps[bitmap->name].reset(bitmap);
  } else {
    bitmap = found->second.get();
  }
  auto vfound = bitmap->id_weights.find(id);
  if (vfound != bitmap->id_weights.end()) {
    if (weight != vfound->second) {
      uint64_t old_weight_id = float_to_uint32(vfound->second);
      old_weight_id = (old_weight_id << 32) + id;
      bitmap->weight_ids.erase(old_weight_id);
    }
  }
  bitmap->id_weights[id] = weight;
  uint64_t new_weight_id = float_to_uint32(weight);
  new_weight_id = (new_weight_id << 32) + id;
  bitmap->weight_ids.insert(new_weight_id);
  bitmap->bitmap.Put(id);
  if (GetFieldMeta().topk_limit() > 0 && bitmap->id_weights.size() > GetFieldMeta().topk_limit()) {
    uint64_t min_weight_id = *(bitmap->weight_ids.rbegin());
    uint32_t remove_id = min_weight_id & 0xFFFFFFFFull;
    bitmap->id_weights.erase(remove_id);
    bitmap->weight_ids.erase(min_weight_id);
  }
  return 0;
}
int RobimsWeightSetField::Remove(uint32_t id) {
  for (auto& pair : _bitmaps) {
    auto found = pair.second->id_weights.find(id);
    if (found == pair.second->id_weights.end()) {
      return -1;
    }
    float weight = found->second;
    uint64_t weight_id = float_to_uint32(weight);
    weight_id = (weight_id << 32) + id;
    pair.second->id_weights.erase(found);
    pair.second->weight_ids.erase(weight_id);
    pair.second->bitmap.Remove(id);
  }
  return 0;
}
int RobimsWeightSetField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
  switch (op) {
    case FIELD_OP_ALL: {
      out.reset(acquire_bitmap());
      roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
      break;
    }
    case FIELD_OP_EQ:
    case FIELD_OP_NEQ: {
      std::string_view* str = std::get_if<std::string_view>(&arg);
      if (nullptr == str) {
        ROBIMS_ERROR("RobimsSetField does NOT support  args with data type which is not string:{}!",
                     arg.index());
        return ROBIMS_ERR_INVALID_ARGS;
      }
      auto found = _bitmaps.find(*str);
      if (found == _bitmaps.end()) {
        return ROBIMS_ERR_NOTFOUND;
      }
      out.reset(acquire_bitmap());
      if (FIELD_OP_EQ == op) {
        roaring_bitmap_overwrite(out.get(), found->second->bitmap.bitmap.get());
      } else {
        roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
        roaring_bitmap_andnot_inplace(out.get(), found->second->bitmap.bitmap.get());
      }
      break;
    }
    default: {
      ROBIMS_ERROR("Unsupoorted operator:{}!", op);
      return ROBIMS_ERR_INVALID_OPERATOR;
    }
  }
  return 0;
}

// int RobimsWeightSetField::Visit(const roaring_bitmap_t* b, const VisitOptions& options) {
//   const std::string_view* str = std::get_if<std::string_view>(&options.field_arg);
//   if (nullptr == str) {
//     ROBIMS_ERROR("RobimsSetField does NOT support  args with data type which is not string:{}!",
//                  options.field_arg.index());
//     return ROBIMS_ERR_INVALID_ARGS;
//   }
//   auto found = _bitmaps.find(*str);
//   if (found == _bitmaps.end()) {
//     return ROBIMS_ERR_NOTFOUND;
//   }
//   uint32_t count = 0;
//   for (uint64_t weight_id : found->second->weight_ids) {
//     uint32_t id = weight_id & 0xFFFFFFFFull;
//     if (!roaring_bitmap_contains(b, id)) {
//       continue;
//     }
//     uint32_t int_weight = (weight_id >> 32) & 0xFFFFFFFFull;
//     float weight = uint32_to_float(int_weight);
//     if (options.filter) {
//       if (options.filter(id)) {
//         continue;
//       }
//     }
//     if (options.visit) {
//       if (!options.visit(id, weight)) {
//         break;
//       }
//     }
//     count++;
//     if (options.topk > 0 && count >= options.topk) {
//       break;
//     }
//   }
//   return 0;
// }
}  // namespace robims

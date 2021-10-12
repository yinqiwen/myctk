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
#include <robims_common.h>
#include <string_view>
#include "robims_err.h"
#include "robims_field.h"
#include "robims_log.h"
#include "robims_table.h"
namespace robims {
int RobimsSetField::OnInit() {
  // _bsi.reset(new BitSliceIntIndex);
  // _bsi->Init(GetFieldMeta());
  return 0;
}
int RobimsSetField::DoSave(FILE* fp, bool readonly) {
  int rc = file_write_uint32(fp, _bitmaps.size());
  if (0 != rc) {
    return rc;
  }
  for (auto& pair : _bitmaps) {
    rc = file_write_string(fp, pair.second->name);
    if (0 != rc) {
      return rc;
    }
    rc = pair.second->bitmap.Save(fp, readonly);
    if (0 != rc) {
      return rc;
    }
  }

  return 0;
}
int RobimsSetField::DoLoad(FILE* fp) {
  uint32_t n = 0;
  if (0 != file_read_uint32(fp, n)) {
    return -1;
  }
  for (uint32_t i = 0; i < n; i++) {
    NamedRoaringBitmapPtr p(new NamedRoaringBitmap);
    if (0 != file_read_string(fp, p->name)) {
      return -1;
    }
    if (0 != p->bitmap.Load(fp)) {
      return -1;
    }
    std::string_view name = p->name;
    _bitmaps[name].reset(p.release());
  }
  return 0;
}
int RobimsSetField::Put(uint32_t id, const std::string_view& val) {
  NamedRoaringBitmap* bitmap = nullptr;
  auto found = _bitmaps.find(val);
  if (found == _bitmaps.end()) {
    bitmap = new NamedRoaringBitmap;
    bitmap->name.assign(val.data(), val.size());
    bitmap->bitmap.NewCRoaringBitmap();
    _bitmaps[bitmap->name].reset(bitmap);
  } else {
    bitmap = found->second.get();
  }
  roaring_bitmap_add(bitmap->bitmap.bitmap.get(), id);
  return 0;
}
int RobimsSetField::Remove(uint32_t id) {
  for (auto& pair : _bitmaps) {
    pair.second->bitmap.Remove(id);
  }
  return 0;
}
int RobimsSetField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
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
      auto& matched_bitmap = found->second->bitmap;
      out.reset(acquire_bitmap());
      if (FIELD_OP_EQ == op) {
        roaring_bitmap_overwrite(out.get(), matched_bitmap.bitmap.get());
      } else {
        roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
        roaring_bitmap_andnot_inplace(out.get(), matched_bitmap.bitmap.get());
      }
      ROBIMS_DEBUG("RobimsSetField EQ return siz={}", roaring_bitmap_get_cardinality(out.get()));
      break;
    }
    default: {
      ROBIMS_ERROR("Unsupoorted operator:{}!", op);
      return ROBIMS_ERR_INVALID_OPERATOR;
    }
  }
  return 0;
}
}  // namespace robims

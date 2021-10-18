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
int RobimsBoolField::OnInit() {
  _bitmap.NewCRoaringBitmap();
  return 0;
}
int RobimsBoolField::DoSave(FILE* fp, bool readonly) { return _bitmap.Save(fp, readonly); }
int RobimsBoolField::DoLoad(FILE* fp) { return _bitmap.Load(fp); }
int RobimsBoolField::Put(uint32_t id, int64_t val) {
  // ROBIMS_DEBUG("Bool put val:{}", val);
  _bitmap.Remove(id);
  if (val == 1) {
    _bitmap.Put(id);
  }
  return 0;
}
int RobimsBoolField::Remove(uint32_t id) {
  _bitmap.Remove(id);
  return 0;
}
int RobimsBoolField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
  switch (op) {
    case FIELD_OP_ALL: {
      out.reset(acquire_bitmap());
      roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
      break;
    }
    case FIELD_OP_EQ:
    case FIELD_OP_NEQ: {
      int64_t* iv = std::get_if<std::int64_t>(&arg);
      if (nullptr == iv) {
        ROBIMS_ERROR("RobimsBoolField does NOT support  args with data type which is not int:{}!",
                     arg.index());
        return ROBIMS_ERR_INVALID_ARGS;
      }
      out.reset(acquire_bitmap());
      // ROBIMS_DEBUG("RobimsBoolField EQ before size={}",
      //            roaring_bitmap_get_cardinality(_bitmap.bitmap.get()));
      if (FIELD_OP_EQ == op) {
        if (*iv == 0) {
          roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
          roaring_bitmap_andnot_inplace(out.get(), _bitmap.bitmap.get());
        } else {
          roaring_bitmap_overwrite(out.get(), _bitmap.bitmap.get());
        }
      } else {
        if (*iv == 0) {
          roaring_bitmap_overwrite(out.get(), _bitmap.bitmap.get());
        } else {
          roaring_bitmap_overwrite(out.get(), _table->GetTableBitmap().bitmap.get());
          roaring_bitmap_andnot_inplace(out.get(), _bitmap.bitmap.get());
        }
      }
      // ROBIMS_DEBUG("RobimsBoolField EQ return size={}",
      // roaring_bitmap_get_cardinality(out.get()));
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

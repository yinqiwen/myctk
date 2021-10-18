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
#include <robims_db.h>
#include "robims_bsi.h"
#include "robims_err.h"
#include "robims_field.h"
#include "robims_log.h"
namespace robims {
int RobimsIntField::OnInit() {
  _bsi.reset(new BitSliceIntIndex);
  _bsi->Init(GetFieldMeta());
  return 0;
}
int RobimsIntField::DoSave(FILE* fp, bool readonly) { return _bsi->Save(fp, readonly); }
int RobimsIntField::DoLoad(FILE* fp) { return _bsi->Load(fp); }
int RobimsIntField::Put(uint32_t id, int64_t val) {
  _bsi->Put(id, val);
  return 0;
}
int RobimsIntField::Remove(uint32_t id) {
  int64_t val;
  if (0 == _bsi->Get(id, val)) {
    _bsi->Remove(id, val);
  }
  return 0;
}
int RobimsIntField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
  int64_t* iv = std::get_if<int64_t>(&arg);
  if (nullptr == iv) {
    ROBIMS_ERROR("RobimsIntField does NOT support  args with data type which is not int:{}!",
                 arg.index());
    return ROBIMS_ERR_INVALID_ARGS;
  }
  out.reset(acquire_bitmap());
  switch (op) {
    case FIELD_OP_EQ: {
      _bsi->RangeEQ(*iv, out.get());
      break;
    }
    case FIELD_OP_NEQ: {
      _bsi->RangeNEQ(*iv, out.get());
      break;
    }
    case FIELD_OP_LT: {
      _bsi->RangeLT(*iv, false, out.get());
      break;
    }
    case FIELD_OP_LTE: {
      _bsi->RangeLT(*iv, true, out.get());
      break;
    }
    case FIELD_OP_GT: {
      _bsi->RangeGT(*iv, false, out.get());
      break;
    }
    case FIELD_OP_GTE: {
      _bsi->RangeGT(*iv, true, out.get());
      break;
    }
    default: {
      ROBIMS_ERROR("Unsupoorted operator:{}!", op);
      return ROBIMS_ERR_INVALID_OPERATOR;
    }
  }
  ROBIMS_DEBUG("Int return siz={}", roaring_bitmap_get_cardinality(out.get()));
  return 0;
}

int RobimsFloatField::OnInit() {
  _bsi.reset(new BitSliceFloatIndex);
  _bsi->Init(GetFieldMeta());
  return 0;
}
int RobimsFloatField::DoSave(FILE* fp, bool readonly) { return _bsi->Save(fp, readonly); }
int RobimsFloatField::DoLoad(FILE* fp) { return _bsi->Load(fp); }
int RobimsFloatField::Put(uint32_t id, float val) {
  float old_val;
  _bsi->Put(id, val, old_val);
  return 0;
}
int RobimsFloatField::Remove(uint32_t id) {
  float val;
  if (0 == _bsi->Get(id, val)) {
    _bsi->Remove(id, val);
  }
  return 0;
}
int RobimsFloatField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
  float fv = 0;
  double* dv = std::get_if<double>(&arg);
  if (nullptr != dv) {
    fv = *dv;
  } else {
    int64_t* iv = std::get_if<int64_t>(&arg);
    if (nullptr == iv) {
      ROBIMS_ERROR(
          "RobimsFloatField does NOT support args with data type which is not float/int:{}!",
          arg.index());
      return ROBIMS_ERR_INVALID_ARGS;
    }
    fv = *iv;
  }
  out.reset(acquire_bitmap());
  switch (op) {
    case FIELD_OP_EQ: {
      _bsi->RangeEQ(fv, out.get());
      break;
    }
    case FIELD_OP_NEQ: {
      _bsi->RangeNEQ(fv, out.get());
      break;
    }
    case FIELD_OP_LT: {
      _bsi->RangeLT(fv, false, out.get());
      break;
    }
    case FIELD_OP_LTE: {
      _bsi->RangeLT(fv, true, out.get());
      break;
    }
    case FIELD_OP_GT: {
      _bsi->RangeGT(fv, false, out.get());
      break;
    }
    case FIELD_OP_GTE: {
      _bsi->RangeGT(fv, true, out.get());
      break;
    }
    default: {
      ROBIMS_ERROR("Unsupoorted operator:{}!", op);
      return ROBIMS_ERR_INVALID_OPERATOR;
    }
  }
  // ROBIMS_ERROR("Float return siz={}", roaring_bitmap_get_cardinality(out.get()));
  return 0;
}
}  // namespace robims

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
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "folly/container/F14Map.h"
#include "robims.pb.h"
#include "robims_bsi.h"
#include "robims_db.h"

namespace robims {
typedef std::variant<std::string_view, int64_t, double> FieldArg;
typedef std::pair<uint32_t, float> IDWeight;

class RobimsTable;
class RobimsField {
 protected:
  RobimsField(RobimsField&) = delete;
  RobimsField& operator=(const RobimsField&) = delete;

  FieldMeta _meta;

  RobimsTable* _table = nullptr;

  virtual int OnInit() = 0;

 public:
  RobimsField() = default;
  int Init(RobimsTable* table, const FieldMeta& meta);
  const FieldMeta& GetFieldMeta() { return _meta; }
  int Save(FILE* fp, bool readonly);
  int Load(FILE* fp);
  virtual int DoLoad(FILE* fp);
  virtual int DoSave(FILE* fp, bool readonly);
  virtual int Put(uint32_t id, const std::string_view& val);
  virtual int Put(uint32_t id, int64_t val);
  virtual int Put(uint32_t id, float val);
  virtual int Put(uint32_t id);
  virtual int Put(uint32_t id, const std::string_view& val, float weight);
  virtual int Remove(uint32_t id);
  virtual int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out);

  virtual ~RobimsField() {}
};

class RobimsBoolField : public RobimsField {
 private:
  RoaringBitmap _bitmap;
  int OnInit() override;
  int DoSave(FILE* fp, bool readonly) override;
  int DoLoad(FILE* fp) override;
  int Put(uint32_t id, int64_t val) override;
  int Remove(uint32_t id) override;
  int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) override;
};

class RobimsIntField : public RobimsField {
 private:
  BitSliceIntIndexPtr _bsi;
  int OnInit() override;
  int DoSave(FILE* fp, bool readonly) override;
  int DoLoad(FILE* fp) override;
  int Put(uint32_t id, int64_t val) override;
  int Remove(uint32_t id) override;
  int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) override;
};
class RobimsFloatField : public RobimsField {
 private:
  BitSliceFloatIndexPtr _bsi;
  int OnInit() override;
  int DoSave(FILE* fp, bool readonly) override;
  int DoLoad(FILE* fp) override;
  int Put(uint32_t id, float val) override;
  int Remove(uint32_t id) override;
  int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) override;
};

struct NamedRoaringBitmap {
  std::string name;
  RoaringBitmap bitmap;
};
typedef std::unique_ptr<NamedRoaringBitmap> NamedRoaringBitmapPtr;
class RobimsSetField : public RobimsField {
 private:
  typedef folly::F14FastMap<std::string_view, NamedRoaringBitmapPtr> NamedRoaringBitmapTable;
  NamedRoaringBitmapTable _bitmaps;
  int OnInit() override;
  int DoSave(FILE* fp, bool readonly) override;
  int DoLoad(FILE* fp) override;
  int Put(uint32_t id, const std::string_view& val) override;
  int Remove(uint32_t id) override;
  int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) override;
};

struct NamedWeightRoaringBitmap {
  std::string name;
  absl::btree_set<uint64_t, std::greater<uint64_t>> weight_ids;
  absl::btree_map<uint32_t, float> id_weights;
  RoaringBitmap bitmap;
};

typedef std::unique_ptr<NamedWeightRoaringBitmap> NamedWeightRoaringBitmapPtr;

class RobimsWeightSetField : public RobimsField {
 private:
  typedef folly::F14FastMap<std::string_view, NamedWeightRoaringBitmapPtr>
      NamedWeightRoaringBitmapTable;
  NamedWeightRoaringBitmapTable _bitmaps;
  int DoSave(FILE* fp, bool readonly) override;
  int DoLoad(FILE* fp) override;

  int OnInit() override;
  int Put(uint32_t id, const std::string_view& val, float weight) override;
  int Remove(uint32_t id) override;
  int Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) override;
  // int Visit(const roaring_bitmap_t* b, const VisitOptions& options) override;
};

class RobimsFieldBuilder {
 public:
  static RobimsField* Build(RobimsTable* table, const FieldMeta& meta);
};

}  // namespace robims
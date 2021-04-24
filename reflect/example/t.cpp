#include "data.pb.h"
#include "data_generated.h"
#include "expr_struct_helper.h"

using namespace expr_struct;
namespace test {
namespace expr_struct {
template <>
template <typename fake>
int ExprStructHelper<SubData>::InitExpr() {
  static bool once = false;
  if (once) {
    return 0;
  }
  once = true;
  SubData* __root_obj = nullptr;
  using PB_TYPE = SubData;
  using PB_HELPER_TYPE = ExprStructHelper<SubData>;
  {
    using FIELD_TYPE = decltype(__root_obj->id());
    expr_struct::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();
    if constexpr (std::is_same<FIELD_TYPE, double>::value ||
                  std::is_same<FIELD_TYPE, float>::value || std::is_same<FIELD_TYPE, char>::value ||
                  std::is_same<FIELD_TYPE, uint8_t>::value ||
                  std::is_same<FIELD_TYPE, int32_t>::value ||
                  std::is_same<FIELD_TYPE, uint32_t>::value ||
                  std::is_same<FIELD_TYPE, int16_t>::value ||
                  std::is_same<FIELD_TYPE, uint16_t>::value ||
                  std::is_same<FIELD_TYPE, int64_t>::value ||
                  std::is_same<FIELD_TYPE, uint64_t>::value ||
                  std::is_same<FIELD_TYPE, bool>::value ||
                  std::is_same<FIELD_TYPE, const std::string&>::value) {
      accessors["id"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        return data->id();
      };
    } else if constexpr (std::is_same<FIELD_TYPE, const flatbuffers::String*>::value) {
      accessors["id"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        const flatbuffers::String* sv = data->id();
        std::string_view rv(sv->c_str(), sv->size());
        return rv;
      };
    } else {
      using R = typename std::remove_reference<FIELD_TYPE>::type;
      using NR = typename std::remove_pointer<R>::type;
      using RR = typename std::remove_const<NR>::type;
      expr_struct::ExprStructHelper<RR>::InitExpr();
      expr_struct::FieldAccessorTable value =
          expr_struct::ExprStructHelper<RR>::GetFieldAccessorTable();
      if constexpr (std::is_pointer<FIELD_TYPE>::value) {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->id();
          return fv;
        };
        accessors["id"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      } else {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->id();
          return &fv;
        };
        accessors["id"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      }
    }
  }
  {
    using FIELD_TYPE = decltype(__root_obj->iid());
    expr_struct::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();
    if constexpr (std::is_same<FIELD_TYPE, double>::value ||
                  std::is_same<FIELD_TYPE, float>::value || std::is_same<FIELD_TYPE, char>::value ||
                  std::is_same<FIELD_TYPE, uint8_t>::value ||
                  std::is_same<FIELD_TYPE, int32_t>::value ||
                  std::is_same<FIELD_TYPE, uint32_t>::value ||
                  std::is_same<FIELD_TYPE, int16_t>::value ||
                  std::is_same<FIELD_TYPE, uint16_t>::value ||
                  std::is_same<FIELD_TYPE, int64_t>::value ||
                  std::is_same<FIELD_TYPE, uint64_t>::value ||
                  std::is_same<FIELD_TYPE, bool>::value ||
                  std::is_same<FIELD_TYPE, const std::string&>::value) {
      accessors["iid"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        return data->iid();
      };
    } else if constexpr (std::is_same<FIELD_TYPE, const flatbuffers::String*>::value) {
      accessors["iid"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        const flatbuffers::String* sv = data->iid();
        std::string_view rv(sv->c_str(), sv->size());
        return rv;
      };
    } else {
      using R = typename std::remove_reference<FIELD_TYPE>::type;
      using NR = typename std::remove_pointer<R>::type;
      using RR = typename std::remove_const<NR>::type;
      expr_struct::ExprStructHelper<RR>::InitExpr();
      expr_struct::FieldAccessorTable value =
          expr_struct::ExprStructHelper<RR>::GetFieldAccessorTable();
      if constexpr (std::is_pointer<FIELD_TYPE>::value) {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->iid();
          return fv;
        };
        accessors["iid"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      } else {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->iid();
          return &fv;
        };
        accessors["iid"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      }
    }
  }
  {
    using FIELD_TYPE = decltype(__root_obj->name());
    expr_struct::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();
    if constexpr (std::is_same<FIELD_TYPE, double>::value ||
                  std::is_same<FIELD_TYPE, float>::value || std::is_same<FIELD_TYPE, char>::value ||
                  std::is_same<FIELD_TYPE, uint8_t>::value ||
                  std::is_same<FIELD_TYPE, int32_t>::value ||
                  std::is_same<FIELD_TYPE, uint32_t>::value ||
                  std::is_same<FIELD_TYPE, int16_t>::value ||
                  std::is_same<FIELD_TYPE, uint16_t>::value ||
                  std::is_same<FIELD_TYPE, int64_t>::value ||
                  std::is_same<FIELD_TYPE, uint64_t>::value ||
                  std::is_same<FIELD_TYPE, bool>::value ||
                  std::is_same<FIELD_TYPE, const std::string&>::value) {
      accessors["name"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        return data->name();
      };
    } else if constexpr (std::is_same<FIELD_TYPE, const flatbuffers::String*>::value) {
      accessors["name"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        const flatbuffers::String* sv = data->name();
        std::string_view rv(sv->c_str(), sv->size());
        return rv;
      };
    } else {
      using R = typename std::remove_reference<FIELD_TYPE>::type;
      using NR = typename std::remove_pointer<R>::type;
      using RR = typename std::remove_const<NR>::type;
      expr_struct::ExprStructHelper<RR>::InitExpr();
      expr_struct::FieldAccessorTable value =
          expr_struct::ExprStructHelper<RR>::GetFieldAccessorTable();
      if constexpr (std::is_pointer<FIELD_TYPE>::value) {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->name();
          return fv;
        };
        accessors["name"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      } else {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->name();
          return &fv;
        };
        accessors["name"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      }
    }
  }
  {
    using FIELD_TYPE = decltype(__root_obj->score());
    expr_struct::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();
    if constexpr (std::is_same<FIELD_TYPE, double>::value ||
                  std::is_same<FIELD_TYPE, float>::value || std::is_same<FIELD_TYPE, char>::value ||
                  std::is_same<FIELD_TYPE, uint8_t>::value ||
                  std::is_same<FIELD_TYPE, int32_t>::value ||
                  std::is_same<FIELD_TYPE, uint32_t>::value ||
                  std::is_same<FIELD_TYPE, int16_t>::value ||
                  std::is_same<FIELD_TYPE, uint16_t>::value ||
                  std::is_same<FIELD_TYPE, int64_t>::value ||
                  std::is_same<FIELD_TYPE, uint64_t>::value ||
                  std::is_same<FIELD_TYPE, bool>::value ||
                  std::is_same<FIELD_TYPE, const std::string&>::value) {
      accessors["score"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        return data->score();
      };
    } else if constexpr (std::is_same<FIELD_TYPE, const flatbuffers::String*>::value) {
      accessors["score"] = [](const void* v) -> expr_struct::FieldValue {
        const PB_TYPE* data = (const PB_TYPE*)v;
        const flatbuffers::String* sv = data->score();
        std::string_view rv(sv->c_str(), sv->size());
        return rv;
      };
    } else {
      using R = typename std::remove_reference<FIELD_TYPE>::type;
      using NR = typename std::remove_pointer<R>::type;
      using RR = typename std::remove_const<NR>::type;
      expr_struct::ExprStructHelper<RR>::InitExpr();
      expr_struct::FieldAccessorTable value =
          expr_struct::ExprStructHelper<RR>::GetFieldAccessorTable();
      if constexpr (std::is_pointer<FIELD_TYPE>::value) {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->score();
          return fv;
        };
        accessors["score"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      } else {
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue {
          const PB_TYPE* data = (const PB_TYPE*)v;
          FIELD_TYPE fv = data->score();
          return &fv;
        };
        accessors["score"] = std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(
            field_accessor, value);
      }
    }
  }
  return 0;
}
}  // namespace expr_struct

}  // namespace test
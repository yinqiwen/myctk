/*
 *Copyright (c) 2017-2018, yinqiwen <yinqiwen@gmail.com>
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
 *  * Neither the name of Redis nor the names of its contributors may be used
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

#ifndef SRC_COMMON_TYPES_HPP_
#define SRC_COMMON_TYPES_HPP_
#include <flatbuffers/idl.h>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/list.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "allocator.hpp"
#include "cmod_robin_hood.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

//#include "cmod_util.hpp"

namespace cmod {
typedef Allocator<char> CharAllocator;
typedef boost::interprocess::offset_ptr<void> VoidPtr;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, Allocator<char>> SHMString;

flatbuffers::Parser *GetFlatBufferParser(const char *name);

template <typename T>
struct SHMVector {
  typedef boost::interprocess::vector<T, Allocator<T>> Type;
};

template <typename T>
struct SHMDeque {
  typedef boost::interprocess::deque<T, Allocator<T>> Type;
};

template <typename T>
struct SHMList {
  typedef boost::interprocess::list<T, Allocator<T>> Type;
};

template <typename T, typename Less = std::less<T>>
struct SHMSet {
  typedef boost::interprocess::set<T, Less, Allocator<T>> Type;
};

template <typename KeyType, typename MappedType, typename Hash = boost::hash<KeyType>,
          typename Equal = std::equal_to<KeyType>>
struct SHMHashMap {
  typedef std::pair<const KeyType, MappedType> ValueType;
  typedef Allocator<ValueType> ShmemAllocator;
  typedef boost::unordered_map<KeyType, MappedType, Hash, Equal, ShmemAllocator> Type;
};

template <typename KeyType, typename MappedType, typename Less = std::less<KeyType>>
struct SHMMap {
  typedef std::pair<const KeyType, MappedType> ValueType;
  typedef Allocator<ValueType> ShmemAllocator;
  typedef boost::interprocess::map<KeyType, MappedType, Less, ShmemAllocator> Type;
};

template <typename KeyType, typename Hash = boost::hash<KeyType>,
          typename Equal = std::equal_to<KeyType>>
struct SHMHashSet {
  typedef boost::unordered_set<KeyType, Hash, Equal, Allocator<KeyType>> Type;
};

typedef SHMHashMap<SHMString, VoidPtr>::Type NamingTable;

struct Base64Value : public SHMString {
  void Copy(const Base64Value &src);
  bool PraseFromJson(const rapidjson::Value &json);
  void WriteToJson(rapidjson::Value &json, rapidjson::Value::AllocatorType &allocator) const;
};

template <typename T>
struct FlatBuffersValue : public SHMString {
  FlatBuffersValue(const CharAllocator &alloc) : SHMString(alloc) {}

  void Copy(const FlatBuffersValue<T> &src) { assign(src.data(), src.size()); }

  bool PraseFromJson(const rapidjson::Value &json) {
    const char *schema = T::schema;
    const rapidjson::Value *val = &json;
    flatbuffers::Parser *p = GetFlatBufferParser(schema);
    if (nullptr == p) {
      return false;
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    val->Accept(writer);
    std::string tmp;
    tmp.assign(buffer.GetString(), buffer.GetSize());
    if (!p->Parse(tmp.c_str())) {
      delete p;
      return false;
    }
    flatbuffers::uoffset_t length = p->builder_.GetSize();
    uint8_t *bufferPointer = p->builder_.GetBufferPointer();
    SHMString::assign((const char *)bufferPointer, length);
    delete p;
    return true;
  }
  void WriteToJson(rapidjson::Value &json, rapidjson::Value::AllocatorType &allocator) const {
    const char *schema = T::schema;
    flatbuffers::Parser *p = GetFlatBufferParser(schema);
    if (nullptr == p) {
      return;
    }
    p->opts.strict_json = true;
    std::string tmp;
    flatbuffers::GenerateText(*p, data(), &tmp);
    delete p;
    if (!tmp.empty()) {
      rapidjson::Document d;
      d.Parse<0>(tmp.c_str());
      if (d.HasParseError()) {
        return;
      }
      json.CopyFrom(d, allocator);
    }
  }
};

template <typename T>
struct SHMConstructor {
  T value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(alloc) {}
};
template <>
struct SHMConstructor<float> {
  float value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<double> {
  double value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};

template <>
struct SHMConstructor<int8_t> {
  int8_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<uint8_t> {
  uint8_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<int16_t> {
  int16_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<uint16_t> {
  uint16_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<uint32_t> {
  uint32_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<int32_t> {
  int32_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<int64_t> {
  int64_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};
template <>
struct SHMConstructor<uint64_t> {
  uint64_t value;
  SHMConstructor(const cmod::CharAllocator &alloc) : value(0) {}
};

template <typename T>
inline void StructCopy(T &dst, const T &src) {
  dst.Copy(src);
}

template <>
inline void StructCopy(double &dst, const double &src) {
  dst = src;
}

template <>
inline void StructCopy(float &dst, const float &src) {
  dst = src;
}
template <>
inline void StructCopy(uint64_t &dst, const uint64_t &src) {
  dst = src;
}
template <>
inline void StructCopy(int64_t &dst, const int64_t &src) {
  dst = src;
}
template <>
inline void StructCopy(uint32_t &dst, const uint32_t &src) {
  dst = src;
}
template <>
inline void StructCopy(int32_t &dst, const int32_t &src) {
  dst = src;
}
template <>
inline void StructCopy(uint16_t &dst, const uint16_t &src) {
  dst = src;
}
template <>
inline void StructCopy(int16_t &dst, const int16_t &src) {
  dst = src;
}
template <>
inline void StructCopy(uint8_t &dst, const uint8_t &src) {
  dst = src;
}
template <>
inline void StructCopy(int8_t &dst, const int8_t &src) {
  dst = src;
}
template <>
inline void StructCopy(char &dst, const char &src) {
  dst = src;
}
template <>
inline void StructCopy(bool &dst, const bool &src) {
  dst = src;
}
template <>
inline void StructCopy(cmod::SHMString &dst, const cmod::SHMString &src) {
  dst.assign(src.data(), src.size());
}

template <typename K, typename V>
inline void StructCopy(boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>,
                                            cmod::Allocator<std::pair<const K, V>>> &dst,
                       const boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>,
                                                  cmod::Allocator<std::pair<const K, V>>> &src) {
  typedef boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>,
                               cmod::Allocator<std::pair<const K, V>>>
      LocalMapType;
  typename LocalMapType::const_iterator it = src.begin();
  while (it != src.end()) {
    const K &kk = it->first;
    const V &vv = it->second;
    SHMConstructor<K> key(dst.get_allocator());
    SHMConstructor<V> value(dst.get_allocator());
    StructCopy(key.value, kk);
    StructCopy(value.value, vv);
    dst.insert(typename LocalMapType::value_type(key.value, value.value));
    it++;
  }
}

template <typename K, typename V>
inline void StructCopy(
    boost::container::map<K, V, std::less<K>, cmod::Allocator<std::pair<const K, V>>> &dst,
    const boost::container::map<K, V, std::less<K>, cmod::Allocator<std::pair<const K, V>>> &src) {
  typedef boost::container::map<K, V, std::less<K>, cmod::Allocator<std::pair<const K, V>>>
      LocalMapType;
  typename LocalMapType::const_iterator it = src.begin();
  while (it != src.end()) {
    const K &kk = it->first;
    const V &vv = it->second;
    SHMConstructor<K> key(dst.get_allocator());
    SHMConstructor<V> value(dst.get_allocator());
    StructCopy(key.value, kk);
    StructCopy(value.value, vv);
    dst.insert(typename LocalMapType::value_type(key.value, value.value));
    it++;
  }
}
template <typename K, typename V>
inline void StructCopy(cmod_robin_hood::unordered_flat_map<K, V> &dst,
                       const cmod_robin_hood::unordered_flat_map<K, V> &src) {
  typedef cmod_robin_hood::unordered_flat_map<K, V> LocalMapType;
  typename LocalMapType::const_iterator it = src.begin();
  while (it != src.end()) {
    const K &kk = it->first;
    const V &vv = it->second;
    SHMConstructor<K> key(dst.get_allocator());
    SHMConstructor<V> value(dst.get_allocator());
    StructCopy(key.value, kk);
    StructCopy(value.value, vv);
    dst.insert(typename LocalMapType::value_type(key.value, value.value));
    it++;
  }
}

template <typename K, typename V>
inline void StructCopy(cmod_robin_hood::unordered_node_map<K, V> &dst,
                       const cmod_robin_hood::unordered_node_map<K, V> &src) {
  typedef cmod_robin_hood::unordered_node_map<K, V> LocalMapType;
  typename LocalMapType::const_iterator it = src.begin();
  while (it != src.end()) {
    const K &kk = it->first;
    const V &vv = it->second;
    SHMConstructor<K> key(dst.get_allocator());
    SHMConstructor<V> value(dst.get_allocator());
    StructCopy(key.value, kk);
    StructCopy(value.value, vv);
    dst.insert(typename LocalMapType::value_type(key.value, value.value));
    it++;
  }
}

template <typename K>
inline void StructCopy(boost::container::vector<K, cmod::Allocator<K>> &dst,
                       const boost::container::vector<K, cmod::Allocator<K>> &src) {
  dst.resize(src.size());
  for (size_t i = 0; i < src.size(); i++) {
    StructCopy(dst[i], src[i]);
  }
}

}  // namespace cmod

#endif /* SRC_COMMON_TYPES_HPP_ */

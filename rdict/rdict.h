/*
** BSD 3-Clause License
**
** Copyright (c) 2023, qiyingwang <qiyingwang@tencent.com>, the respective contributors, as shown by the AUTHORS file.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
** * Redistributions of source code must retain the above copyright notice, this
** list of conditions and the following disclaimer.
**
** * Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** * Neither the name of the copyright holder nor the names of its
** contributors may be used to endorse or promote products derived from
** this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/Likely.h"
#include "rdict/mmap_file.h"
#include "rdict/rdict.h"

namespace rdict {

namespace detail {
struct nonesuch {};

template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
struct detector {
  using value_t = std::false_type;
  using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
  using value_t = std::true_type;
  using type = Op<Args...>;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template <typename T>
using detect_avalanching = typename T::is_avalanching;
template <typename T>
struct Serializer {
  static void Pack(const T& v, std::vector<uint8_t>& buffer) {
    size_t orig_len = buffer.size();
    buffer.resize(buffer.size() + sizeof(T));
    memcpy(&buffer[0] + orig_len, &v, sizeof(T));
  }
  static size_t Unpack(const uint8_t* data, T& v) {
    memcpy(&v, data, sizeof(T));
    return sizeof(T);
  }
  static size_t GetSize(const uint8_t* data) { return sizeof(T); }
};

template <>
struct Serializer<std::string_view> {
  static void Pack(const std::string_view& v, std::vector<uint8_t>& buffer) {
    uint32_t v_len = static_cast<uint32_t>(v.size());
    size_t orig_len = buffer.size();
    buffer.resize(buffer.size() + sizeof(uint32_t) + v.size());
    memcpy(&buffer[0] + orig_len, &v_len, sizeof(uint32_t));
    memcpy(&buffer[0] + orig_len + sizeof(uint32_t), v.data(), v.size());
  }
  static size_t Unpack(const uint8_t* data, std::string_view& v) {
    uint32_t len;
    memcpy(&len, data, sizeof(uint32_t));
    std::string_view view(reinterpret_cast<const char*>(data + sizeof(uint32_t)), len);
    v = view;
    return sizeof(uint32_t) + v.size();
  }
  static size_t GetSize(const uint8_t* data) {
    uint32_t len;
    memcpy(&len, data, sizeof(uint32_t));
    return sizeof(uint32_t) + len;
  }
};

struct KeyValFlags {
  uint8_t invalid : 1;
  uint8_t reserved : 7;
  KeyValFlags() : invalid(0), reserved(0) {}
};

template <typename KeyType, typename ValueType>
struct Bucket {
  static constexpr uint32_t k_dist_inc = 1U << 8U;                // skip 1 byte fingerprint
  static constexpr uint32_t k_fingerprint_mask = k_dist_inc - 1;  // mask for 1 byte of fingerprint
  static constexpr bool is_flat = false;
  uint64_t value_idx;             // index into the m_values vector.
  uint32_t dist_and_fingerprint;  // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
};

#define RDICT_BUCKET_PRIMITIVE(K, V)                               \
  template <>                                                      \
  struct Bucket<K, V> {                                            \
    static constexpr uint32_t k_dist_inc = 1U << 8U;               \
    static constexpr uint32_t k_fingerprint_mask = k_dist_inc - 1; \
    static constexpr bool is_flat = true;                          \
    K key;                                                         \
    V val;                                                         \
    uint32_t dist_and_fingerprint;                                 \
  }

RDICT_BUCKET_PRIMITIVE(uint32_t, uint64_t);
RDICT_BUCKET_PRIMITIVE(uint32_t, uint32_t);
RDICT_BUCKET_PRIMITIVE(uint64_t, uint64_t);
RDICT_BUCKET_PRIMITIVE(uint64_t, uint32_t);

template <typename K, typename V>
struct KeyValPair {
  static std::vector<uint8_t> Pack(const K& k, const V& v) {
    std::vector<uint8_t> buffer;
    Serializer<K>::Pack(k, buffer);
    Serializer<V>::Pack(v, buffer);
    // 1byte flags
    size_t allign_len = (buffer.size() + 1 + 7) & ~7;
    if (buffer.size() < allign_len) {
      buffer.resize(allign_len);
    }
    return buffer;
  }
  static K UnpackKey(const uint8_t* data) {
    K k;
    Serializer<K>::Unpack(data, k);
    return k;
  }
  static size_t Unpack(const uint8_t* data, K& k, V& v) {
    size_t n1 = Serializer<K>::Unpack(data, k);
    size_t n2 = Serializer<V>::Unpack(data + n1, v);
    return n1 + n2;
  }
  static size_t GetKeyValuePackSize(const uint8_t* data) {
    size_t n1 = Serializer<K>::GetSize(data);
    size_t n2 = Serializer<V>::GetSize(data + n1);
    // 1byte flags
    size_t len = n1 + n2 + 1;
    size_t allign_len = (len + 7) & ~7;
    return allign_len;
  }
  static KeyValFlags GetFlags(const uint8_t* data) {
    size_t n1 = Serializer<K>::GetSize(data);
    size_t n2 = Serializer<V>::GetSize(data + n1);
    KeyValFlags flags;
    memcpy(&flags, data + n1 + n2, 1);
    return flags;
  }
  static void SetFlags(uint8_t* data, KeyValFlags flags) {
    size_t n1 = Serializer<K>::GetSize(data);
    size_t n2 = Serializer<V>::GetSize(data + n1);
    memcpy(data + n1 + n2, &flags, 1);
  }
};

}  // namespace detail

namespace wyhash {
inline void mum(uint64_t* a, uint64_t* b) {
#if defined(__SIZEOF_INT128__)
  __uint128_t r = *a;
  r *= *b;
  *a = static_cast<uint64_t>(r);
  *b = static_cast<uint64_t>(r >> 64U);
#elif defined(_MSC_VER) && defined(_M_X64)
  *a = _umul128(*a, *b, b);
#else
  uint64_t ha = *a >> 32U;
  uint64_t hb = *b >> 32U;
  uint64_t la = static_cast<uint32_t>(*a);
  uint64_t lb = static_cast<uint32_t>(*b);
  uint64_t hi{};
  uint64_t lo{};
  uint64_t rh = ha * hb;
  uint64_t rm0 = ha * lb;
  uint64_t rm1 = hb * la;
  uint64_t rl = la * lb;
  uint64_t t = rl + (rm0 << 32U);
  auto c = static_cast<uint64_t>(t < rl);
  lo = t + (rm1 << 32U);
  c += static_cast<uint64_t>(lo < t);
  hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
  *a = lo;
  *b = hi;
#endif
}

// multiply and xor mix function, aka MUM
[[nodiscard]] inline auto mix(uint64_t a, uint64_t b) -> uint64_t {
  mum(&a, &b);
  return a ^ b;
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
[[nodiscard]] inline auto r8(const uint8_t* p) -> uint64_t {
  uint64_t v{};
  std::memcpy(&v, p, 8U);
  return v;
}

[[nodiscard]] inline auto r4(const uint8_t* p) -> uint64_t {
  uint32_t v{};
  std::memcpy(&v, p, 4);
  return v;
}

// reads 1, 2, or 3 bytes
[[nodiscard]] inline auto r3(const uint8_t* p, size_t k) -> uint64_t {
  return (static_cast<uint64_t>(p[0]) << 16U) | (static_cast<uint64_t>(p[k >> 1U]) << 8U) | p[k - 1];
}

[[maybe_unused]] [[nodiscard]] inline auto hash(void const* key, size_t len) -> uint64_t {
  static constexpr auto secret = std::array{UINT64_C(0xa0761d6478bd642f), UINT64_C(0xe7037ed1a0b428db),
                                            UINT64_C(0x8ebc6af09c88c6e3), UINT64_C(0x589965cc75374cc3)};

  auto const* p = static_cast<uint8_t const*>(key);
  uint64_t seed = secret[0];
  uint64_t a{};
  uint64_t b{};
  if (FOLLY_LIKELY(len <= 16)) {
    if (FOLLY_LIKELY(len >= 4)) {
      a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
      b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
    } else if (FOLLY_LIKELY(len > 0)) {
      a = r3(p, len);
      b = 0;
    } else {
      a = 0;
      b = 0;
    }
  } else {
    size_t i = len;
    if (FOLLY_UNLIKELY(i > 48)) {
      uint64_t see1 = seed;
      uint64_t see2 = seed;
      do {
        seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
        see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
        see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
        p += 48;
        i -= 48;
      } while (FOLLY_LIKELY(i > 48));
      seed ^= see1 ^ see2;
    }
    while (FOLLY_UNLIKELY(i > 16)) {
      seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
      i -= 16;
      p += 16;
    }
    a = r8(p + i - 16);
    b = r8(p + i - 8);
  }

  return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
}

[[nodiscard]] inline auto hash(uint64_t x) -> uint64_t { return wyhash::mix(x, UINT64_C(0x9E3779B97F4A7C15)); }

}  // namespace wyhash

template <typename T, typename Enable = void>
struct hash {
  auto operator()(T const& obj) const
      noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>()))) -> uint64_t {
    return std::hash<T>{}(obj);
  }
};

template <>
struct hash<std::string_view> {
  using is_avalanching = void;
  auto operator()(std::string_view const& str) const noexcept -> uint64_t {
    return wyhash::hash(str.data(), str.size());
  }
};

#define RDICT_HASH_STATICCAST(T)                                                                                  \
  template <>                                                                                                     \
  struct hash<T> {                                                                                                \
    using is_avalanching = void;                                                                                  \
    auto operator()(T const& obj) const noexcept -> uint64_t { return wyhash::hash(static_cast<uint64_t>(obj)); } \
  }

RDICT_HASH_STATICCAST(bool);
RDICT_HASH_STATICCAST(uint8_t);
RDICT_HASH_STATICCAST(int8_t);
RDICT_HASH_STATICCAST(uint16_t);
RDICT_HASH_STATICCAST(int16_t);
RDICT_HASH_STATICCAST(uint32_t);
RDICT_HASH_STATICCAST(int32_t);
RDICT_HASH_STATICCAST(uint64_t);
RDICT_HASH_STATICCAST(int64_t);

// struct Bucket {
//   static constexpr uint32_t k_dist_inc = 1U << 8U;                // skip 1 byte fingerprint
//   static constexpr uint32_t k_fingerprint_mask = k_dist_inc - 1;  // mask for 1 byte of fingerprint
//   uint64_t value_idx;                                             // index into the m_values vector.
//   uint32_t dist_and_fingerprint;  // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
// };

template <typename KeyType, typename ValueType, typename HashFn = hash<KeyType>,
          typename KeyEqual = std::equal_to<KeyType>>
class ReadonlyDict {
 public:
  static constexpr float k_default_max_load_factor = 0.8F;
  struct Options {
    std::string path_prefix;
    size_t reserved_space_bytes = 100 * 1024 * 1024 * 1024LL;
    size_t bucket_count = 0;
    float max_load_factor = k_default_max_load_factor;
    bool readonly = false;
  };

  static absl::StatusOr<std::unique_ptr<ReadonlyDict>> New(const Options& opt);
  bool Exists(const KeyType& key) const;
  absl::StatusOr<ValueType> Get(const KeyType& key) const;
  absl::Status Put(const KeyType& key, const ValueType& val);

  size_t Size() const { return meta_->size; }
  absl::Status Commit();
  absl::Status Merge(const ReadonlyDict& other);

 private:
  static constexpr uint8_t initial_shifts = 64 - 2;  // 2^(64-m_shift) number of buckets
  static constexpr float default_max_load_factor = 0.8F;
  struct IndexMeta {
    size_t size = 0;
    size_t num_buckets = 0;
    size_t max_bucket_capacity = 0;
    uint8_t shifts = 0;
  };
  static constexpr uint32_t k_meta_reserved_space = 64;
  using Bucket = detail::Bucket<KeyType, ValueType>;
  // using value_idx_type = decltype(Bucket::value_idx);
  using value_idx_type = uint64_t;
  using dist_and_fingerprint_type = decltype(Bucket::dist_and_fingerprint);

  ReadonlyDict() {}
  absl::Status Init(const Options& opt);

  absl::Status LoadIndex(bool ignore_nonexist);

  [[nodiscard]] constexpr dist_and_fingerprint_type dist_and_fingerprint_from_hash(uint64_t hash) const {
    return Bucket::k_dist_inc | (static_cast<dist_and_fingerprint_type>(hash) & Bucket::k_fingerprint_mask);
  }
  [[nodiscard]] constexpr value_idx_type bucket_idx_from_hash(uint64_t hash) const {
    return static_cast<value_idx_type>(hash >> meta_->shifts);
  }
  // use the dist_inc and dist_dec functions so that uint16_t types work without warning
  [[nodiscard]] static constexpr auto dist_inc(dist_and_fingerprint_type x) -> dist_and_fingerprint_type {
    return static_cast<dist_and_fingerprint_type>(x + Bucket::k_dist_inc);
  }
  [[nodiscard]] value_idx_type next(value_idx_type bucket_idx) const {
    return (bucket_idx + 1U == meta_->num_buckets) ? 0 : static_cast<value_idx_type>(bucket_idx + 1U);
  }
  [[nodiscard]] constexpr auto mixed_hash(KeyType const& key) const -> uint64_t {
    if constexpr (detail::is_detected_v<detail::detect_avalanching, HashFn>) {
      // we know that the hash is good because is_avalanching.
      if constexpr (sizeof(decltype(hash_(key))) < sizeof(uint64_t)) {
        // 32bit hash and is_avalanching => multiply with a constant to avalanche bits upwards
        return hash_(key) * UINT64_C(0x9ddfea08eb382d69);
      } else {
        // 64bit and is_avalanching => only use the hash itself.
        return hash_(key);
      }
    } else {
      // not is_avalanching => apply wyhash
      return wyhash::hash(hash_(key));
    }
  }
  [[nodiscard]] static constexpr auto max_size() noexcept -> size_t {
    if constexpr ((std::numeric_limits<value_idx_type>::max)() == (std::numeric_limits<size_t>::max)()) {
      return size_t{1} << (sizeof(value_idx_type) * 8 - 1);
    } else {
      return size_t{1} << (sizeof(value_idx_type) * 8);
    }
  }
  static constexpr auto max_bucket_count() noexcept -> size_t {  // NOLINT(modernize-use-nodiscard)
    return max_size();
  }
  [[nodiscard]] auto max_load_factor() const -> float { return max_load_factor_; }
  [[nodiscard]] static constexpr auto calc_num_buckets(uint8_t shifts) -> size_t {
    return (std::min)(max_bucket_count(), size_t{1} << (64U - shifts));
  }
  auto bucket_count() const noexcept -> size_t {  // NOLINT(modernize-use-nodiscard)
    return meta_->num_buckets;
  }
  [[nodiscard]] constexpr auto calc_shifts_for_size(size_t s) const -> uint8_t {
    auto shifts = initial_shifts;
    while (shifts > 0 && static_cast<size_t>(static_cast<float>(calc_num_buckets(shifts)) * max_load_factor()) < s) {
      --shifts;
    }
    return shifts;
  }
  [[nodiscard]] auto next_while_less(KeyType const& key) const -> std::pair<uint32_t, uint64_t> {
    auto hash = mixed_hash(key);
    auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
    auto bucket_idx = bucket_idx_from_hash(hash);

    while (dist_and_fingerprint < buckets_[bucket_idx].dist_and_fingerprint) {
      dist_and_fingerprint = dist_inc(dist_and_fingerprint);
      bucket_idx = next(bucket_idx);
    }
    return {bucket_idx, dist_and_fingerprint};
  }
  void place_and_shift_up(Bucket bucket, value_idx_type place);
  /**
   * True when no element can be added any more without increasing the size
   */
  [[nodiscard]] auto is_full() const -> bool { return meta_->size > meta_->max_bucket_capacity; }
  absl::Status increase_size();
  void allocate_buckets_from_shift(uint8_t shift);
  absl::Status clear_and_fill_buckets_from_values(const uint8_t* data);
  void deallocate_buckets();
  void clear_buckets();
  absl::Status reserve(size_t capa);

  KeyType GetKeyByBucket(uint64_t bucket_idx) const;
  // KeyType GetKey(uint64_t offset) const;
  detail::KeyValFlags GetKeyValFlags(uint64_t offset) const;
  // ValueType GetValue(uint64_t offset) const;
  ValueType GetValueByBucket(uint64_t bucket_idx) const;
  absl::Status Update(Bucket* bucket, const KeyType& k, const ValueType& v);
  absl::Status PutDirect(const KeyType& k, uint64_t offset);
  const uint8_t* GetKeyValData(uint64_t offset) const;
  uint8_t* GetKeyValData(uint64_t offset);
  absl::StatusOr<size_t> Append(const KeyType& k, const ValueType& v);
  absl::StatusOr<Bucket> NewBucket(const KeyType& k, const ValueType& v,
                                   dist_and_fingerprint_type dist_and_fingerprint);

  Options opt_;
  std::unique_ptr<MmapFile> data_mmap_file_;
  HashFn hash_{};
  KeyEqual equal_;
  Bucket* buckets_ = nullptr;
  IndexMeta* meta_ = nullptr;
  std::vector<uint8_t> index_buffer_;

  float max_load_factor_ = default_max_load_factor;
};
template <typename K, typename V, typename H, typename E>
absl::StatusOr<std::unique_ptr<ReadonlyDict<K, V, H, E>>> ReadonlyDict<K, V, H, E>::New(const Options& opt) {
  std::unique_ptr<ReadonlyDict<K, V, H, E>> p(new ReadonlyDict<K, V, H, E>);
  auto status = p->Init(opt);
  if (!status.ok()) {
    return status;
  }
  return p;
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::LoadIndex(bool ignore_nonexist) {
  std::string index_path = opt_.path_prefix + ".index";
  if (!folly::readFile(index_path.c_str(), index_buffer_)) {
    if (ignore_nonexist) {
      if (access(index_path.c_str(), F_OK) == -1) {
        // nonexist
        return absl::OkStatus();
      }
    }
    return absl::InvalidArgumentError("read index failed");
  }
  buckets_ = reinterpret_cast<Bucket*>(&index_buffer_[k_meta_reserved_space]);
  meta_ = reinterpret_cast<IndexMeta*>(&index_buffer_[0]);
  return absl::OkStatus();
}

template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::Init(const Options& opt) {
  opt_ = opt;
  if constexpr (Bucket::is_flat) {
    auto status = LoadIndex(true);
    if (status.ok()) {
      if (nullptr != meta_ && meta_->size > 0) {
        return absl::OkStatus();
      }
    } else {
      return status;
    }
    if (0 != opt_.bucket_count) {
      auto status = reserve(opt_.bucket_count);
      if (!status.ok()) {
        return status;
      }
    } else {
      allocate_buckets_from_shift(0);
      clear_buckets();
    }
    return absl::OkStatus();
  } else {
    std::string data_path = opt.path_prefix + ".data";
    MmapFile::Options data_opts;
    data_opts.path = data_path;
    data_opts.readonly = opt.readonly;
    data_opts.reserved_space_bytes = opt.reserved_space_bytes;

    auto data_file_result = MmapFile::Open(data_opts);
    if (!data_file_result.ok()) {
      return data_file_result.status();
    }
    data_mmap_file_ = std::move(data_file_result.value());
    max_load_factor_ = opt_.max_load_factor;

    if (data_mmap_file_->GetWriteOffset() == 0) {
      if (0 != opt_.bucket_count) {
        auto status = reserve(opt_.bucket_count);
        if (!status.ok()) {
          return status;
        }
      } else {
        allocate_buckets_from_shift(0);
        clear_buckets();
      }
      return absl::OkStatus();
    } else {
      // read exist data
      return LoadIndex(false);
    }
  }
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::reserve(size_t capa) {
  capa = (std::min)(capa, max_size());
  size_t current_size = nullptr != meta_ ? meta_->size : 0;
  size_t num_buckets = nullptr != meta_ ? meta_->num_buckets : 0;
  uint8_t current_shifts = nullptr == meta_ ? initial_shifts : meta_->shifts;
  auto shifts = calc_shifts_for_size((std::max)(capa, current_size));
  if (0 == num_buckets || shifts < current_shifts) {
    deallocate_buckets();
    if constexpr (Bucket::is_flat) {
      auto old_index_data = index_buffer_;
      allocate_buckets_from_shift(shifts);
      return clear_and_fill_buckets_from_values(&old_index_data[k_meta_reserved_space]);
    } else {
      allocate_buckets_from_shift(shifts);
      return clear_and_fill_buckets_from_values(data_mmap_file_->GetRawData());
    }
  }
  return absl::OkStatus();
}

template <typename K, typename V, typename H, typename E>
void ReadonlyDict<K, V, H, E>::allocate_buckets_from_shift(uint8_t shifts) {
  // auto ba = bucket_alloc(m_values.get_allocator());
  if (shifts == 0) {
    shifts = nullptr == meta_ ? initial_shifts : meta_->shifts;
  }
  size_t orig_size = nullptr == meta_ ? 0 : meta_->size;
  size_t num_buckets = calc_num_buckets(shifts);

  index_buffer_.resize(num_buckets * sizeof(Bucket) + k_meta_reserved_space);
  buckets_ = reinterpret_cast<Bucket*>(&index_buffer_[k_meta_reserved_space]);
  meta_ = reinterpret_cast<IndexMeta*>(&index_buffer_[0]);

  meta_->num_buckets = num_buckets;
  meta_->shifts = shifts;
  meta_->size = orig_size;

  if (meta_->num_buckets == max_bucket_count()) {
    // reached the maximum, make sure we can use each bucket
    meta_->max_bucket_capacity = max_bucket_count();
  } else {
    meta_->max_bucket_capacity =
        static_cast<value_idx_type>(static_cast<float>(meta_->num_buckets) * max_load_factor());
  }
  // printf("after allocate_buckets_from_shift,max_bucket_capacity:%lld, num_buckets:%lld, size:%lld, shifts:%d\n",
  //        meta_->max_bucket_capacity, meta_->num_buckets, meta_->size, meta_->shifts);
}

template <typename K, typename V, typename H, typename E>
absl::StatusOr<typename ReadonlyDict<K, V, H, E>::Bucket> ReadonlyDict<K, V, H, E>::NewBucket(
    const K& k, const V& v, dist_and_fingerprint_type dist_and_fingerprint) {
  if constexpr (Bucket::is_flat) {
    return Bucket{k, v, dist_and_fingerprint};
  } else {
    auto buffer = detail::KeyValPair<K, V>::Pack(k, v);
    auto result = data_mmap_file_->Add(buffer.data(), buffer.size());
    if (!result.ok()) {
      return result.status();
    }
    return Bucket{result.value(), dist_and_fingerprint};
  }
}

template <typename K, typename V, typename H, typename E>
void ReadonlyDict<K, V, H, E>::place_and_shift_up(Bucket bucket, value_idx_type place) {
  // Bucket bucket{offset, dist_and_fingerprint};
  while (0 != buckets_[place].dist_and_fingerprint) {
    bucket = std::exchange(buckets_[place], bucket);
    bucket.dist_and_fingerprint = dist_inc(bucket.dist_and_fingerprint);
    place = next(place);
  }
  buckets_[place] = bucket;
}

template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::clear_and_fill_buckets_from_values(const uint8_t* data) {
  clear_buckets();
  if constexpr (Bucket::is_flat) {
    const Bucket* old_buckets = reinterpret_cast<const Bucket*>(data);
    uint64_t bucket_offset = 0;
    for (uint64_t value_idx = 0; value_idx < meta_->size;) {
      if (old_buckets[bucket_offset].dist_and_fingerprint == 0) {
        bucket_offset++;
        continue;
      }
      auto [bucket_idx, dist_and_fingerprint] = next_while_less(old_buckets[bucket_offset].key);
      Bucket new_bucket = old_buckets[bucket_offset];
      new_bucket.dist_and_fingerprint = dist_and_fingerprint;
      place_and_shift_up(new_bucket, bucket_idx);
      bucket_offset++;
      value_idx++;
    }
  } else {
    uint64_t offset = 0;
    for (uint64_t value_idx = 0; value_idx < meta_->size;) {
      if (offset >= data_mmap_file_->GetWriteOffset()) {
        return absl::InvalidArgumentError("offset overflow");
      }
      auto flags = GetKeyValFlags(offset);
      auto key_val_len = detail::KeyValPair<K, V>::GetKeyValuePackSize(data + offset);
      if (!flags.invalid) {
        auto key = detail::KeyValPair<K, V>::UnpackKey(data + offset);
        auto [bucket_idx, dist_and_fingerprint] = next_while_less(key);
        // printf("####￥￥￥ key:%lld bucket_idx:%lld dist_and_fingerprint:%lld, offset:%lld\n", key, bucket_idx,
        //        dist_and_fingerprint, offset);
        place_and_shift_up({offset, dist_and_fingerprint}, bucket_idx);
        value_idx++;
      }
      offset += key_val_len;
    }
    // printf("####end clear_and_fill_buckets_from_values with size:%lld\n", meta_->size);
  }

  return absl::OkStatus();
}
template <typename K, typename V, typename H, typename E>
const uint8_t* ReadonlyDict<K, V, H, E>::GetKeyValData(uint64_t offset) const {
  return data_mmap_file_->GetRawData() + offset;
}
template <typename K, typename V, typename H, typename E>
uint8_t* ReadonlyDict<K, V, H, E>::GetKeyValData(uint64_t offset) {
  return data_mmap_file_->GetRawData() + offset;
}

template <typename K, typename V, typename H, typename E>
K ReadonlyDict<K, V, H, E>::GetKeyByBucket(uint64_t bucket_idx) const {
  const Bucket* bucket = buckets_ + bucket_idx;
  if constexpr (Bucket::is_flat) {
    return bucket->key;
  } else {
    const uint8_t* key_val_data = GetKeyValData(bucket->value_idx);
    return detail::KeyValPair<K, V>::UnpackKey(key_val_data);
  }
}
template <typename K, typename V, typename H, typename E>
V ReadonlyDict<K, V, H, E>::GetValueByBucket(uint64_t bucket_idx) const {
  const Bucket* bucket = buckets_ + bucket_idx;
  if constexpr (Bucket::is_flat) {
    return bucket->val;
  } else {
    const uint8_t* key_val_data = GetKeyValData(bucket->value_idx);
    K k;
    V v;
    detail::KeyValPair<K, V>::Unpack(key_val_data, k, v);
    return v;
  }
}

template <typename K, typename V, typename H, typename E>
detail::KeyValFlags ReadonlyDict<K, V, H, E>::GetKeyValFlags(uint64_t offset) const {
  const uint8_t* key_val_data = GetKeyValData(offset);
  return detail::KeyValPair<K, V>::GetFlags(key_val_data);
}

template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::Update(Bucket* bucket, const K& k, const V& v) {
  if constexpr (Bucket::is_flat) {
    bucket->val = v;
  } else {
    auto buffer = detail::KeyValPair<K, V>::Pack(k, v);
    auto result = data_mmap_file_->Add(buffer.data(), buffer.size());
    if (!result.ok()) {
      return result.status();
    }
    uint8_t* key_val_data = GetKeyValData(bucket->value_idx);
    detail::KeyValFlags flags;
    flags.invalid = 1;
    detail::KeyValPair<K, V>::SetFlags(key_val_data, flags);
    bucket->value_idx = result.value();
  }
  return absl::OkStatus();
}

template <typename K, typename V, typename H, typename E>
void ReadonlyDict<K, V, H, E>::deallocate_buckets() {
  if (nullptr != buckets_) {
    // free(buckets_);
    buckets_ = nullptr;
  }
  if (nullptr != meta_) {
    meta_->num_buckets = 0;
    meta_->max_bucket_capacity = 0;
  }
}
template <typename K, typename V, typename H, typename E>
void ReadonlyDict<K, V, H, E>::clear_buckets() {
  if (buckets_ != nullptr) {
    std::memset(buckets_, 0, sizeof(Bucket) * bucket_count());
  }
}
template <typename K, typename V, typename H, typename E>
absl::StatusOr<size_t> ReadonlyDict<K, V, H, E>::Append(const K& k, const V& v) {
  if constexpr (Bucket::is_flat) {
    return 0;
  } else {
    auto buffer = detail::KeyValPair<K, V>::Pack(k, v);
    auto result = data_mmap_file_->Add(buffer.data(), buffer.size());
    return result;
  }
}

template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::increase_size() {
  if (meta_->max_bucket_capacity == max_bucket_count()) {
    // remove the value again, we can't add it!
    return absl::InvalidArgumentError("capacity overflow");
  }
  --meta_->shifts;
  deallocate_buckets();
  if constexpr (Bucket::is_flat) {
    auto old_index_data = index_buffer_;
    allocate_buckets_from_shift(0);
    return clear_and_fill_buckets_from_values(&old_index_data[k_meta_reserved_space]);
  } else {
    allocate_buckets_from_shift(0);
    return clear_and_fill_buckets_from_values(data_mmap_file_->GetRawData());
  }
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::Put(const K& key, const V& val) {
  auto hash = mixed_hash(key);
  auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
  auto bucket_idx = bucket_idx_from_hash(hash);

  while (dist_and_fingerprint <= buckets_[bucket_idx].dist_and_fingerprint) {
    if (dist_and_fingerprint == buckets_[bucket_idx].dist_and_fingerprint && equal_(key, GetKeyByBucket(bucket_idx))) {
      return Update(buckets_ + bucket_idx, key, val);
    }
    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
    bucket_idx = next(bucket_idx);
  }

  // auto buffer = detail::KeyValPair<K, V>::Pack(key, val);
  // auto result = data_mmap_file_->Add(buffer.data(), buffer.size());
  auto result = NewBucket(key, val, dist_and_fingerprint);
  if (!result.ok()) {
    return result.status();
  }
  auto entry_bucket = result.value();

  // printf(
  //     "###put key:%lld bucket_idx:%lld offset:%lld is_full:%d num_buckets:%lld, size:%lld max_bucket_capacity: % "
  //     "lld\n ",
  //     key, bucket_idx, result.value(), is_full(), meta_->num_buckets, meta_->size, meta_->max_bucket_capacity);
  absl::Status status;
  if (FOLLY_UNLIKELY(is_full())) {
    status = increase_size();
    if (!status.ok()) {
      return status;
    }
    auto [key_bucket_idx, dist_and_fingerprint] = next_while_less(key);
    entry_bucket.dist_and_fingerprint = dist_and_fingerprint;
    place_and_shift_up(entry_bucket, key_bucket_idx);
  } else {
    // place element and shift up until we find an empty spot
    place_and_shift_up(entry_bucket, bucket_idx);
    // printf("###Put[%lld] bucket_idx:%lld dist_and_fingerprint:%lld, offset:%lld\n", key, bucket_idx,
    //        dist_and_fingerprint, result.value());
  }
  meta_->size++;
  return status;
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::PutDirect(const K& key, uint64_t entry_offset) {
  auto hash = mixed_hash(key);
  auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
  auto bucket_idx = bucket_idx_from_hash(hash);
  while (dist_and_fingerprint <= buckets_[bucket_idx].dist_and_fingerprint) {
    if (dist_and_fingerprint == buckets_[bucket_idx].dist_and_fingerprint && equal_(key, GetKeyByBucket(bucket_idx))) {
      buckets_[bucket_idx].value_idx = entry_offset;
      meta_->size++;
      return absl::OkStatus();
    }
    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
    bucket_idx = next(bucket_idx);
  }
  absl::Status status;
  if (FOLLY_UNLIKELY(is_full())) {
    // increase_size just rehashes all the data we have in m_values
    meta_->size++;
    status = increase_size();
    if (status.ok()) {
      meta_->size--;
    }
    // if (status.ok()) {
    //   auto [bucket_idx, dist_and_fingerprint] = next_while_less(key);
    //   place_and_shift_up({entry_offset, dist_and_fingerprint}, bucket_idx);
    // }
  } else {
    // place element and shift up until we find an empty spot
    place_and_shift_up({entry_offset, dist_and_fingerprint}, bucket_idx);
  }
  return status;
}

template <typename K, typename V, typename H, typename E>
bool ReadonlyDict<K, V, H, E>::Exists(const K& key) const {
  auto hash = mixed_hash(key);
  auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
  auto bucket_idx = bucket_idx_from_hash(hash);
  while (dist_and_fingerprint <= buckets_[bucket_idx].dist_and_fingerprint) {
    if (dist_and_fingerprint == buckets_[bucket_idx].dist_and_fingerprint && equal_(key, GetKeyByBucket(bucket_idx))) {
      return true;
    }
    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
    bucket_idx = next(bucket_idx);
  }
  return false;
}

template <typename K, typename V, typename H, typename E>
absl::StatusOr<V> ReadonlyDict<K, V, H, E>::Get(const K& key) const {
  auto hash = mixed_hash(key);
  auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
  auto bucket_idx = bucket_idx_from_hash(hash);
  // printf("####[%lld]get %lld %lld %lld\n", key, bucket_idx, dist_and_fingerprint,
  //        buckets_[bucket_idx].dist_and_fingerprint);
  while (dist_and_fingerprint <= buckets_[bucket_idx].dist_and_fingerprint) {
    // printf("##cmp %lld %lld %lld %d\n", key, GetKey(offset), offset, equal_(key, GetKey(offset)));
    if (dist_and_fingerprint == buckets_[bucket_idx].dist_and_fingerprint && equal_(key, GetKeyByBucket(bucket_idx))) {
      return GetValueByBucket(bucket_idx);
    }
    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
    bucket_idx = next(bucket_idx);
  }
  return absl::NotFoundError("not found entry");
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::Commit() {
  std::string index_path = opt_.path_prefix + ".index";

  if (!folly::writeFile(index_buffer_, index_path.c_str())) {
    int err = errno;
    return absl::ErrnoToStatus(err, "write rdict index file failed.");
  }
  if (data_mmap_file_) {
    auto result = data_mmap_file_->ShrinkToFit();
    if (!result.ok()) {
      return result.status();
    }
  }

  return absl::OkStatus();
}
template <typename K, typename V, typename H, typename E>
absl::Status ReadonlyDict<K, V, H, E>::Merge(const ReadonlyDict& other) {
  if (opt_.readonly) {
    return absl::PermissionDeniedError("Unable to merge into readonly rdict");
  }
  size_t total_size = meta_->size + other.meta_->size;
  size_t estimate_bucket_num = static_cast<size_t>(total_size * 1.0 / max_load_factor_);
  auto status = reserve(estimate_bucket_num);
  if (!status.ok()) {
    return status;
  }
  if constexpr (Bucket::is_flat) {
    const Bucket* other_buckets = other.buckets_;
    uint64_t bucket_offset = 0;
    for (uint64_t value_idx = 0; value_idx < other.meta_->size;) {
      if (other_buckets[bucket_offset].dist_and_fingerprint == 0) {
        bucket_offset++;
        continue;
      }
      auto [bucket_idx, dist_and_fingerprint] = next_while_less(other_buckets[bucket_offset].key);
      Bucket new_bucket = other_buckets[bucket_offset];
      new_bucket.dist_and_fingerprint = dist_and_fingerprint;
      place_and_shift_up(new_bucket, bucket_idx);
      bucket_offset++;
      value_idx++;
      meta_->size++;
    }
  } else {
    uint64_t offset = 0;
    const uint8_t* other_data = other.data_mmap_file_->GetRawData();
    uint64_t currrent_write_offset = data_mmap_file_->GetWriteOffset();
    for (uint64_t value_idx = 0; value_idx < other.meta_->size;) {
      if (offset >= other.data_mmap_file_->GetWriteOffset()) {
        return absl::InvalidArgumentError("offset overflow");
      }
      auto flags = other.GetKeyValFlags(offset);
      auto key_val_len = detail::KeyValPair<K, V>::GetKeyValuePackSize(other_data + offset);
      if (!flags.invalid) {
        auto key = detail::KeyValPair<K, V>::UnpackKey(other_data + offset);
        auto [bucket_idx, dist_and_fingerprint] = next_while_less(key);
        place_and_shift_up({currrent_write_offset + offset, dist_and_fingerprint}, bucket_idx);
        // printf("####put key:%lld, offset:%lld/%lld, bucket_idx:%lld,key_val_len:%lld\n", key, offset,
        //        currrent_write_offset + offset, bucket_idx, key_val_len);
        value_idx++;
        meta_->size++;
      } else {
        // printf("####invalid key_val_len:%lld,offset:%lld\n", key_val_len, offset);
      }
      offset += key_val_len;
    }
    // printf("###currrent_write_offset:%lld, other write_offset:%lld, size:%lld\n", currrent_write_offset,
    //        other.data_mmap_file_->GetWriteOffset(), meta_->size);
    auto result = data_mmap_file_->Add(other.data_mmap_file_->GetRawData(), other.data_mmap_file_->GetWriteOffset());
    if (!result.ok()) {
      return result.status();
    }
  }
  return absl::OkStatus();
}

}  // namespace rdict
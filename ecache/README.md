# ECache
A cache lib built on facebook's CacheLib

## Usage
一些基本概念和限制：  
- 缓存淘汰策略， 目前支持四种：
  - CACHE_LRU
  - CACHE_LRU2Q
  - CACHE_TINYLFU
  - CACHE_LRU_SPIN_BUCKET
- Pool, CacheLib中的cache分区，一个cache实例可以有多个Pool； 每个Pool有自己的大小限制
- Value类型，类似Redis的多数据结构， 目前只有三种：
  - String， 大小目前最大4MB（后续可能去掉）
  - Ordered Map， 有序map， 类似std::map
    - Field的自定义结构需要提供Encode/Decode实现, 需要按照比较语义顺序编码各个字段
    - Field的限制为定长，一般为8的倍数， 且目前代码实现中限制为最大256;
    - Value限制也是4MB
  - Unordered Map, hashmap实现， 类似std::unordered_map
    - Field的自定义结构需要提供Encode/Decode实现, 需要按照比较语义顺序编码各个字段
    - Field的限制为定长，一般为8的倍数， 且目前代码实现中限制为最大256;
    - Value限制也是4MB

### 创建Cache

```cpp
    ECacheManagerConfig manager_config;
    manager_config.set_type(CACHE_LRU2Q);                //缓存淘汰策略
    manager_config.set_size(1 * 1024 * 1024 * 1024);     //Cache大小
    ECacheManager cache_manager;
    cache_manager.Init(manager_config);
```

### 创建Pool
```cpp
    ECacheConfig config;
    config.set_name("test");              // pool name
    config.set_size(1024 * 1024 * 1024);  // pool 大小
    std::unique_ptr<ECache> cache = cache_manager.NewCache(config);
```

### Read/Write String
```cpp
    std::string key = "string" + std::to_string(i);
    std::string val = "value" + std::to_string(i);
    cache->Set(key, val, 10000);  //set with expire secs

    auto val = cache->Get(key);
    ECACHE_INFO("key:{},val:{}", key,  val.value_view);
```

### 自定义Map(Ordered/Unordered) Field结构
```cpp
struct CustomMapField {
  static constexpr int field_size = 16;
  int64_t score = 0;
  int64_t id = 0;
  folly::fbstring Encode() const {
    folly::fbstring s;
    // 按排序规则顺序encode，encode结果长度和field_size 一致
    field_encode_int(s, score); 
    field_encode_int(s, id);
    return s;
  }
  template <typename T>
  void Decode(T& view) {
    field_decode_int(view, 0, score);
    field_decode_int(view, 8, id);
  }
};
```

### Read/Write Unordered Map
```cpp
    CustomMapField f;
    f.score = 1000;
    f.id = 0;
    cache->UnorderedMapSet("hashmap", f, "value0", 10000);  //set with expire secs
    auto val = cache->UnorderedMapGet("hashmap", f);
    ECACHE_INFO("key:hashmap, field:{}/{}, val:{}", f.score, f.id, val.value_view);
```

### Read/Write Ordered Map
```cpp
    std::string key = "range_map";
    CustomMapField f;
    f.score = 1000;
    f.id = 0;
    for (int j = 0; j < 1000; j++) {
      std::string val = "value_" + std::to_string(j);
      cache2->OrderedMapSet(key, f, val, 10000);
      f.score++;
      f.id++;
    }

    std::vector<CacheValue> range;
    CustomMapField min, max;
    min.score = 100;
    max.score = 200;
    cache->OrderedMapRangeGet<CustomMapField>("range_map", min, max, range);

```

### Full API
```cpp
class ECache {
 public:
  virtual int Init(const ECacheConfig& config) = 0;
  virtual uint8_t GetPoolId() = 0;
  virtual CacheValue Set(std::string_view key, std::string_view value, int expire_secs = -1) = 0;
  virtual CacheValue Get(std::string_view key) = 0;
  virtual RemoveRes Del(std::string_view key, CacheKeyType type = CACHE_KEY_STRING,
                        size_t field_size = 0) = 0;
  virtual bool Expire(std::string_view key, uint32_t secs, CacheKeyType type = CACHE_KEY_STRING,
                      size_t field_size = 0) = 0;
  virtual bool ExpireAt(std::string_view key, uint32_t secs, CacheKeyType type = CACHE_KEY_STRING,
                        size_t field_size = 0) = 0;
  virtual bool Exists(std::string_view key, CacheKeyType type = CACHE_KEY_STRING,
                      size_t field_size = 0) = 0;

  template <typename T>
  RemoveRes DelOrderedMap(std::string_view key) {
    return Del(key, CACHE_KEY_RANGE_MAP, T::field_size);
  }
  template <typename T>
  bool ExistsOrderedMap(std::string_view key) {
    return Exists(key, CACHE_KEY_RANGE_MAP, T::field_size);
  }
  template <typename T>
  InsertOrReplaceResult OrderedMapSet(std::string_view key, const T& field, std::string_view value,
                                      int expire_secs = -1) {
    auto field_bin = field.Encode();
    return RangeMapSet(key, field_bin, value, expire_secs);
  }
  template <typename T>
  CacheValue OrderedMapGet(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return RangeMapGet(key, field_bin);
  }
  template <typename T>
  int OrderedMapRangeGet(std::string_view key, const T& min, const T& max,
                         std::vector<CacheValue>& vals) {
    auto field_min_bin = min.Encode();
    auto field_max_bin = max.Encode();
    return RangeMapRangeGet(key, field_min_bin, field_max_bin, vals);
  }
  template <typename T>
  CacheValue OrderedMapMin(std::string_view key) {
    return RangeMapMin(key, T::field_size);
  }
  template <typename T>
  bool OrderedMapDel(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return RangeMapDel(key, field_bin);
  }
  template <typename T>
  int OrderedMapSize(std::string_view key) {
    return RangeMapSize(key, T::field_size);
  }
  template <typename T>
  int OrderedMapCapacity(std::string_view key) {
    return RangeMapCapacity(key, T::field_size);
  }
  template <typename T>
  int OrderedMapSizeInBytes(std::string_view key) {
    return RangeMapSizeInBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapRemainingBytes(std::string_view key) {
    return RangeMapRemainingBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapWastedBytes(std::string_view key) {
    return RangeMapWastedBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapCompact(std::string_view key) {
    return RangeMapCompact(key, T::field_size);
  }
  template <typename T>
  bool OrderedMapPop(std::string_view key) {
    return RangeMapPop(key, T::field_size);
  }
  template <typename T>
  int OrderedMapGetAll(std::string_view key, std::vector<CacheValue>& vals) {
    return RangeMapGetAll(key, T::field_size, vals);
  }

  template <typename T>
  RemoveRes DelUnorderedMap(std::string_view key) {
    return Del(key, CACHE_KEY_HASH_MAP, T::field_size);
  }
  template <typename T>
  bool ExistsUnorderedMap(std::string_view key) {
    return Exists(key, CACHE_KEY_HASH_MAP, T::field_size);
  }
  template <typename T>
  InsertOrReplaceResult UnorderedMapSet(std::string_view key, const T& field,
                                        std::string_view value, int expire_secs = -1) {
    auto field_bin = field.Encode();
    return HashMapSet(key, field_bin, value, expire_secs);
  }
  template <typename T>
  CacheValue UnorderedMapGet(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return HashMapGet(key, field_bin);
  }
  template <typename T>
  int UnorderedMapSize(std::string_view key) {
    return HashMapSize(key, T::field_size);
  }
  template <typename T>
  bool UnorderedMapDel(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return HashMapDel(key, field_bin);
  }

  template <typename T>
  int UnorderedMapGetAll(std::string_view key, std::vector<CacheValue>& vals) {
    return HashMapGetAll(key, T::field_size, vals);
  }
  template <typename T>
  int UnorderedMapSizeInBytes(std::string_view key) {
    return HashMapSizeInBytes(key, T::field_size);
  }
  template <typename T>
  int UnorderedMapCompact(std::string_view key) {
    return HashMapCompact(key, T::field_size);
  }
};
```

### 备份
```cpp
  ECacheManager cache_manager;
  int rc = cache_manager.Save("./ecache.save");
  if (0 != rc) {
    ECACHE_ERROR("Failed to save ecache", rc);
  }
```


### 恢复
```cpp
  ECacheManager cache_manager;
  int rc = cache_manager.Load("./ecache.save");
  if (0 != rc) {
    ROBIMS_ERROR("Failed to load ecache", rc);
  }

  std::unique_ptr<ECache> cache = cache_manager.GetCache("test"); //Get by Pool name
```


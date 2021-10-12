# Robims
bitmap index lib

## Usage
一些基本概念：  
- DB， 一份索引实例
- Table， 索引中的一个表， 一个索引可以含多个表
- Field， 表里的字段
- IndexType， 字段索引类型，目前支持有限几种：
  - SET_INDEX， 一个字段可以有多个string值，例如item的tag
  - WEIGHT_SET_INDEX, 一个字段可以有多个带权重的string值，例如item的tag+score
  - MUTEX_INDEX， 一个字段只有一个string值
  - BOOL_INDEX， 一个字段只有一个bool值
  - INT_INDEX， 一个字段只有一个int64值
  - FLOAT_INDEX， 一个字段只有一个float值

### 创建表

```cpp
  TableSchema table0;
  table0.set_name("test");                   //table 名
  table0.set_id_field("id");                 // 必须有的唯一id字段
  auto field0 = table0.add_index_field();    // 增加索引字段
  field0->set_name("age");
  field0->set_index_type(INT_INDEX);      
  auto field1 = table0.add_index_field();
  field1->set_name("city");
  field1->set_index_type(SET_INDEX);
  auto field3 = table0.add_index_field();
  field3->set_name("score");
  field3->set_index_type(robims::FLOAT_INDEX);
  auto field4 = table0.add_index_field();
  field4->set_name("gender");
  field4->set_index_type(robims::MUTEX_INDEX);
  auto field5 = table0.add_index_field();
  field5->set_name("is_child");
  field5->set_index_type(robims::BOOL_INDEX);

  RobimsDB db;
  int rc = db.CreateTable(table0);           // 创建table
  if (0 != rc) {
    ROBIMS_ERROR("Failed to create table:{}", rc);
  }
```

### 写入数据
```cpp
  RobimsDB db;
  //.....
  std::string json = "...";       // valid json by schema
  int rc = db.Put("test", json);  // 'test' is table name
  if (0 != rc) {
    ROBIMS_ERROR("Failed to put json with rc:{}", rc);
  }
```

### 搜索
```cpp
  RobimsDB db;
  //.....
  // 表达式中使用 <table>.<field>代表索引字段， 类sql中用法
  // BOOL_INDEX的比较用1/0表示， 如 test.is_child==1
  std::string query = "test.age>50 && test.score>60 && test.city == \"sz\"";
  std::vector<std::string> ids;
  int rc = db.Select(query, ids);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to select with rc:{}", rc);
    return;
  }
```

### 恢复
```cpp
  RobimsDB db;
  //.....
  // true: 保存只读readonly模式  false：非readonly模式
  int rc = db.Save("./robims.save", true);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save robims:{}", rc);
  }
```

### 恢复
```cpp
  RobimsDB db;
  int rc = db.Load("./robims.save");
  if (0 != rc) {
    ROBIMS_ERROR("Failed to load robims", rc);
    return;
  }
```
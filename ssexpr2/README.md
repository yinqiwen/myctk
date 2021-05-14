# Spirit Jit Expression
基于Boost Spirit X3的jit expression 实现， 支持基本运算，变量访问以及自定义方法;   
包含两部分：
- JIT Struct, 基于jit的动态结构成员访问,支持自定义的POD，flatbuffers、protobuffers数据结构
- JIT Expression， 基于jit的表达式执行
相比非jit的实现，example的性能提升在15~100倍之间；同时引入一个限制（linux + x86-64 only）

## Example

### Logic Expression
```cpp
//custom func1
static Value cfunc1() {
  Value v;
  v.Set<int64_t>(101);
  return v;
}
//custom func2
static Value cfunc2(Value arg0) {
  Value v;
  v.Set<double>(arg0.Get<int64_t>() + 2.0);
  return v;
}

DEFINE_JIT_STRUCT(SubItem1, (double)score, (std::string)id, (int32_t)vv)
DEFINE_JIT_STRUCT(Item1, (double)score, (std::string)id, (int32_t)vv, (SubItem1)sub)

int main() {
  // 5.2 + 101 - 5.0
  std::string str = "1 + 2*3.1 - 6/3 + cfunc1() - cfunc2(3) + sub.score";
  ssexpr2::SpiritExpression expr;
  ssexpr2::ExprOptions options;
  options.Init<Item1>();
  // add custom func
  options.functions["cfunc1"] = (ExprFunction)cfunc1;
  options.functions["cfunc2"] = (ExprFunction)cfunc2;
  int rc = expr.Init(str, options);
  if (0 != rc) {
    printf("Init %s err:%d\n", str.c_str(), rc);
    return rc;
  }

  Item1 item;
  item.sub.score = 99.2;
  ssexpr2::Value rv = expr.Eval(item);
  printf("Eval result:%.2f\n", rv.Get<double>()); //200.2
  return 0;
}
```



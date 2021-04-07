# Spirit Expression
基于Boost Spirit X3的expression 实现， 支持基本运算，变量访问以及自定义方法

## Example

### Logic Expression
```cpp
DEFINE_EXPR_STRUCT(SubUser, (int32_t)age, (std::string)id)
DEFINE_EXPR_STRUCT(User, (double)score, (std::string)id, (SubUser*)sub)
DEFINE_EXPR_STRUCT(SubItem, (int64_t)uv, (std::string)id)
DEFINE_EXPR_STRUCT(Item, (double)score, (std::string)id, (SubItem)sub)
DEFINE_EXPR_STRUCT(Combine, (User*)user, (Item*)item, (double)score, (std::string)name)

void test_logic() {
  ssexpr::SpiritExpression expr;
  ssexpr::ExprOptions opt;
  opt.Init<Combine>();
  //                         true                true                  false
  int rc = expr.Init("user.id == \"uid123\" && (item.score > 1 || item.id == \"item101\") ", opt);
  printf("Logic Expression init with rc:%d\n", rc);
  SubUser su = {101, "random"};
  User user = {1.21, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine root = {&user, &item, 100.2, "combine"};  // root object
  ssexpr::EvalContext ctx;
  auto val = expr.Eval(ctx, root);
  printf("Logic Expression eval result:%d\n", std::get<bool>(val));
}
```

### Calc Expression
```cpp
DEFINE_EXPR_STRUCT(SubUser, (int32_t)age, (std::string)id)
DEFINE_EXPR_STRUCT(User, (double)score, (std::string)id, (SubUser*)sub)
DEFINE_EXPR_STRUCT(SubItem, (int64_t)uv, (std::string)id)
DEFINE_EXPR_STRUCT(Item, (double)score, (std::string)id, (SubItem)sub)
DEFINE_EXPR_STRUCT(Combine, (User*)user, (Item*)item, (double)score, (std::string)name)

void test_calc() {
  ssexpr::SpiritExpression expr;
  ssexpr::ExprOptions opt;
  // custom functions
  opt.functions["hash"] = [](const std::vector<ssexpr::Value>& args) {
    int64_t v1 = std::get<int64_t>(args[0]);
    int64_t v2 = std::get<int64_t>(args[1]);
    ssexpr::Value v = v1 % v2;
    return v;
  };
  opt.Init<Combine>();
  int rc = expr.Init("1 + 2.11 + hash(1712, 100) + user.score", opt);

  printf("Calc Expression init with rc:%d\n", rc);
  SubUser su = {101, "random"};
  User user = {1.21, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine root = {&user, &item, 100.2, "combine"};  // root object
  ssexpr::EvalContext ctx;
  auto val = expr.Eval(ctx, root);
  printf("Calc Expression eval result:%.2f\n", std::get<double>(val));
}
```


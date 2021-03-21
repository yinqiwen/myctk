// Created on 2021/03/21
// Authors: yinqiwen (yinqiwen@gmail.com)

#include <stdio.h>
#include <sys/time.h>
#include <sstream>
#include "expr_struct.h"
using namespace expr_struct;
static int64_t ustime(void) {
  struct timeval tv;
  int64_t ust;

  gettimeofday(&tv, NULL);
  ust = ((long)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

std::vector<std::string> split_string(const std::string& str) {
  std::vector<std::string> strings;
  std::istringstream f(str);
  std::string s;
  while (getline(f, s, '.')) {
    strings.push_back(s);
  }
  return strings;
}
DEFINE_EXPR_STRUCT(SubUser, (int32_t)age, (std::string)id)
DEFINE_EXPR_STRUCT(User, (double)score, (std::string)id, (SubUser*)sub)
DEFINE_EXPR_STRUCT(SubItem, (int64_t)uv, (std::string)id)
DEFINE_EXPR_STRUCT(Item, (double)score, (std::string)id, (SubItem)sub)
DEFINE_EXPR_STRUCT(Combine, (User*)user, (Item*)item, (double)score, (std::string)name)

int64_t loop_count = 1000000;
double benchReflect(Combine& c) {
  double ret = 0;
  std::vector<std::string> names = split_string("score");
  std::vector<expr_struct::FieldAccessor> accessors;
  Combine::GetFieldAccessors(names, accessors);  // get access functions

  int64_t start_us = ustime();
  for (int i = 0; i < loop_count; i++) {
    auto val = c.GetFieldValue(accessors);  // eval by root object
    try {
      double v = GetValue<double, FieldValue>(val);
      ret = v;
    } catch (const std::bad_variant_access& e) {
      printf("###error:%s\n", e.what());
    }
  }
  int64_t end_us = ustime();
  printf("##benchReflect cost %lldus to loop count %lld\n", end_us - start_us, loop_count);
  return ret;
}

static void* getUser(void* c) {
  Combine* cc = (Combine*)c;
  return cc->user;
}
static double getScore(void* c) {
  Combine* u = (Combine*)c;
  return u->score;
}
double benchDirectAccess(Combine& c) {
  double ret = 0;
  int64_t start_us = ustime();

  auto getUserFunc = getUser;
  auto getScoreFunc = getScore;

  for (int i = 0; i < loop_count; i++) {
    auto val = getScoreFunc(&c);  // eval by root object
    ret = val;
  }
  int64_t end_us = ustime();
  printf("##benchDirectAccess cost %lldus to loop count %lld\n", end_us - start_us, loop_count);
  return ret;
}

int main() {
  /*
  **  static init root class
  */
  Combine::Init();
  /*
   **  get access functions by given field access expression,
   **  the functions can be reused later repeatedly
   */
  std::vector<std::string> names = split_string("user.score");
  std::vector<expr_struct::FieldAccessor> accessors;
  Combine::GetFieldAccessors(names, accessors);

  /*
  **  init root object & child object
  */
  SubUser su = {101, "random"};
  User user = {1.2, "uid123", &su};
  SubItem si = {123456789, "subitem"};
  Item item = {3.14, "item100", {123456789, "subitem"}};
  Combine c = {&user, &item, 100.2, "combine"};

  /*
  ** eval access functors by given root object to get final result
  */
  auto val = c.GetFieldValue(accessors);
  try {
    double v = GetValue<double, FieldValue>(val);
    printf("user.score = %.2f\n", v);
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("user.id");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    std::string v = GetValue<std::string, FieldValue>(val);
    printf("user.id = %s\n", v.c_str());
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("item.score");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    double v = GetValue<double, FieldValue>(val);
    printf("item.score = %.2f\n", v);
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("item.id");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    std::string v = GetValue<std::string, FieldValue>(val);
    printf("item.id = %s\n", v.c_str());
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("score");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    double v = GetValue<double, FieldValue>(val);
    printf("score = %.2f\n", v);
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("name");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    std::string v = GetValue<std::string, FieldValue>(val);
    printf("name = %s\n", v.c_str());
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("user.sub.age");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    int32_t v = GetValue<int32_t, FieldValue>(val);
    printf("user.sub.age = %d\n", v);
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  names = split_string("item.sub.uv");
  Combine::GetFieldAccessors(names, accessors);  // get access functions
  val = c.GetFieldValue(accessors);              // eval by root object
  try {
    int64_t v = GetValue<int64_t, FieldValue>(val);
    printf("item.sub.uv = %d\n", v);
  } catch (const std::bad_variant_access& e) {
    printf("###error:%s\n", e.what());
    return -1;
  }

  benchReflect(c);
  benchDirectAccess(c);
  return 0;
}
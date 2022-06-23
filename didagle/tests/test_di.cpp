// Copyright (c) 2021, Tencent Inc.
// All rights reserved.

#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include "didagle/di_container.h"
using namespace didagle;
struct TestPOD {
  int a = 101;
};

TEST(DIContainer, POD) {
  DIContainer::RegisterBuilder<TestPOD>("pod", std::make_unique<DIObjectBuilder<TestPOD>>([]() {
                                          static TestPOD t;
                                          t.a = 202;
                                          return &t;
                                        }));
  DIContainer::Init();

  const TestPOD* p = DIContainer::Get<TestPOD>("pod");
  EXPECT_EQ(202, p->a);
}

TEST(DIContainer, SharedPtr) {
  typedef std::shared_ptr<std::string> StringPtr;
  DIContainer::RegisterBuilder<StringPtr>("test_id", std::make_unique<DIObjectBuilder<StringPtr>>([]() {
                                            StringPtr id(new std::string("hello"));
                                            return id;
                                          }));
  DIContainer::Init();

  StringPtr p = DIContainer::Get<StringPtr>("test_id");
  EXPECT_EQ("hello", *p);
}

TEST(DIContainer, UniquePtr) {
  typedef std::unique_ptr<std::string> StringPtr;
  DIContainer::RegisterBuilder<StringPtr>("test_uniq_id", std::make_unique<DIObjectBuilder<StringPtr>>([]() {
                                            StringPtr id(new std::string("world"));
                                            return std::move(id);
                                          }));
  DIContainer::Init();

  StringPtr p = DIContainer::Get<StringPtr>("test_uniq_id");
  EXPECT_EQ("world", *p);
}

struct TestPOD2 {
  int a = 101;
  std::string id = "test";
};

struct TestPOD2Builder : public DIObjectBuilder<TestPOD2> {
  TestPOD2 instance;
  int Init() override {
    instance.a = 10000;
    instance.id = "new_instance";
    return 0;
  }
  const TestPOD2* Get() override { return &instance; }
};

TEST(DIContainer, CustomBuilderClass) {
  DIContainer::RegisterBuilder<TestPOD2>("test_pod2", std::make_unique<TestPOD2Builder>());
  DIContainer::Init();

  const TestPOD2* p = DIContainer::Get<TestPOD2>("test_pod2");
  EXPECT_EQ(10000, p->a);
  EXPECT_EQ("new_instance", p->id);
}

struct TestPOD3 {
  int a = 101;
  std::string id = "test";
};

struct TestPOD4 : public DIObject {
  DI_DEP(TestPOD3, pod3)
  DI_DEP(std::shared_ptr<TestPOD3>, pod33)
};

TEST(DIContainer, DepInject) {
  DIContainer::RegisterBuilder<TestPOD3>("pod3", std::make_unique<DIObjectBuilder<TestPOD3>>([]() {
                                           static TestPOD3 pod;
                                           pod.a = 202;
                                           pod.id = "notest";
                                           return &pod;
                                         }));
  DIContainer::RegisterBuilder<std::shared_ptr<TestPOD3>>(
      "pod33", std::make_unique<DIObjectBuilder<std::shared_ptr<TestPOD3>>>([]() {
        std::shared_ptr<TestPOD3> p(new TestPOD3);
        p->a = 303;
        p->id = "sptest";
        return p;
      }));
  DIContainer::RegisterBuilder<TestPOD4>("pod4", std::make_unique<DIObjectBuilder<TestPOD4>>([]() {
                                           static TestPOD4 pod;
                                           return &pod;
                                         }));
  DIContainer::Init();

  const TestPOD4* p = DIContainer::Get<TestPOD4>("pod4");
  EXPECT_EQ(202, p->pod3->a);
  EXPECT_EQ("notest", p->pod3->id);
  EXPECT_EQ(303, p->pod33->a);
  EXPECT_EQ("sptest", p->pod33->id);
}

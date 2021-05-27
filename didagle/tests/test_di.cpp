#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include "di_container.h"
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
  DIContainer::RegisterBuilder<StringPtr>("test_id",
                                          std::make_unique<DIObjectBuilder<StringPtr>>([]() {
                                            StringPtr id(new std::string("hello"));
                                            return id;
                                          }));
  DIContainer::Init();

  StringPtr p = DIContainer::Get<StringPtr>("test_id");
  EXPECT_EQ("hello", *p);
}

TEST(DIContainer, UniquePtr) {
  typedef std::unique_ptr<std::string> StringPtr;
  DIContainer::RegisterBuilder<StringPtr>("test_uniq_id",
                                          std::make_unique<DIObjectBuilder<StringPtr>>([]() {
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

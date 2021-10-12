/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
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
 *  * Neither the name of rimos nor the names of its contributors may be used
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
#include <fmt/core.h>
#include <google/protobuf/util/json_util.h>
#include <boost/algorithm/string/replace.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "folly/Random.h"
#include "robims_cache.h"
#include "robims_db.h"
#include "robims_log.h"
#include "user.pb.h"

using namespace robims;

static std::vector<test::User> g_test_users(128);
static std::unordered_map<std::string, test::User*> g_user_table;
static void testSave() {
  RobimsDB db;
  TableSchema table0;
  table0.set_name("test");
  table0.set_id_field("id");
  auto field0 = table0.add_index_field();
  field0->set_name("age");
  field0->set_index_type(INT_INDEX);
  auto field1 = table0.add_index_field();
  field1->set_name("city");
  field1->set_index_type(SET_INDEX);
  // auto field2 = table0.add_index_field();
  // field2->set_name("f2");
  // field2->set_index_type(WEIGHT_SET);
  auto field3 = table0.add_index_field();
  field3->set_name("score");
  field3->set_index_type(robims::FLOAT_INDEX);
  auto field4 = table0.add_index_field();
  field4->set_name("gender");
  field4->set_index_type(robims::MUTEX_INDEX);
  auto field5 = table0.add_index_field();
  field5->set_name("is_child");
  field5->set_index_type(robims::BOOL_INDEX);
  int rc = db.CreateTable(table0);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to create table:{}", rc);
  }

  std::vector<std::string> cities = {"sz", "bj", "sh", "nj", "wh"};

  for (uint32_t i = 0; i < g_test_users.size(); i++) {
    test::User& user = g_test_users[i];
    user.set_id(i + 1);
    user.set_name("u" + std::to_string(i));
    user.set_age(folly::Random::secureRand32(120));
    user.set_score(folly::Random::secureRandDouble(20, 100));
    user.add_city(cities[folly::Random::secureRand32(cities.size())]);
    user.set_gender(folly::Random::secureRand32(2) == 0 ? "male" : "female");
    user.set_is_child(folly::Random::secureRand32(2) == 1);
    g_user_table[std::to_string(user.id())] = &user;

    google::protobuf::util::JsonPrintOptions opt;
    opt.add_whitespace = true;
    opt.preserve_proto_field_names = true;
    std::string json;
    google::protobuf::util::MessageToJsonString(user, &json, opt);
    rc = db.Put("test", json);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to put json with rc:{}", rc);
    }
  }
  std::string query = "test.age>50 && test.score>60 && test.city == \"sz\"";
  std::vector<std::string> ids;
  rc = db.Select(query, ids);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to select with rc:{}", rc);
    return;
  }
  ROBIMS_INFO("Result size:{}", ids.size());
  for (const auto& id : ids) {
    auto found = g_user_table.find(id);
    if (found == g_user_table.end()) {
      ROBIMS_ERROR("No user found for id:{}", id);
    } else {
      ROBIMS_INFO("{}", found->second->DebugString());
    }
  }

  rc = db.Save("./robims.save", true);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save robims:{}", rc);
  }
}

static void testLoad() {
  RobimsDB db;
  int rc = db.Load("./robims.save");
  if (0 != rc) {
    ROBIMS_ERROR("Failed to load robims", rc);
    return;
  }
  std::string query = "test.age>50 && test.score>60 && test.city == \"sz\"";
  std::vector<std::string> ids;
  rc = db.Select(query, ids);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to select with rc:{}", rc);
    return;
  }
  ROBIMS_INFO("Result size:{}", ids.size());
  for (const auto& id : ids) {
    auto found = g_user_table.find(id);
    if (found == g_user_table.end()) {
      ROBIMS_ERROR("No user found for id:{}", id);
    } else {
      ROBIMS_INFO("{}", found->second->DebugString());
    }
  }
}

static void testQuery() {
  RobimsDB db;
  int rc = db.Load("./robims.save");
  if (0 != rc) {
    ROBIMS_ERROR("Failed to load robims", rc);
    return;
  }
  std::string query = "test.is_child==1 && test.gender!=\"male\"";
  std::vector<std::string> ids;
  rc = db.Select(query, ids);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to select with rc:{}", rc);
    return;
  }
  ROBIMS_INFO("Result size:{}", ids.size());
  for (const auto& id : ids) {
    auto found = g_user_table.find(id);
    if (found == g_user_table.end()) {
      ROBIMS_ERROR("No user found for id:{}", id);
    } else {
      ROBIMS_INFO("{}", found->second->DebugString());
    }
  }
}

int main() {
  testSave();
  ROBIMS_INFO("==================================");
  testLoad();
  testQuery();
  clear_bitmap_cache();
  return 0;
}

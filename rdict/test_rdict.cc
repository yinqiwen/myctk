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
#include <gtest/gtest.h>
#include <string_view>
#include "rdict/rdict.h"

TEST(Rdict, simple_ints) {
  rdict::ReadonlyDict<uint64_t, uint64_t>::Options opts;
  opts.readonly = false;
  opts.path_prefix = "./test_rdict";
  opts.bucket_count = 1300000;
  auto result = rdict::ReadonlyDict<uint64_t, uint64_t>::New(opts);
  auto dict = std::move(result.value());
  for (uint64_t i = 1; i < 1000000; i++) {
    dict->Put(i, i + 100);
  }
  dict->Commit();

  opts.readonly = true;
  auto result1 = rdict::ReadonlyDict<uint64_t, uint64_t>::New(opts);
  auto dict1 = std::move(result1.value());
  for (uint64_t i = 1; i < 1000000; i++) {
    auto val = dict1->Get(i);
    ASSERT_EQ(val.value(), i + 100);
  }

  rdict::ReadonlyDict<uint64_t, uint64_t>::Options opts1;
  opts1.readonly = false;
  opts1.path_prefix = "./test_rdict1";
  opts1.bucket_count = 1300000;
  auto other_result = rdict::ReadonlyDict<uint64_t, uint64_t>::New(opts1);
  auto other_dict = std::move(other_result.value());
  for (uint64_t i = 1; i < 1000000; i++) {
    other_dict->Put(i + 1000000, i + 100);
  }
  other_dict->Merge(*dict1);
  other_dict->Commit();

  for (uint64_t i = 1; i < 1000000; i++) {
    auto val = other_dict->Get(i);
    ASSERT_EQ(val.value(), i + 100);
    val = other_dict->Get(i + 1000000);
    ASSERT_EQ(val.value(), i + 100);
  }
}

TEST(Rdict, simple_strs) {
  uint64_t test_count = 1000000;
  rdict::ReadonlyDict<uint64_t, std::string_view>::Options opts;
  opts.readonly = false;
  opts.path_prefix = "./test_str_rdict";
  opts.bucket_count = 1300000;
  auto result = rdict::ReadonlyDict<uint64_t, std::string_view>::New(opts);
  auto dict = std::move(result.value());
  std::string data = "hello,world";
  data.resize(256);
  for (uint64_t i = 1; i < test_count; i++) {
    std::string_view str = data;
    dict->Put(i, str);
  }
  dict->Commit();
  dict.reset();

  opts.readonly = true;
  auto result1 = rdict::ReadonlyDict<uint64_t, std::string_view>::New(opts);
  auto dict1 = std::move(result1.value());
  for (uint64_t i = 1; i < test_count; i++) {
    auto val = dict1->Get(i);
    ASSERT_EQ(val.value(), data);
  }

  rdict::ReadonlyDict<uint64_t, std::string_view>::Options opts1;
  opts1.readonly = false;
  opts1.path_prefix = "./test_str_rdict1";
  opts1.bucket_count = 1300000;
  auto other_result = rdict::ReadonlyDict<uint64_t, std::string_view>::New(opts1);
  auto other_dict = std::move(other_result.value());
  for (uint64_t i = 1; i < test_count; i++) {
    other_dict->Put(i + 1000000, data);
  }
  other_dict->Merge(*dict1);
  other_dict->Commit();

  for (uint64_t i = 1; i < test_count; i++) {
    auto val = other_dict->Get(i);
    ASSERT_EQ(val.value(), data);
    val = other_dict->Get(i + 1000000);
    ASSERT_EQ(val.value(), data);
  }
}
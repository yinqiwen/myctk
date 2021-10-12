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
#include "robims_simple_id_mapping.h"
#include <arpa/inet.h>
#include <string.h>
#include <memory>
#include <string_view>
#include "robims_common.h"
#include "robims_log.h"

namespace robims {
SimpleIDMapping::SimpleIDMapping() : id_seed_(0) {}
int SimpleIDMapping::Save(FILE* fp, bool readonly) {
  int rc = file_write_uint32(fp, id_seed_);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save id_seed");
    return rc;
  }
  uint32_t n = real_local_mapping_.size();
  rc = file_write_uint32(fp, n);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save mapping count");
    return rc;
  }
  for (auto& [id, pair] : real_local_mapping_) {
    rc = file_write_uint32(fp, pair->first);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to save local id");
      return rc;
    }
    rc = file_write_string(fp, pair->second);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to save real id");
      return rc;
    }
  }
  return 0;
}
int SimpleIDMapping::Load(FILE* fp) {
  int rc = file_read_uint32(fp, id_seed_);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to read id_seed");
    return rc;
  }
  uint32_t n = 0;
  rc = file_read_uint32(fp, n);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to read mapping count");
    return rc;
  }
  for (uint32_t i = 0; i < n; i++) {
    uint32_t local_id;
    std::string real_id;
    rc = file_read_uint32(fp, local_id);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to read local id");
      return rc;
    }
    rc = file_read_string(fp, real_id);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to read real id");
      return rc;
    }
    auto id_pair = std::make_unique<IDPair>();
    id_pair->first = local_id;
    id_pair->second = std::move(real_id);
    std::string_view real_id_view = id_pair->second;
    real_local_mapping_[real_id_view] = std::move(id_pair);
    local_real_mapping_[local_id] = real_id_view;
  }
  return 0;
}
int SimpleIDMapping::GetLocalID(const std::string_view& id, bool create_ifnotexist,
                                uint32_t& local_id) {
  auto found = real_local_mapping_.find(id);
  if (found != real_local_mapping_.end()) {
    local_id = found->second->first;
    return 0;
  } else {
    if (!create_ifnotexist) {
      return -1;
    }
  }
  local_id = id_seed_;
  id_seed_++;
  auto id_pair = std::make_unique<IDPair>();
  id_pair->first = local_id;
  std::string real_id(id.data(), id.size());
  id_pair->second = std::move(real_id);
  std::string_view real_id_view = id_pair->second;
  real_local_mapping_[real_id_view] = std::move(id_pair);
  local_real_mapping_[local_id] = real_id_view;
  return 0;
}
int SimpleIDMapping::GetID(uint32_t local_id, std::string_view& id) {
  auto found = local_real_mapping_.find(local_id);
  if (found != local_real_mapping_.end()) {
    id = found->second;
    return 0;
  }

  return -1;
}
}  // namespace robims
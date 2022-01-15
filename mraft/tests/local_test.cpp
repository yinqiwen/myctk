/*
 *Copyright (c) 2022, qiyingwang <qiyingwang@tencent.com>
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
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "folly/Conv.h"
#include "folly/String.h"
#include "mraft/logger.h"
#include "mraft/tests/local_raft.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

using namespace mraft;

static std::map<int, std::shared_ptr<Raft>> g_rafts;
static std::vector<RaftNodePeer> g_raft_peers;
static int g_raft_node_count = 0;
static void add_node(int create_cluster) {
  RaftOptions opt;
  opt.name = "test_cluster";
  opt.home = "./test_cluster" + std::to_string(g_raft_node_count);
  opt.create_cluster = create_cluster;
  opt.self_peer.set_host("127.0.0.1");
  opt.self_peer.set_port(8000 + g_raft_node_count);
  bool add = true;
  for (const auto& peer : g_raft_peers) {
    if (peer.host() == opt.self_peer.host() && peer.port() == opt.self_peer.port()) {
      add = false;
      break;
    }
  }
  if (add) {
    g_raft_peers.push_back(opt.self_peer);
  }
  opt.raft_peers = g_raft_peers;

  std::shared_ptr<Raft> new_raft(new LocalRaft);
  int rc = new_raft->Init(opt);
  if (0 != rc) {
    printf("Failed yo init raft with counter:%d\n", g_raft_node_count);
    return;
  }
  printf("Success to create node %s:%d with id:%d\n", opt.self_peer.host().c_str(), opt.self_peer.port(),
         new_raft->GetNodeID());
  g_rafts[new_raft->GetNodeID()] = new_raft;
  g_raft_node_count++;
}

int main() {
  auto file_logger = spdlog::basic_logger_mt("file_logger", "./raft_test.log");
  spdlog::set_default_logger(file_logger);
  spdlog::set_pattern("%+");

  int init_node_count = 3;

  for (int i = 0; i < init_node_count; i++) {
    RaftNodePeer peer;
    peer.set_host("127.0.0.1");
    peer.set_port(8000 + i);
    g_raft_peers.emplace_back(peer);
  }

  for (int i = 0; i < init_node_count; i++) {
    add_node(true);
  }

  for (std::string line; std::getline(std::cin, line);) {
    auto cmd = folly::trimWhitespace(line);
    std::vector<folly::StringPiece> cmd_args;
    folly::split(" ", cmd, cmd_args);

    auto get_raft = [&cmd_args, &line]() -> Raft* {
      if (cmd_args.size() < 2) {
        std::cout << "invalid command:" << line << std::endl;
        return nullptr;
      }
      auto node_id = folly::tryTo<int>(cmd_args[1]);
      if (!node_id.hasValue()) {
        std::cout << "invalid command:" << line << std::endl;
        return nullptr;
      }
      return Raft::Get("test_cluster", node_id.value());
    };
    auto get_target_node_id = [&cmd_args, &line]() -> int {
      if (cmd_args.size() < 3) {
        std::cout << "invalid command:" << line << std::endl;
        return -1;
      }
      auto node_id = folly::tryTo<int>(cmd_args[2]);
      if (!node_id.hasValue()) {
        std::cout << "invalid command:" << line << std::endl;
        return -1;
      }
      return node_id.value();
    };

    if (cmd_args[0] == "hang") {
      auto remote = get_raft();
      if (!remote) {
        continue;
      }
      // remote->Hang();
    } else if (cmd_args[0] == "resume") {
      auto remote = get_raft();
      if (!remote) {
        continue;
      }
    } else if (cmd_args[0] == "remove") {
      auto remote = get_raft();
      if (!remote) {
        continue;
      }
      remote->Shutdown();
    } else if (cmd_args[0] == "info") {
      if (cmd_args.size() == 1) {
        for (auto& pair : g_rafts) {
          std::cout << pair.second->Info() << std::endl;
        }
      } else {
        auto remote = get_raft();
        if (!remote) {
          continue;
        }
        std::cout << remote->Info() << std::endl;
      }
    } else if (cmd_args[0] == "add") {
      auto remote = get_raft();
      if (!remote) {
        continue;
      }
      auto v = folly::tryTo<int64_t>(cmd_args[2]);
      if (!v.hasValue()) {
        std::cout << "invalid command:" << line << std::endl;
        continue;
      }
      dynamic_cast<LocalRaft*>(remote)->Operate('+', v.value());
    } else if (cmd_args[0] == "join") {
      add_node(false);
    } else if (cmd_args[0] == "transfer_leader") {
      auto remote = get_raft();
      if (!remote) {
        continue;
      }
      int target = get_target_node_id();
      if (target < 0) {
        continue;
      }
      remote->TransferLeader(target, 0,
                             [](int rc) { printf("TransferLeader finish with：%s\n", Raft::GetErrString(rc).data()); });
    } else {
      std::cout << "unknown command:" << line << std::endl;
      continue;
    }
  }
  return 0;
}
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
#include <optional>
#include <string>
#include <vector>
#include "folly/Conv.h"
#include "folly/String.h"
#include "linenoise.hpp"
#include "mraft/logger.h"
#include "mraft/tests/local_raft.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

using namespace mraft;

static std::map<int, std::shared_ptr<Raft>> g_rafts;
static std::vector<RaftNodePeer> g_raft_peers;
static void start_node(bool create_cluster, int node_idx) {
  RaftOptions opt;
  opt.name = "test_cluster";
  opt.home = "./test_cluster" + std::to_string(node_idx);
  opt.create_cluster = create_cluster;
  opt.self_peer.set_host("127.0.0.1");
  opt.self_peer.set_port(8000 + node_idx);
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
    printf("Failed yo init raft with cursor:%d\n", node_idx);
    return;
  }
  printf("Success to start node %s:%d with id:%d\n", opt.self_peer.host().c_str(), opt.self_peer.port(),
         new_raft->GetNodeID());
  g_rafts[new_raft->GetNodeID()] = new_raft;
}

static int stop_node(int node_id) {
  auto raft = Raft::Get("test_cluster", node_id);
  if (!raft) {
    std::cout << "No node found for node_id:" << node_id << std::endl;
    return -1;
  }
  raft->Shutdown();
  g_rafts.erase(node_id);
  std::cout << "Stop raft node with node_id:" << node_id << std::endl;
  return 0;
}

static int remove_node(int node_id) {
  if (0 != stop_node(node_id)) {
    return -1;
  }
  for (auto& pair : g_rafts) {
    auto raft = pair.second;
    if (raft->IsLeader()) {
      RaftNodePeer remove_peer;
      remove_peer.set_id(node_id);
      raft->RemoveNode(remove_peer, [node_id](int rc) { printf("Remove Node:%d complete with rc:%d\n", node_id, rc); });
      break;
    }
  }
  return 0;
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
    start_node(true, i);
  }

  const auto path = "history.txt";
  // Setup completion words every time when a user types
  linenoise::SetCompletionCallback([](const char* editBuffer, std::vector<std::string>& completions) {
    if (editBuffer[0] == 'h') {
      completions.push_back("hello");
      completions.push_back("hello there");
    }
  });

  // Enable the multi-line mode
  linenoise::SetMultiLine(true);
  // Set max length of the history
  linenoise::SetHistoryMaxLen(10);
  // Load history
  linenoise::LoadHistory(path);

  while (true) {
    // Read line
    std::string line;
    auto quit = linenoise::Readline("raft> ", line);
    if (quit) {
      break;
    }
    auto cmd = folly::trimWhitespace(line);
    std::vector<folly::StringPiece> cmd_args;
    folly::split(" ", cmd, cmd_args);
    std::optional<int> node_id;
    if (cmd_args.size() >= 2) {
      auto v = folly::tryTo<int>(cmd_args[1]);
      if (!v.hasValue()) {
        std::cout << "invalid command:" << line << std::endl;
      } else {
        node_id = v.value();
      }
    }

    if (cmd_args[0] == "start") {
      if (!node_id.has_value()) {
        continue;
      }
      start_node(false, node_id.value());
    } else if (cmd_args[0] == "stop") {
      if (!node_id.has_value()) {
        continue;
      }
      stop_node(node_id.value());
    } else if (cmd_args[0] == "remove") {
      if (!node_id.has_value()) {
        continue;
      }
      remove_node(node_id.value());
    } else if (cmd_args[0] == "info") {
      for (auto& pair : g_rafts) {
        std::cout << pair.second->Info() << std::endl;
      }
    } else if (cmd_args[0] == "transfer_leader") {
      if (!node_id.has_value()) {
        continue;
      }
      for (auto& pair : g_rafts) {
        auto raft = pair.second;
        if (raft->IsLeader()) {
          raft->TransferLeader(node_id.value(), 0, [](int rc) {
            printf("TransferLeader finish with：%s\n", Raft::GetErrString(rc).data());
          });
          break;
        }
      }
    }

    // Add text to history
    linenoise::AddHistory(line.c_str());
  }

  // Save history
  linenoise::SaveHistory(path);
  g_rafts.clear();

  return 0;
}
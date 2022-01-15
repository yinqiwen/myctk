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
#include "mraft.h"

#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "fmt/core.h"
#include "folly/container/F14Map.h"
#include "folly/portability/Filesystem.h"
#include "folly/synchronization/Latch.h"
#include "mraft/logger.h"
#include "mraft/segment_log_storage.h"
#include "mraft/utils.h"

namespace mraft {
static constexpr std::string_view kRaftMeta = "mraft_meta";
static constexpr std::string_view kLogDir = "log";
static constexpr std::string_view kSnapshotDir = "snapshot";
static constexpr std::string_view kSnapshotMeta = "snapshot_meta";
static constexpr uint32_t kMaxHostLen = 32;

struct RaftIDHash {
  size_t operator()(const RaftID& id) const noexcept {
    size_t v = std::hash<std::string_view>{}(id.cluster);
    v = v ^ std::hash<std::string_view>{}(id.cluster);
    v = v ^ static_cast<size_t>(id.id);
    return v;
  }
};
struct RaftIDEqual {
  bool operator()(const RaftID& x, const RaftID& y) const { return x.cluster == y.cluster && x.id == y.id; }
};

using RaftTable = folly::F14FastMap<RaftID, Raft*, RaftIDHash, RaftIDEqual>;
static RaftTable* g_raft_table = nullptr;
static std::mutex g_raft_table_mutex;

static bool contains_raft(const std::string& name, int id) {
  RaftID rid = {name, id};
  std::lock_guard<std::mutex> guard(g_raft_table_mutex);
  if (nullptr == g_raft_table) {
    return false;
  }
  auto ret = g_raft_table->find(rid);
  if (ret != g_raft_table->end()) {
    return true;
  }
  return false;
}

static bool try_emplace_raft(Raft* raft) {
  std::lock_guard<std::mutex> guard(g_raft_table_mutex);
  if (nullptr == g_raft_table) {
    g_raft_table = new RaftTable;
  }
  RaftID rid = {raft->Name(), raft->GetNodeID()};
  auto ret = g_raft_table->emplace(rid, raft);
  return ret.second;
}

static void clear_raft(const std::string& name, int id) {
  std::lock_guard<std::mutex> guard(g_raft_table_mutex);
  if (nullptr == g_raft_table) {
    return;
  }
  RaftID rid = {name, id};
  auto ret = g_raft_table->find(rid);
  if (ret != g_raft_table->end()) {
    g_raft_table->erase(ret);
  }
}

static Raft* get_raft(const std::string_view& name, int id) {
  std::lock_guard<std::mutex> guard(g_raft_table_mutex);
  if (nullptr == g_raft_table) {
    return nullptr;
  }
  RaftID rid = {name, id};
  auto ret = g_raft_table->find(rid);
  if (ret == g_raft_table->end()) {
    return nullptr;
  }
  return ret->second;
}

struct RaftCfgChange {
  int32_t node_id = -1;
  uint16_t port = 0;
  char host[kMaxHostLen];
};

bool operator==(const RaftNodePeer& x, const RaftNodePeer& y) { return x.host() == y.host() && x.port() == y.port(); }

static inline uint64_t ustime() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((uint64_t)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

std::string_view Raft::GetErrString(int err) {
  switch (err) {
    case RAFT_ERR_OK: {
      return "OK";
    }
    case RAFT_ERR_INVALID_STATE: {
      return "RAFT_ERR_INVALID_STATE";
    }
    case RAFT_ERR_NOT_LEADER: {
      return "RAFT_ERR_NOT_LEADER";
    }
    case RAFT_ERR_ONE_VOTING_CHANGE_ONLY: {
      return "RAFT_ERR_ONE_VOTING_CHANGE_ONLY";
    }
    case RAFT_ERR_SHUTDOWN: {
      return "RAFT_ERR_SHUTDOWN";
    }
    case RAFT_ERR_NOMEM: {
      return "RAFT_ERR_NOMEM";
    }
    case RAFT_ERR_SNAPSHOT_IN_PROGRESS: {
      return "RAFT_ERR_SNAPSHOT_IN_PROGRESS";
    }
    case RAFT_ERR_SNAPSHOT_ALREADY_LOADED: {
      return "RAFT_ERR_SNAPSHOT_ALREADY_LOADED";
    }
    case RAFT_ERR_INVALID_NODEID: {
      return "RAFT_ERR_INVALID_NODEID";
    }
    case RAFT_ERR_LEADER_TRANSFER_IN_PROGRESS: {
      return "RAFT_ERR_LEADER_TRANSFER_IN_PROGRESS";
    }
    case RAFT_ERR_DONE: {
      return "RAFT_ERR_DONE";
    }
    case RAFT_ERR_STALE_TERM: {
      return "RAFT_ERR_STALE_TERM";
    }
    case RAFT_ERR_LAST: {
      return "RAFT_ERR_STALE_TERM";
    }
    case RAFT_ERR_RPC_TIMEOUT: {
      return "RAFT_ERR_RPC_TIMEOUT";
    }
    default: {
      return "unknown raft error";
    }
  }
}

Raft* Raft::Get(std::string_view name, int id) { return get_raft(name, id); }

int Raft::GetNodeIdByPeer(const RaftNodePeer& peer) {
  std::lock_guard<std::mutex> guard(g_raft_table_mutex);
  if (nullptr == g_raft_table) {
    return -1;
  }
  for (auto& pair : *g_raft_table) {
    if (pair.second->GetPeer() == peer) {
      return pair.second->GetPeer().id();
    }
  }
  return -1;
}

int Raft::RaftSendRequestVote(raft_server_t* raft, void* user_data, raft_node_t* node, msg_requestvote_t* m) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  raft_node_id_t node_id = raft_node_get_id(node);
  MRAFT_INFO("[Node[{}]RaftSendRequestVote to {}", r->GetNodeID(), node_id);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    return -1;
  }
  RequestVoteRequest msg;
  msg.set_prevote(m->prevote);
  msg.set_term(m->term);
  msg.set_candidate_id(m->candidate_id);
  msg.set_last_log_idx(m->last_log_idx);
  msg.set_last_log_term(m->last_log_term);
  msg.set_transfer_leader(m->transfer_leader);
  return r->RequestVote(peer, msg);
}
int Raft::RaftSendAppendEntries(raft_server_t* raft, void* user_data, raft_node_t* node, msg_appendentries_t* m) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  raft_node_id_t node_id = raft_node_get_id(node);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    return -1;
  }
  AppendEntriesRequest request;
  request.set_leader_id(m->leader_id);
  request.set_msg_id(m->msg_id);
  request.set_term(m->term);
  request.set_prev_log_idx(m->prev_log_idx);
  request.set_prev_log_term(m->prev_log_term);
  request.set_leader_commit(m->leader_commit);
  for (int i = 0; i < m->n_entries; i++) {
    auto entry = request.add_entries();
    entry->set_term(m->entries[i]->term);
    entry->set_id(m->entries[i]->id);
    entry->set_type(m->entries[i]->type);
    if (m->entries[i]->data_len > 0) {
      entry->set_data(m->entries[i]->data, m->entries[i]->data_len);
    }
  }
  return r->AppendEntries(peer, request);
}
int Raft::RaftPersistVote(raft_server_t* raft, void* user_data, raft_node_id_t vote) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  return r->PersistVote(vote);
}
int Raft::RaftPersistTerm(raft_server_t* raft, void* user_data, raft_term_t term, raft_node_id_t vote) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  return r->PersistTerm(term, vote);
}
int Raft::RaftApplyLog(raft_server_t* raft, void* user_data, raft_entry_t* entry, raft_index_t entry_idx) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  r->meta_.set_last_applied_idx(entry_idx);
  r->meta_.set_last_applied_term(entry->term);
  int rc = 0;
  switch (entry->type) {
    case RAFT_LOGTYPE_REMOVE_NODE: {
      break;
    }
    case RAFT_LOGTYPE_ADD_NONVOTING_NODE: {
    }
    case RAFT_LOGTYPE_ADD_NODE: {
      RaftCfgChange* req = reinterpret_cast<RaftCfgChange*>(entry->data);
      if (r->meta_.node_peers().count(req->node_id) == 0) {
        RaftNodePeer peer;
        peer.set_host(req->host);
        peer.set_port(req->port);
        peer.set_id(req->node_id);
        r->meta_.mutable_node_peers()->insert({req->node_id, peer});
      }
      break;
    }
    case RAFT_LOGTYPE_NO_OP: {
      break;
    }
    case RAFT_LOGTYPE_NORMAL: {
      RaftEntryClosure* closure = reinterpret_cast<RaftEntryClosure*>(entry->user_data);
      if (nullptr != closure) {
        entry->user_data = nullptr;
        int rc = r->OnApplyLog(LogEntry::Wrap(entry, false), closure);
        closure->Run(static_cast<RaftErrorCode>(rc));
      } else {
        rc = r->OnApplyLog(LogEntry::Wrap(entry, false), closure);
      }
    }
    default:
      break;
  }
  r->raft_log_store_->ClearCacheEntry(entry_idx);
  return rc;
}
void Raft::RaftLog(raft_server_t* raft, raft_node_id_t node, void* user_data, const char* buf) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  MRAFT_INFO("[Node[{}]{}", r->GetNodeID(), buf);
}
raft_node_id_t Raft::RaftLogGetNodeId(raft_server_t* raft, void* user_data, raft_entry_t* entry,
                                      raft_index_t entry_idx) {
  RaftCfgChange* req = reinterpret_cast<RaftCfgChange*>(entry->data);
  return req->node_id;
}
int Raft::RaftNodeHasSufficientLogs(raft_server_t* raft, void* user_data, raft_node_t* raft_node) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  raft_node_id_t node_id = raft_node_get_id(raft_node);
  MRAFT_DEBUG("[{}]RaftNodeHasSufficientLogs ", node_id);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    return -1;
  }
  return r->AddNodeHasSufficientLogs(peer);
}

void Raft::RaftNotifyMembershipEvent(raft_server_t* raft, void* user_data, raft_node_t* raft_node, raft_entry_t* entry,
                                     raft_membership_e type) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  raft_node_id_t node_id = raft_node_get_id(raft_node);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    return;
  }
  switch (type) {
    case RAFT_MEMBERSHIP_ADD: {
      MRAFT_INFO("Node {}/{}:{} Add", node_id, peer.host(), peer.port());
      raft_node_set_udata(raft_node, r);
      break;
    }
    case RAFT_MEMBERSHIP_REMOVE: {
      MRAFT_INFO("Node {}/{}:{}Remove", node_id, peer.host(), peer.port());
      break;
    }
    default: {
      break;
    }
  }
}
void Raft::RaftNotifyStateEvent(raft_server_t* raft, void* user_data, raft_state_e state) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  switch (state) {
    case RAFT_STATE_FOLLOWER: {
      MRAFT_INFO("Node:{} State change: node is now a follower, term {}", r->SelfNode(), raft_get_current_term(raft));
      break;
    }
    case RAFT_STATE_PRECANDIDATE: {
      MRAFT_INFO("Node:{} State change: Election starting, node is now a pre-candidate, term {}", r->SelfNode(),
                 raft_get_current_term(raft));
      break;
    }
    case RAFT_STATE_CANDIDATE: {
      MRAFT_INFO("Node:{} State change: Node is now a candidate, term {}", r->SelfNode(), raft_get_current_term(raft));
      break;
    }
    case RAFT_STATE_LEADER: {
      MRAFT_INFO("Node:{} State change: Node is now a leader, term {}", r->SelfNode(), raft_get_current_term(raft));
      break;
    }
    default: {
      break;
    }
  }
}
int Raft::RaftSendTimeoutNow(raft_server_t* raft, raft_node_t* raft_node) {
  Raft* r = reinterpret_cast<Raft*>(raft_node_get_udata(raft_node));
  raft_node_id_t node_id = raft_node_get_id(raft_node);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    MRAFT_WARN("Can NOT find node by id:{}", node_id);
    return -1;
  }
  return r->TimeoutNow(peer);
}
void Raft::RaftNotifyTransferEvent(raft_server_t* raft, void* user_data, raft_transfer_state_e state) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  r->HandleTransferLeaderComplete(state);
}

int Raft::RaftSendSnapshot(raft_server_t* raft, void* user_data, raft_node_t* node, msg_snapshot_t* msg) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  // Snapshot request;
  raft_node_id_t node_id = raft_node_get_id(node);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    MRAFT_WARN("Can NOT find node by id:{}", node_id);
    return -1;
  }
  return r->SendSnapshot(peer, msg);
}
int Raft::RaftLoadSnapshot(raft_server_t* raft, void* user_data, raft_index_t snapshot_index,
                           raft_term_t snapshot_term) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  return r->RaftLoadSnapshot(snapshot_term, snapshot_index);
}
int Raft::RaftGetSnapshotChunk(raft_server_t* raft, void* user_data, raft_node_t* node, raft_size_t offset,
                               raft_snapshot_chunk_t* chunk) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  raft_node_id_t node_id = raft_node_get_id(node);
  RaftNodePeer peer;
  if (0 != r->GetNodePeerById(node_id, peer)) {
    return -1;
  }
  return r->GetSnapshotChunk(peer, offset, chunk);
}
int Raft::RaftStoreSnapshotChunk(raft_server_t* raft, void* user_data, raft_index_t snapshot_index, raft_size_t offset,
                                 raft_snapshot_chunk_t* chunk) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  return r->StoreSnapshotChunk(snapshot_index, offset, chunk);
}
int Raft::RaftClearSnapshot(raft_server_t* raft, void* user_data) {
  Raft* r = reinterpret_cast<Raft*>(user_data);
  return r->ClearInComingSnapshot();
}

Raft::Raft() {}
Raft::~Raft() { Shutdown(); }

int Raft::Shutdown() {
  state_ = RaftState::RAFT_SHUTDOWN;
  if (nullptr != raft_server_) {
    MRAFT_INFO("shutdown raft:{}", GetNodeID());
    clear_raft(options_.name, GetNodeID());
    if (event_base_thread_) {
      event_base_.runInEventBaseThreadAndWait([this] {
        folly::HHWheelTimer& t = event_base_.timer();
        t.cancelAll();
      });
      event_base_.terminateLoopSoon();
      event_base_thread_->join();
    }
    raft_destroy(raft_server_);
    raft_server_ = nullptr;
  }
  return 0;
}

int Raft::GetNodeID() {
  if (id_ <= 0) {
    id_ = raft_get_nodeid(raft_server_);
  }
  return id_;
}

std::string Raft::SelfNode() {
  return fmt::format("[{}][{}:{}]", options_.self_peer.id(), options_.self_peer.host(), options_.self_peer.port());
}
std::string Raft::Info() {
  std::string s;
  s.append(fmt::format("node_id:{},num_nodes:{},num_voting_nodes:{},", raft_get_nodeid(raft_server_),
                       raft_get_num_nodes(raft_server_), raft_get_num_voting_nodes(raft_server_)));
  s.append(fmt::format("log_count:{},current_term:{},current_idx:{},", raft_get_log_count(raft_server_),
                       raft_get_current_term(raft_server_), raft_get_current_idx(raft_server_)));
  s.append(fmt::format("commit_idx:{},is_follower:{},is_leader:{},", raft_get_commit_idx(raft_server_),
                       raft_is_follower(raft_server_), raft_is_leader(raft_server_)));
  s.append(fmt::format("is_precandidate:{},is_candidate:{},last_applied_idx:{},", raft_is_precandidate(raft_server_),
                       raft_is_candidate(raft_server_), raft_get_last_applied_idx(raft_server_)));
  s.append(fmt::format("nvotes_for_me:{},voted_for:{},leader_id:{}", raft_get_nvotes_for_me(raft_server_),
                       raft_get_voted_for(raft_server_), raft_get_leader_id(raft_server_)));
  return s;
}

void Raft::GetClusterNodes(std::vector<RaftNodePeer>& peers) {
  for (const auto& pair : meta_.node_peers()) {
    peers.push_back(pair.second);
  }
}

bool Raft::CheckRaftState() {
  switch (state_.load()) {
    case RaftState::RAFT_UNINITIALIZED: {
      MRAFT_ERROR("No Raft Cluster");
      return false;
    }
    case RaftState::RAFT_JOINING: {
      MRAFT_ERROR("No Raft Cluster (joining now)");
      return false;
    }
    case RaftState::RAFT_LOADING: {
      MRAFT_ERROR("Raft is loading data");
      return false;
    }
    case RaftState::RAFT_UP: {
      return true;
    }
    case RaftState::RAFT_SHUTDOWN: {
      MRAFT_ERROR("Raft is shutdown");
      return false;
    }
    default: {
      MRAFT_ERROR("Unknown state:{}", state_.load());
      return false;
    }
  }
}

void Raft::ScheduleRoutine() {
  folly::HHWheelTimer& t = event_base_.timer();
  t.scheduleTimeoutFn(std::bind(&Raft::Routine, this), std::chrono::milliseconds(options_.routine_period_msecs));
}

void Raft::EventLoop() {
  ScheduleRoutine();
  event_base_.loopForever();
}

void Raft::HandleTransferLeaderComplete(raft_transfer_state_e state) {
  switch (state) {
    case RAFT_STATE_LEADERSHIP_TRANSFER_EXPECTED_LEADER: {
      MRAFT_INFO("Transfer Leader Complete OK");
      break;
    }
    case RAFT_STATE_LEADERSHIP_TRANSFER_UNEXPECTED_LEADER: {
      MRAFT_INFO("Transfer Leader Complete Err:different node elected leader");
      break;
    }
    case RAFT_STATE_LEADERSHIP_TRANSFER_TIMEOUT: {
      MRAFT_INFO("Transfer Leader Complete Err:transfer timed out");
      break;
    }
    default: {
      MRAFT_ERROR("Transfer Leader Complete Err:unknown state:{}", state);
      break;
    }
  }
}

int Raft::AppendConfigChange(raft_logtype_e change_type, const RaftNodePeer& peer, bool only_log) {
  raft_entry_t* ety = raft_entry_new(sizeof(RaftCfgChange));
  RaftCfgChange* cfgchange = reinterpret_cast<RaftCfgChange*>(ety->data);
  cfgchange->node_id = peer.id();
  cfgchange->port = peer.port();
  std::memcpy(cfgchange->host, peer.host().data(), peer.host().size());
  cfgchange->host[peer.host().size()] = 0;
  ety->type = change_type;
  if (only_log) {
    raft_log_impl_.append(raft_log_store_.get(), ety);
  } else {
    msg_entry_response_t response;
    int e = raft_recv_entry(raft_server_, ety, &response);
  }

  raft_entry_release(ety);
  return 0;
}

int Raft::AddNodeHasSufficientLogs(const RaftNodePeer& peer) {
  if (state_ == RaftState::RAFT_LOADING) {
    return 0;
  }
  return AppendConfigChange(RAFT_LOGTYPE_ADD_NODE, peer, false);
}

int Raft::GetNodePeerById(uint32_t id, RaftNodePeer& peer) {
  auto found = meta_.node_peers().find(id);
  if (found != meta_.node_peers().end()) {
    peer = found->second;
    return 0;
  }
  MRAFT_ERROR("[{}]Can NOT find peer by id:{}", GetNodeID(), id);
  return -1;
}

std::unique_ptr<Snapshot> Raft::DoLoadSnapshot(int64_t index) {
  std::unique_ptr<Snapshot> s = std::make_unique<Snapshot>(snapshot_home_, index);
  int rc = s->Open();
  if (0 != rc) {
    return nullptr;
  }
  rc = OnSnapshotLoad(*s);
  if (0 != rc) {
    return nullptr;
  }
  return s;
}

void Raft::DoSnapshotSave() {
  int64_t snapshot_index = raft_log_store_->LastLogIndex();
  std::unique_ptr<Snapshot> s = std::make_unique<Snapshot>(snapshot_home_, snapshot_index);
  raft_begin_snapshot(raft_server_, RAFT_SNAPSHOT_NONBLOCKING_APPLY);
  int rc = OnSnapshotSave(*s);
  raft_end_snapshot(raft_server_);
  if (0 == rc) {
    last_snapshot_ = std::move(s);
  }
}

int Raft::LoadLastSnapshot() {
  if (meta_.snapshot_last_idx() <= 0) {
    MRAFT_WARN("No snapshot need to load.");
    return 0;
  }
  last_snapshot_ = DoLoadSnapshot(meta_.snapshot_last_idx());
}

int Raft::LoadMeta(bool& is_empty) {
  is_empty = false;
  std::string meta_file = options_.home + "/";
  meta_file.append(kRaftMeta);
  if (!folly::fs::is_regular_file(meta_file)) {
    MRAFT_WARN("Meta:{} is not exist.", meta_file);
    is_empty = true;
    return 0;
  }
  int rc = pb_read_file(meta_file, meta_);
  if (0 == rc) {
    // return 0;
  } else {
    MRAFT_ERROR("Meta:{} failed to read.", meta_file);
  }
  return rc;
}

int Raft::SaveMeta() {
  std::string meta_file = options_.home + "/";
  meta_file.append(kRaftMeta);
  int rc = pb_write_file(meta_file, meta_);
  if (0 != rc) {
    MRAFT_ERROR("Meta:{} failed to save.", meta_file);
  }
  return rc;
}

int Raft::LoadCommitLog() {
  if (meta_.last_applied_idx() > 0) {
    raft_set_commit_idx(raft_server_, meta_.last_applied_idx());
  }
  raft_set_snapshot_metadata(raft_server_, meta_.snapshot_last_term(), meta_.snapshot_last_idx());
  raft_apply_all(raft_server_);
  raft_set_current_term(raft_server_, meta_.last_applied_term());
  if (meta_.voted_for() > 0) {
    raft_vote_for_nodeid(raft_server_, meta_.voted_for());
  }
  return 0;
}

void Raft::SetElectionTimeout(int msec) { raft_set_election_timeout(raft_server_, msec); }
void Raft::SetRequestTimeout(int msec) { raft_set_request_timeout(raft_server_, msec); }

int Raft::PersistVote(uint32_t node_id) {
  meta_.set_voted_for(node_id);
  return SaveMeta();
}
int Raft::PersistTerm(int64_t term, int32_t node_id) {
  if (state_ == RaftState::RAFT_LOADING) {
    return 0;
  }
  meta_.set_voted_for(node_id);
  meta_.set_last_applied_term(term);
  return SaveMeta();
}

const std::string& Raft::Name() const { return options_.name; }

const RaftNodePeer& Raft::GetPeer() const { return meta_.self_peer(); }

bool Raft::IsLeader() const { return raft_is_leader(raft_server_); }

int Raft::TryJoinExistingCluster() {
  bool success_join = false;
  NodeJoinResponse join_res;
  for (auto& peer : options_.raft_peers) {
    if (peer == options_.self_peer) {
      continue;
    }
    NodeJoinRequest request;
    request.set_cluster(options_.name);
    request.mutable_leader()->CopyFrom(peer);
    request.mutable_peer()->CopyFrom(options_.self_peer);

    folly::Latch latch(1);
    int rc = OnSendNodeJoin(peer, request, [this, &success_join, &latch, &join_res](NodeJoinResponse&& res) {
      if (res.success()) {
        success_join = true;
        join_res = std::move(res);
      } else {
        success_join = false;
      }
      latch.count_down();
    });
    if (0 == rc) {
      latch.wait();
    }
    if (success_join) {
      break;
    } else {
      MRAFT_WARN("[{}:{}]Failed to join raft cluster:{} with leader:{}:{} with err:{}/{}", options_.self_peer.host(),
                 options_.self_peer.port(), options_.name, peer.host(), peer.port(), join_res.errcode(),
                 GetErrString(join_res.errcode()));
    }
  }
  if (!success_join) {
    MRAFT_ERROR("[{}:{}]Failed to join raft cluster:{} with err:{}/{}", options_.self_peer.host(),
                options_.self_peer.port(), options_.name, join_res.errcode(), GetErrString(join_res.errcode()));
    return -1;
  } else {
    if (join_res.joined_peer() == options_.self_peer) {
      MRAFT_INFO("[{}:{}]Join raft cluster success with id:{}", options_.self_peer.host(), options_.self_peer.port(),
                 options_.name, join_res.joined_peer().id());
      options_.self_peer.CopyFrom(join_res.joined_peer());
      meta_.mutable_self_peer()->CopyFrom(options_.self_peer);
      AddNode(join_res.joined_peer(), false);
    } else {
      MRAFT_ERROR("[{}:{}]Failed to join raft cluster:{} with empty response.", options_.self_peer.host(),
                  options_.self_peer.port(), options_.name);
      return -1;
    }
  }
  return 0;
}

int Raft::Init(const RaftOptions& options) {
  if (options.home.empty()) {
    MRAFT_ERROR("mraft 'home' is EMPTY");
    return -1;
  }
  if (options.create_cluster != 0 && options.create_cluster != 1) {
    MRAFT_ERROR("'create_cluster' MUST be set to 0 or 1.");
    return -1;
  }
  if (options.self_peer.host().empty() || 0 == options.self_peer.port()) {
    MRAFT_ERROR("'slef_peer' is not set.");
    return -1;
  }
  std::string log_home = options.home + "/log";
  if (options.create_cluster) {
    folly::fs::remove_all(log_home);
    MRAFT_ERROR("Remove {} since create_cluster = 1", log_home);
  }
  raft_log_store_ = std::make_unique<SegmentLogStorage>(log_home);
  if (0 != raft_log_store_->Init()) {
    return -1;
  }

  snapshot_home_ = options.home + "/snapshot";
  init_raft_log_impl(&raft_log_impl_);
  raft_server_ = raft_new_with_log(&raft_log_impl_, raft_log_store_.get());
  raft_cbs_t raft_funcs;
  raft_funcs.send_requestvote = Raft::RaftSendRequestVote;
  raft_funcs.send_appendentries = Raft::RaftSendAppendEntries;
  raft_funcs.send_snapshot = Raft::RaftSendSnapshot;
  raft_funcs.load_snapshot = Raft::RaftLoadSnapshot;
  raft_funcs.get_snapshot_chunk = Raft::RaftGetSnapshotChunk;
  raft_funcs.clear_snapshot = Raft::RaftClearSnapshot;
  raft_funcs.applylog = Raft::RaftApplyLog;
  raft_funcs.persist_vote = Raft::RaftPersistVote;
  raft_funcs.persist_term = Raft::RaftPersistTerm;
  raft_funcs.get_node_id = Raft::RaftLogGetNodeId;
  raft_funcs.node_has_sufficient_logs = Raft::RaftNodeHasSufficientLogs;
  raft_funcs.notify_membership_event = Raft::RaftNotifyMembershipEvent;
  raft_funcs.notify_state_event = Raft::RaftNotifyStateEvent;
  raft_funcs.notify_transfer_event = Raft::RaftNotifyTransferEvent;
  raft_funcs.log = Raft::RaftLog;
  raft_funcs.send_timeoutnow = Raft::RaftSendTimeoutNow;
  raft_set_callbacks(raft_server_, &raft_funcs, this);
  meta_.Clear();
  options_ = options;
  std::sort(options_.raft_peers.begin(), options_.raft_peers.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.host() < rhs.host()) {
      return true;
    }
    if (lhs.host() > rhs.host()) {
      return false;
    }
    return lhs.port() < rhs.port();
  });
  state_ = RaftState::RAFT_UNINITIALIZED;
  bool start_from_empty = false;

  if (options.create_cluster) {
    start_from_empty = true;
  } else {
    if (0 != LoadMeta(start_from_empty)) {
      return -1;
    }
  }
  state_ = RaftState::RAFT_UP;
  if (start_from_empty) {
    if (options.create_cluster) {
      if (options_.self_peer == options_.raft_peers[0]) {
        MRAFT_INFO("[{}:{}]Since self node is min address, start as first leader node.", options_.self_peer.host(),
                   options_.self_peer.port());
        // init first node by node_id 1
        options_.self_peer.set_id(1);
        meta_.mutable_self_peer()->CopyFrom(options_.self_peer);
        AddNode(options_.self_peer, true);
        raft_become_leader(raft_server_);
        AppendConfigChange(RAFT_LOGTYPE_ADD_NODE, options_.self_peer, true);
      } else {
        int rc = TryJoinExistingCluster();
        if (0 != rc) {
          MRAFT_INFO("[{}:{}]Join cluster failed.", options_.self_peer.host(), options_.self_peer.port());
          return rc;
        }
        MRAFT_INFO("[{}]Join cluster success.", GetNodeID());
      }
    } else {
      // new node join
      // 1. get node id from leader
      // 2. add nodes
      if (0 != TryJoinExistingCluster()) {
        return -1;
      }
    }

  } else {
    // load snapshot
    if (0 != LoadLastSnapshot()) {
      return -1;
    }
    for (const auto& peer_pair : meta_.node_peers()) {
      const auto& peer = peer_pair.second;
      AddNode(peer, true);
    }
    // load commit entry
    if (0 != LoadCommitLog()) {
      return -1;
    }
  }
  try_emplace_raft(this);
  event_base_thread_ = std::make_unique<std::thread>(&Raft::EventLoop, this);
  event_base_.waitUntilRunning();
  return 0;
}

int Raft::ClearInComingSnapshot() {
  if (incoming_snapshot_) {
    incoming_snapshot_->Remove();
    incoming_snapshot_ = nullptr;
  }
  return 0;
}

int Raft::TransferLeader(int32_t target_node_id, int32_t timeout_msecs, Done&& done) {
  event_base_.runInEventBaseThread([this, target_node_id, timeout_msecs, done = std::move(done)]() {
    if (!CheckRaftState()) {
      done(RAFT_ERR_INVALID_STATE);
      return;
    }
    int rc = raft_transfer_leader(raft_server_, target_node_id, timeout_msecs);
    done(rc);
  });

  return 0;
}

int Raft::RecvNodeJoin(const RaftNodePeer& peer, const NodeJoinRequest& req, NodeJoinResponse& res, Done&& done) {
  if (!CheckRaftState()) {
    return -1;
  }
  event_base_.runInEventBaseThread([this, peer, &req, &res, done = std::move(done)]() {
    int rc = RaftErrorCode::RAFT_ERR_OK;
    if (!IsLeader()) {
      rc = RaftErrorCode::RAFT_ERR_NOT_LEADER;
      res.set_success(false);
      res.set_errcode(rc);
    } else {
      int32_t next_id = NextNodeID();
      RaftNodePeer add_peer;
      add_peer.set_id(next_id);
      add_peer.set_host(req.peer().host());
      add_peer.set_port(req.peer().port());
      res.set_success(true);
      res.mutable_joined_peer()->CopyFrom(add_peer);
      meta_.mutable_node_peers()->insert({next_id, add_peer});

      AppendConfigChange(RAFT_LOGTYPE_ADD_NONVOTING_NODE, add_peer, false);
    }
    done(rc);
  });
  return 0;
}

int Raft::RecvEntry(const folly::IOBuf& data, RaftEntryClosure* closure) {
  if (!CheckRaftState()) {
    return -1;
  }
  raft_entry_t* entry = raft_entry_new(data.length());
  memcpy(entry->data, reinterpret_cast<const void*>(data.data()), data.length());
  entry->data_len = data.length();
  entry->type = RAFT_LOGTYPE_NORMAL;
  entry->user_data = closure;
  event_base_.runInEventBaseThread([this, entry, closure]() {
    RaftErrorCode rc = RaftErrorCode::RAFT_ERR_OK;
    if (!IsLeader()) {
      rc = RaftErrorCode::RAFT_ERR_NOT_LEADER;
    } else {
      msg_entry_response_t r;
      int e = raft_recv_entry(raft_server_, entry, &r);
      rc = static_cast<RaftErrorCode>(e);
    }
    if (RaftErrorCode::RAFT_ERR_OK != rc) {
      raft_entry_release(entry);
      closure->Run(rc);
    }
  });
  return 0;
}

int Raft::DoRecvAppendEntries(const RaftNodePeer& peer, const AppendEntriesRequest& req, AppendEntriesResponse& res) {
  raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
  if (nullptr == raft_node) {
    MRAFT_ERROR("No node found by id:{}", peer.id());
    return -1;
  }
  msg_appendentries_t entries;
  entries.leader_id = req.leader_id();
  entries.msg_id = req.msg_id();
  entries.term = req.term();
  entries.prev_log_idx = req.prev_log_idx();
  entries.prev_log_term = req.prev_log_term();
  entries.leader_commit = req.leader_commit();
  entries.entries = reinterpret_cast<msg_entry_t**>(malloc(sizeof(msg_entry_t*) * req.entries_size()));
  entries.n_entries = req.entries_size();

  for (int i = 0; i < req.entries_size(); i++) {
    auto entry = raft_entry_new(req.entries(i).data().size());
    entry->term = req.entries(i).term();
    entry->id = req.entries(i).id();
    entry->type = req.entries(i).type();
    entry->data_len = req.entries(i).data().size();

    MRAFT_DEBUG("Recv entry id:{}, type:{}", entry->id, entry->type);

    if (req.entries(i).data().size() > 0) {
      memcpy(entry->data, req.entries(i).data().data(), entry->data_len);
    }
    entries.entries[i] = entry;
  }
  msg_appendentries_response_t r;
  int rc = raft_recv_appendentries(raft_server_, raft_node, &entries, &r);
  for (int i = 0; i < req.entries_size(); i++) {
    raft_entry_release(entries.entries[i]);
  }
  free(entries.entries);
  if (0 == rc) {
    res.set_success(true);
    res.set_msg_id(r.msg_id);
    res.set_term(r.term);
    res.set_current_idx(r.current_idx);
  } else {
    res.set_success(false);
  }
  return rc;
}

int Raft::RecvAppendEntries(const RaftNodePeer& peer, const AppendEntriesRequest& req, AppendEntriesResponse& res,
                            Done&& done) {
  if (!CheckRaftState()) {
    return -1;
  }
  event_base_.runInEventBaseThread([this, peer, &req, &res, done = std::move(done)]() {
    int rc = DoRecvAppendEntries(peer, req, res);
    done(rc);
  });
  return 0;
}

int Raft::RecvTimeoutNow(const RaftNodePeer& peer, const TimeoutNowRequest& req, TimeoutNowResponse& res, Done&& done) {
  event_base_.runInEventBaseThread([this, peer, &req, &res, done = std::move(done)]() {
    if (!CheckRaftState()) {
      done(RAFT_ERR_INVALID_STATE);
      return;
    }
    raft_set_timeout_now(raft_server_);
    done(0);
  });
  return 0;
}

int Raft::RecvAppendEntriesResponse(const RaftNodePeer& peer, AppendEntriesResponse&& res) {
  if (!CheckRaftState()) {
    return -1;
  }
  if (nullptr == raft_server_) {
    return -1;
  }
  msg_appendentries_response_t response;
  response.msg_id = res.msg_id();
  response.current_idx = res.current_idx();
  response.term = res.term();
  response.success = res.success();

  raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
  if (nullptr == raft_node) {
    MRAFT_ERROR("No node found by id:{}", peer.id());
    return -1;
  }

  int ret;
  if ((ret = raft_recv_appendentries_response(raft_server_, raft_node, &response)) != 0) {
    MRAFT_ERROR("raft_recv_appendentries_response failed, error {}", ret);
  }

  /* Maybe we have pending stuff to apply now */
  raft_apply_all(raft_server_);
  return 0;
}

int Raft::RecvRequestVoteResponse(const RaftNodePeer& peer, RequestVoteResponse&& res) {
  if (!CheckRaftState()) {
    return -1;
  }
  MRAFT_DEBUG("[{}]RecvRequestVoteResponse {}", GetNodeID(), res.DebugString());
  msg_requestvote_response_t response;
  response.prevote = res.prevote();
  response.request_term = res.request_term();
  response.term = res.term();
  response.vote_granted = res.vote_granted();

  raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
  if (nullptr == raft_node) {
    MRAFT_ERROR("RAFT.REQUESTVOTE stale reply, no node found by id:{}", peer.id());
    return -1;
  }

  int ret;
  if ((ret = raft_recv_requestvote_response(raft_server_, raft_node, &response)) != 0) {
    MRAFT_ERROR("raft_recv_requestvote_response failed, error {}", ret);
  }
  return ret;
}

int Raft::RecvSnapshotResponse(const RaftNodePeer& peer, SnapshotResponse&& res) {
  if (!CheckRaftState()) {
    return -1;
  }
  msg_snapshot_response_t response;
  response.msg_id = res.msg_id();
  response.term = res.term();
  response.offset = res.offset();
  response.success = res.success();
  response.last_chunk = res.last_chunk();

  raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
  if (nullptr == raft_node) {
    MRAFT_ERROR("RAFT.RECVSNAPSHOTRES stale reply, no node found by id:{}", peer.id());
    return -1;
  }

  int ret;
  if ((ret = raft_recv_snapshot_response(raft_server_, raft_node, &response)) != 0) {
    MRAFT_ERROR("raft_recv_snapshot_response failed, error {}", ret);
  }
  return ret;
}

int Raft::DoRecvRequestVote(const RaftNodePeer& peer, const RequestVoteRequest& req, RequestVoteResponse& res) {
  if (!CheckRaftState()) {
    return -1;
  }
  msg_requestvote_response_t response;
  msg_requestvote_t request;
  request.prevote = req.prevote();
  request.term = req.term();
  request.candidate_id = req.candidate_id();
  request.last_log_idx = req.last_log_idx();
  request.last_log_term = req.last_log_term();
  request.transfer_leader = req.transfer_leader();
  raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
  if (nullptr == raft_node) {
    MRAFT_ERROR("no node found by id:{}", peer.id());
    return -1;
  }
  if (raft_recv_requestvote(raft_server_, raft_node, &request, &response) != 0) {
    MRAFT_ERROR("RecvRequestVote operation failed");
    return -1;
  }
  MRAFT_ERROR("#### vote_granted:{}", response.vote_granted);
  res.set_prevote(response.prevote);
  res.set_request_term(response.request_term);
  res.set_term(response.term);
  res.set_vote_granted(response.vote_granted);
  return 0;
}

int Raft::RecvRequestVote(const RaftNodePeer& peer, const RequestVoteRequest& req, RequestVoteResponse& res,
                          Done&& done) {
  event_base_.runInEventBaseThread([this, peer, &req, &res, done = std::move(done)]() {
    int rc = DoRecvRequestVote(peer, req, res);
    done(rc);
  });
  return 0;
}

int Raft::RecvSnapshot(const RaftNodePeer& peer, const SnapshotRequest& req, SnapshotResponse& res, Done&& done) {
  event_base_.runInEventBaseThread([this, peer, &req, &res, done = std::move(done)]() {
    if (!CheckRaftState()) {
      done(-1);
      return;
    }
    if (0 == req.chunk().offset()) {
      incoming_snapshot_ = std::make_unique<Snapshot>(snapshot_home_, req.snapshot_index());
      if (0 != incoming_snapshot_->Open(&req.meta())) {
        done(-1);
        return;
      }
    }
    msg_snapshot_t request;
    request.term = req.term();
    request.leader_id = req.leader_id();
    request.msg_id = req.msg_id();
    request.snapshot_index = req.snapshot_index();
    request.snapshot_term = req.snapshot_term();
    request.chunk.offset = req.chunk().offset();
    request.chunk.last_chunk = req.chunk().last_chunk();
    request.chunk.data = const_cast<char*>(req.chunk().data().data());
    request.chunk.len = req.chunk().data().size();

    msg_snapshot_response_t response;
    raft_node_t* raft_node = raft_get_node(raft_server_, peer.id());
    if (nullptr == raft_node) {
      MRAFT_ERROR("no node found by id:{}", peer.id());
      done(-1);
      return;
    }
    if (raft_recv_snapshot(raft_server_, raft_node, &request, &response) != 0) {
      MRAFT_ERROR("RecvSnapshot operation failed");
      res.set_success(false);
      done(-1);
      return;
    }
    res.set_success(true);
    res.set_last_chunk(response.last_chunk);
    res.set_term(response.term);
    res.set_offset(response.offset);
    res.set_msg_id(response.msg_id);
    done(0);
  });
  return 0;
}

int Raft::SendSnapshot(const RaftNodePeer& peer, msg_snapshot_t* msg) {
  if (!last_snapshot_) {
    MRAFT_ERROR("Empty last snapshot to send.");
    return -1;
  }
  SnapshotRequest request;
  request.set_cluster(options_.name);
  request.set_node_id(peer.id());
  request.set_term(msg->term);
  request.set_leader_id(msg->leader_id);
  request.set_msg_id(msg->msg_id);
  request.set_snapshot_index(msg->snapshot_index);
  request.set_snapshot_term(msg->snapshot_term);
  request.mutable_chunk()->set_offset(msg->chunk.offset);
  request.mutable_chunk()->set_last_chunk(msg->chunk.last_chunk);
  request.mutable_chunk()->set_data(msg->chunk.data, msg->chunk.len);
  if (msg->chunk.offset == 0) {
    request.mutable_meta()->CopyFrom(last_snapshot_->GetMeta());
  }
  return OnSendSnapshot(peer, request, [this, peer](SnapshotResponse&& res) {
    event_base_.runInEventBaseThread(
        [this, peer, res = std::move(res)]() mutable { RecvSnapshotResponse(peer, std::move(res)); });
  });
}

int Raft::AppendEntries(const RaftNodePeer& peer, AppendEntriesRequest& request) {
  request.set_cluster(options_.name);
  request.set_node_id(peer.id());
  return OnSendAppendEntries(peer, request, [this, peer](AppendEntriesResponse&& res) {
    event_base_.runInEventBaseThread(
        [this, peer, res = std::move(res)]() mutable { RecvAppendEntriesResponse(peer, std::move(res)); });
  });
}

int Raft::RequestVote(const RaftNodePeer& peer, RequestVoteRequest& request) {
  request.set_cluster(options_.name);
  request.set_node_id(peer.id());
  return OnSendRequestVote(peer, request, [this, peer](RequestVoteResponse&& res) {
    event_base_.runInEventBaseThread(
        [this, peer, res = std::move(res)]() mutable { RecvRequestVoteResponse(peer, std::move(res)); });
  });
}

int Raft::TimeoutNow(const RaftNodePeer& peer) {
  TimeoutNowRequest request;
  request.set_cluster(options_.name);
  request.set_node_id(peer.id());
  return OnSendTimeoutNow(peer, request, [this, peer](TimeoutNowResponse&& res) {
    event_base_.runInEventBaseThread(
        [this, peer, res = std::move(res)]() mutable { RecvTimeoutNowResponse(peer, std::move(res)); });
  });
}

int Raft::RecvTimeoutNowResponse(const RaftNodePeer& peer, TimeoutNowResponse&& res) { return 0; }

int Raft::StoreSnapshotChunk(raft_index_t snapshot_index, raft_size_t offset, raft_snapshot_chunk_t* chunk) {
  if (!incoming_snapshot_) {
    MRAFT_ERROR("Empty incoming snapshot");
    return -1;
  }

  if (incoming_snapshot_->GetIndex() != snapshot_index) {
    MRAFT_ERROR("Snapshot index was : {}, received a chunk for {} ", incoming_snapshot_->GetIndex(), snapshot_index);
    return -1;
  }

  int rc = incoming_snapshot_->Write(offset, chunk->data, chunk->len);
  if (0 != rc) {
    MRAFT_ERROR("Snapshot index:{} write chunk:{}/{} failed with rc:{}, close  incoming snapshot.", snapshot_index,
                offset, chunk->len, rc);
    incoming_snapshot_ = nullptr;
  } else {
    if (chunk->last_chunk) {
      incoming_snapshot_ = nullptr;  // close incoming snapshot for last success chunk
    }
  }
  return rc;
}

int Raft::GetSnapshotChunk(const RaftNodePeer& peer, raft_size_t offset, raft_snapshot_chunk_t* chunk) {
  if (!last_snapshot_) {
    return -1;
  }
  int64_t chunk_len = 0;
  bool last_chunk = false;
  int rc = last_snapshot_->Read(offset, chunk->data, chunk_len, last_chunk);
  if (0 == rc) {
    chunk->len = chunk_len;
    chunk->last_chunk = last_chunk;
    chunk->offset = offset;
  }
  return rc;
}

int Raft::RaftLoadSnapshot(int64_t term, int64_t index) {
  if (raft_begin_load_snapshot(raft_server_, term, index) != 0) {
    MRAFT_ERROR("Cannot load snapshot: already loaded?");
    return -1;
  }
  auto s = DoLoadSnapshot(index);
  raft_end_load_snapshot(raft_server_);
  if (!s) {
    return -1;
  }
  last_snapshot_ = std::move(s);
  return 0;
}

int32_t Raft::NextNodeID() {
  int32_t max_node_id = 0;
  for (const auto& pair : meta_.node_peers()) {
    if (pair.first > max_node_id) {
      max_node_id = pair.first;
    }
  }
  return max_node_id + 1;
}

int Raft::AddNode(const RaftNodePeer& peer, bool vote) {
  int is_self = 0;
  if (peer.host() == GetPeer().host() && peer.port() == GetPeer().port()) {
    is_self = 1;
  }
  int32_t node_id = peer.id();
  meta_.mutable_node_peers()->insert({node_id, peer});
  raft_node_t* node = nullptr;
  if (vote) {
    node = raft_add_node(raft_server_, this, node_id, is_self);
  } else {
    node = raft_add_non_voting_node(raft_server_, this, node_id, is_self);
    if (nullptr == node) {
      auto* x = raft_get_node(raft_server_, node_id);
      printf("existing node:%d %d\n", node_id, x == nullptr);
    }
  }
  if (nullptr == node) {
    MRAFT_ERROR("Failed to add node {}/{}:{}", node_id, peer.host(), peer.port());
    return -1;
  }
  if (is_self) {
    meta_.mutable_self_peer()->CopyFrom(peer);
  }
  return 0;
}

void Raft::Routine() {
  if (nullptr == raft_server_) {
    return;
  }
  raft_periodic(raft_server_, options_.routine_period_msecs);
  raft_apply_all(raft_server_);
  ScheduleRoutine();
}
}  // namespace mraft

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

#pragma once
#include <stdint.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "folly/io/IOBuf.h"
#include "folly/io/async/EventBase.h"
#include "mraft.pb.h"
#include "mraft/c_raft.h"
#include "mraft/log_entry.h"
#include "mraft/snapshot.h"

namespace mraft {

struct RaftOptions {
  std::string name;
  std::string home;
  RaftNodePeer self_peer;
  std::vector<RaftNodePeer> raft_peers;
  uint32_t routine_period_msecs = 100;
  int create_cluster = -1;  // must set this to 0/1
};

enum class RaftState {
  RAFT_UNINITIALIZED, /* Waiting for RAFT.CLUSTER command */
  RAFT_UP,            /* Up and running */
  RAFT_LOADING,       /* Loading (or attempting) RDB/Raft Log on startup */
  RAFT_JOINING,       /* Processing a RAFT.CLUSTER JOIN command */
  RAFT_SHUTDOWN,
};

enum RaftErrorCode {
  RAFT_ERR_OK = 0,
  RAFT_ERR_NOT_LEADER = -2,
  RAFT_ERR_ONE_VOTING_CHANGE_ONLY = -3,
  RAFT_ERR_SHUTDOWN = -4,
  RAFT_ERR_NOMEM = -5,
  RAFT_ERR_SNAPSHOT_IN_PROGRESS = -6,
  RAFT_ERR_SNAPSHOT_ALREADY_LOADED = -7,
  RAFT_ERR_INVALID_NODEID = -8,
  RAFT_ERR_LEADER_TRANSFER_IN_PROGRESS = -9,
  RAFT_ERR_DONE = -10,
  RAFT_ERR_STALE_TERM = -11,
  RAFT_ERR_LAST = -100,
  RAFT_ERR_RPC_TIMEOUT = -108,
  RAFT_ERR_INVALID_STATE = -109,
};

class LogStorage;

using AppendEntriesCallback = std::function<void(AppendEntriesResponse&&)>;
using RequestVoteCallback = std::function<void(RequestVoteResponse&&)>;
using SendSnapshotCallback = std::function<void(SnapshotResponse&&)>;
using TimeoutNowCallback = std::function<void(TimeoutNowResponse&&)>;
using NodeJoinCallback = std::function<void(NodeJoinResponse&&)>;
using Done = std::function<void(int)>;

struct RaftID {
  std::string_view cluster;
  int id = -1;
};

struct RaftEntryClosure {
  virtual void Run(RaftErrorCode code) = 0;
  virtual ~RaftEntryClosure() {}
};

class Raft {
 protected:
  virtual int OnSendAppendEntries(const RaftNodePeer& peer, AppendEntriesRequest& msg,
                                  AppendEntriesCallback&& callback) = 0;
  virtual int OnSendRequestVote(const RaftNodePeer& peer, RequestVoteRequest& msg, RequestVoteCallback&& callback) = 0;
  virtual int OnSendSnapshot(const RaftNodePeer& peer, const SnapshotRequest& msg, SendSnapshotCallback&& callback) = 0;
  virtual int OnSnapshotLoad(const Snapshot& snapshot) = 0;
  virtual int OnSnapshotSave(Snapshot& snapshot) = 0;
  virtual int OnApplyLog(LogEntry entry, RaftEntryClosure* closure) = 0;
  virtual int OnSendTimeoutNow(const RaftNodePeer& peer, const TimeoutNowRequest& msg,
                               TimeoutNowCallback&& callback) = 0;
  virtual int OnSendNodeJoin(const RaftNodePeer& peer, const NodeJoinRequest& msg, NodeJoinCallback&& callback) = 0;

 public:
  static Raft* Get(std::string_view name, int id);
  static int GetNodeIdByPeer(const RaftNodePeer& peer);
  static std::string_view GetErrString(int err);
  Raft();
  int Init(const RaftOptions& options);
  int Shutdown();
  const std::string& Name() const;
  bool IsLeader() const;
  int GetNodeID();
  const RaftNodePeer& GetPeer() const;
  std::string Info();

  void SetElectionTimeout(int msec);
  void SetRequestTimeout(int msec);

  void GetClusterNodes(std::vector<RaftNodePeer>& peers);

  int RecvEntry(const folly::IOBuf& data, RaftEntryClosure* closure);
  int RecvAppendEntries(const RaftNodePeer& peer, const AppendEntriesRequest& req, AppendEntriesResponse& res,
                        Done&& done);
  int RecvRequestVote(const RaftNodePeer& peer, const RequestVoteRequest& req, RequestVoteResponse& res, Done&& done);
  int RecvSnapshot(const RaftNodePeer& peer, const SnapshotRequest& req, SnapshotResponse& res, Done&& done);
  int RecvNodeJoin(const RaftNodePeer& peer, const NodeJoinRequest& req, NodeJoinResponse& res, Done&& done);
  int RecvTimeoutNow(const RaftNodePeer& peer, const TimeoutNowRequest& req, TimeoutNowResponse& res, Done&& done);

  int TransferLeader(int32_t target_node_id, int32_t timeout_msecs, Done&& done);

  virtual ~Raft();

 protected:
  std::string SelfNode();

 private:
  static int RaftSendRequestVote(raft_server_t* raft, void* user_data, raft_node_t* node, msg_requestvote_t* m);
  static int RaftSendAppendEntries(raft_server_t* raft, void* user_data, raft_node_t* node, msg_appendentries_t* m);
  static int RaftPersistVote(raft_server_t* raft, void* user_data, raft_node_id_t vote);
  static int RaftPersistTerm(raft_server_t* raft, void* user_data, raft_term_t term, raft_node_id_t vote);
  static int RaftApplyLog(raft_server_t* raft, void* user_data, raft_entry_t* entry, raft_index_t entry_idx);
  static void RaftLog(raft_server_t* raft, raft_node_id_t node, void* user_data, const char* buf);
  static raft_node_id_t RaftLogGetNodeId(raft_server_t* raft, void* user_data, raft_entry_t* entry,
                                         raft_index_t entry_idx);
  static int RaftNodeHasSufficientLogs(raft_server_t* raft, void* user_data, raft_node_t* raft_node);
  static void RaftNotifyMembershipEvent(raft_server_t* raft, void* user_data, raft_node_t* raft_node,
                                        raft_entry_t* entry, raft_membership_e type);
  static void RaftNotifyStateEvent(raft_server_t* raft, void* user_data, raft_state_e state);
  static int RaftSendTimeoutNow(raft_server_t* raft, raft_node_t* raft_node);
  static void RaftNotifyTransferEvent(raft_server_t* raft, void* user_data, raft_transfer_state_e state);
  static int RaftSendSnapshot(raft_server_t* raft, void* user_data, raft_node_t* node, msg_snapshot_t* msg);
  static int RaftLoadSnapshot(raft_server_t* raft, void* user_data, raft_index_t snapshot_index,
                              raft_term_t snapshot_term);
  static int RaftGetSnapshotChunk(raft_server_t* raft, void* user_data, raft_node_t* node, raft_size_t offset,
                                  raft_snapshot_chunk_t* chunk);
  static int RaftStoreSnapshotChunk(raft_server_t* raft, void* user_data, raft_index_t snapshot_index,
                                    raft_size_t offset, raft_snapshot_chunk_t* chunk);
  static int RaftClearSnapshot(raft_server_t* raft, void* user_data);

  int StoreSnapshotChunk(raft_index_t snapshot_index, raft_size_t offset, raft_snapshot_chunk_t* chunk);
  int GetSnapshotChunk(const RaftNodePeer& peer, raft_size_t offset, raft_snapshot_chunk_t* chunk);
  int AppendConfigChange(raft_logtype_e type, const RaftNodePeer& peer, bool only_log);
  void HandleTransferLeaderComplete(raft_transfer_state_e state);
  int AddNodeHasSufficientLogs(const RaftNodePeer& peer);
  int PersistVote(uint32_t node_id);
  int PersistTerm(int64_t term, int32_t node_id);
  int GetNodePeerById(uint32_t id, RaftNodePeer& peer);

  int AppendEntries(const RaftNodePeer& peer, AppendEntriesRequest& msg);
  int RequestVote(const RaftNodePeer& peer, RequestVoteRequest& msg);
  int SendSnapshot(const RaftNodePeer& peer, msg_snapshot_t* msg);
  int TimeoutNow(const RaftNodePeer& peer);
  int ClearInComingSnapshot();

  int DoRecvRequestVote(const RaftNodePeer& peer, const RequestVoteRequest& req, RequestVoteResponse& res);
  int DoRecvAppendEntries(const RaftNodePeer& peer, const AppendEntriesRequest& req, AppendEntriesResponse& res);

  int RecvAppendEntriesResponse(const RaftNodePeer& peer, AppendEntriesResponse&& res);
  int RecvRequestVoteResponse(const RaftNodePeer& peer, RequestVoteResponse&& res);
  int RecvSnapshotResponse(const RaftNodePeer& peer, SnapshotResponse&& res);
  int RecvTimeoutNowResponse(const RaftNodePeer& peer, TimeoutNowResponse&& res);

  int LoadMeta(bool& is_empty);
  int SaveMeta();
  int LoadCommitLog();
  int LoadLastSnapshot();

  int TryJoinExistingCluster();

  int AddNode(const RaftNodePeer& peer, bool vote);
  int32_t NextNodeID();
  void DoSnapshotSave();
  void EventLoop();
  bool CheckRaftState();
  void ScheduleRoutine();
  void Routine();
  std::unique_ptr<Snapshot> DoLoadSnapshot(int64_t index);
  int RaftLoadSnapshot(int64_t term, int64_t index);

  RaftOptions options_;
  std::string snapshot_home_;
  folly::EventBase event_base_;
  std::unique_ptr<std::thread> event_base_thread_;
  raft_log_impl_t raft_log_impl_;
  raft_server_t* raft_server_ = nullptr;
  std::unique_ptr<LogStorage> raft_log_store_;
  std::unique_ptr<Snapshot> last_snapshot_;
  std::unique_ptr<Snapshot> incoming_snapshot_;
  RaftMeta meta_;
  std::atomic<RaftState> state_ = {RaftState::RAFT_UP};
  int id_ = -1;
};
}  // namespace mraft
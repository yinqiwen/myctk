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
#include "mraft/tests/local_raft.h"
#include <bits/stdint-intn.h>
#include "folly/FileUtil.h"
#include "mraft/logger.h"

namespace mraft {
int LocalRaft::OnSendAppendEntries(const RaftNodePeer& peer, AppendEntriesRequest& msg,
                                   AppendEntriesCallback&& callback) {
  Raft* remote_raft = Raft::Get(msg.cluster(), msg.node_id());
  if (!remote_raft) {
    return -1;
  }
  LocalRaft* remote = dynamic_cast<LocalRaft*>(remote_raft);
  if (remote->IsHanging()) {
    return -1;
  }
  MRAFT_DEBUG("DoAppendEntries to {} with size:{}", peer.id(), msg.entries_size());
  std::shared_ptr<AppendEntriesResponse> res = std::make_shared<AppendEntriesResponse>();
  std::shared_ptr<AppendEntriesRequest> clone_req = std::make_shared<AppendEntriesRequest>(std::move(msg));
  remote_raft->RecvAppendEntries(peer, *clone_req, *res, [clone_req, res, callback = std::move(callback)](int code) {
    callback(std::move(*res));
  });

  return 0;
}
int LocalRaft::OnSendRequestVote(const RaftNodePeer& peer, RequestVoteRequest& msg, RequestVoteCallback&& callback) {
  Raft* remote_raft = Raft::Get(msg.cluster(), msg.node_id());
  if (!remote_raft) {
    return -1;
  }
  LocalRaft* remote = dynamic_cast<LocalRaft*>(remote_raft);
  if (remote->IsHanging()) {
    return -1;
  }
  MRAFT_DEBUG("DoRequestVote to {} {}", peer.id(), msg.node_id());
  std::shared_ptr<RequestVoteResponse> res = std::make_shared<RequestVoteResponse>();
  std::shared_ptr<RequestVoteRequest> clone_req = std::make_shared<RequestVoteRequest>(std::move(msg));
  remote_raft->RecvRequestVote(peer, *clone_req, *res, [clone_req, res, callback = std::move(callback)](int code) {
    callback(std::move(*res));
  });
  return 0;
}
int LocalRaft::OnSendSnapshot(const RaftNodePeer& peer, const SnapshotRequest& msg, SendSnapshotCallback&& callback) {
  MRAFT_DEBUG("DoSendSnapshot");
  return 0;
}
int LocalRaft::OnSnapshotLoad(const Snapshot& snapshot) {
  MRAFT_DEBUG("OnSnapshotLoad");
  auto f = snapshot.GetReadableFile("counter");

  return 0;
}
int LocalRaft::OnSnapshotSave(Snapshot& snapshot) {
  MRAFT_DEBUG("OnSnapshotSave");
  auto f = snapshot.GetWritableFile("counter");
  if (!f) {
    return -1;
  }
  folly::writeNoInt(f->GetFD(), &counter, sizeof(counter));
  f->Close();
  return 0;
}

int LocalRaft::OnSendTimeoutNow(const RaftNodePeer& peer, const TimeoutNowRequest& msg, TimeoutNowCallback&& callback) {
  MRAFT_DEBUG("DoTimeoutNow");
  Raft* remote_raft = Raft::Get(msg.cluster(), msg.node_id());
  if (!remote_raft) {
    return -1;
  }
  std::shared_ptr<TimeoutNowResponse> res = std::make_shared<TimeoutNowResponse>();
  std::shared_ptr<TimeoutNowRequest> clone_req = std::make_shared<TimeoutNowRequest>(std::move(msg));
  remote_raft->RecvTimeoutNow(peer, *clone_req, *res, [clone_req, res, callback = std::move(callback)](int code) {
    callback(std::move(*res));
  });
  return 0;
}

int LocalRaft::OnSendNodeJoin(const RaftNodePeer& peer, const NodeJoinRequest& msg, NodeJoinCallback&& callback) {
  Raft* remote_raft = Raft::Get(msg.cluster(), Raft::GetNodeIdByPeer(msg.leader()));
  if (!remote_raft) {
    MRAFT_ERROR("No leader found for {}:{} {}", msg.leader().host(), msg.leader().port(),
                Raft::GetNodeIdByPeer(msg.leader()));
    return -1;
  }
  LocalRaft* remote = dynamic_cast<LocalRaft*>(remote_raft);
  MRAFT_DEBUG("OnSendNodeJoin to {}", peer.id());
  std::shared_ptr<NodeJoinResponse> res = std::make_shared<NodeJoinResponse>();
  std::shared_ptr<NodeJoinRequest> clone_req = std::make_shared<NodeJoinRequest>(std::move(msg));
  remote_raft->RecvNodeJoin(peer, *clone_req, *res,
                            [clone_req, res, callback = std::move(callback)](int code) { callback(std::move(*res)); });
  return 0;
}

struct CalcClosure : public RaftEntryClosure {
  uint8_t op;
  int64_t v;
  CalcClosure(uint8_t a = 0, int64_t b = 0) : op(a), v(b) {}
  void Run(RaftErrorCode code) override { delete this; }
};

int LocalRaft::OnApplyLog(LogEntry entry, RaftEntryClosure* closure) {
  CalcClosure tmp;
  CalcClosure* calc = nullptr;
  if (nullptr != closure) {
    calc = dynamic_cast<CalcClosure*>(closure);
    MRAFT_DEBUG("[{}]OnApplyLog with non empty closure", GetNodeID());
  }
  if (nullptr == calc) {
    MRAFT_DEBUG("[{}]OnApplyLog with null closure", GetNodeID());
    calc = &tmp;
    calc->op = entry.Data()[0];
    memcpy(&calc->v, entry.Data() + 1, sizeof(int64_t));
  }
  switch (calc->op) {
    case '+': {
      counter += calc->v;
      break;
    }
    case '-': {
      counter -= calc->v;
      break;
    }
    case '*': {
      counter *= calc->v;
      break;
    }
    case '/': {
      counter /= calc->v;
      break;
    }
    default: {
      MRAFT_DEBUG("OnApplyLog with unknlkwn:{}", calc->op);
      break;
    }
  }
  printf("[%d] counter = %d\n", GetNodeID(), counter);
  return 0;
}
void LocalRaft::Operate(uint8_t op, int64_t v) {
  std::unique_ptr<folly::IOBuf> buf = folly::IOBuf::create(sizeof(int64_t) + 1);
  memcpy(buf->writableData(), &op, 1);
  memcpy(buf->writableData() + 1, &v, sizeof(v));
  buf->append(sizeof(int64_t) + 1);
  RecvEntry(*buf, new CalcClosure(op, v));
}
}  // namespace mraft

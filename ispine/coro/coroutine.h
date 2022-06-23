// Copyright (c) 2022, Tencent Inc.
// All rights reserved.
// Created on 2022/06/23 12:01:37
// Authors: qiyingwang (qiyingwang@tencent.com)

#pragma once
#include <utility>
#include <vector>
#include "folly/Function.h"
#include "folly/Portability.h"
#include "folly/experimental/coro/Collect.h"
#include "folly/experimental/coro/Promise.h"
#include "folly/experimental/coro/Task.h"

#if FOLLY_HAS_COROUTINES
#define ISPINE_HAS_COROUTINES FOLLY_HAS_COROUTINES
#endif

namespace ispine {
#if ISPINE_HAS_COROUTINES
template <typename T>
using Awaitable = folly::coro::Task<T>;

template <typename T>
using CoroFuture = folly::SemiFuture<T>;

template <typename EX, typename T>
CoroFuture<T> coro_spawn(EX ex, Awaitable<T>&& t) {
  return std::move(t).scheduleOn(ex).start();
}

template <typename EX, typename F>
auto coro_spawn(EX ex, F&& f) {
  return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).start();
}
template <typename T>
inline Awaitable<std::vector<T>> coro_join_all(std::vector<CoroFuture<T>>&& all) {
  co_return co_await folly::coro::collectAllRange(std::move(all));
}

#endif
}  // namespace ispine

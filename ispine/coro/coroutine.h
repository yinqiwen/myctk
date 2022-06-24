// Copyright (c) 2022, Tencent Inc.
// All rights reserved.
// Created on 2022/06/23 12:01:37
// Authors: qiyingwang (qiyingwang@tencent.com)

#pragma once
#include <type_traits>
#include <utility>
#include <vector>
#include "folly/Function.h"
#include "folly/Portability.h"
#include "folly/executors/InlineExecutor.h"
#include "folly/executors/QueuedImmediateExecutor.h"
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
using CoroPromise = folly::coro::Promise<T>;

template <typename T>
using CoroFuture = folly::coro::Future<T>;

template <typename EX, typename T>
auto coro_spawn(EX ex, Awaitable<T>&& t) {
  if constexpr (std::is_pointer_v<EX>) {
    using EXClass = typename std::remove_pointer<EX>::type;
    if constexpr (std::is_same_v<EXClass, folly::InlineExecutor>) {
      return std::move(t).scheduleOn(ex).startInlineUnsafe().via(ex);
    } else if constexpr (std::is_same_v<EXClass, folly::QueuedImmediateExecutor>) {
      return std::move(t).scheduleOn(ex).startInlineUnsafe().via(ex);
    } else {
      return std::move(t).scheduleOn(ex).start().via(ex);
    }
  } else {
    if (!dynamic_cast<folly::InlineExecutor*>(ex.get())) {
      return std::move(t).scheduleOn(ex).start().via(ex);
    } else {
      return std::move(t).scheduleOn(ex).startInlineUnsafe().via(ex);
    }
  }
}

template <typename EX, typename F>
auto coro_spawn(EX ex, F&& f) {
  if constexpr (std::is_pointer_v<EX>) {
    using EXClass = typename std::remove_pointer<EX>::type;
    if constexpr (std::is_same_v<EXClass, folly::InlineExecutor>) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe().via(ex);
    } else if constexpr (std::is_same_v<EXClass, folly::QueuedImmediateExecutor>) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe().via(ex);
    } else {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).start().via(ex);
    }
  } else {
    if (!dynamic_cast<folly::InlineExecutor*>(ex.get())) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).start().via(ex);
    } else {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe().via(ex);
    }
  }
}

template <typename T>
std::pair<CoroPromise<T>, CoroFuture<T>> make_coro_promise_contract() {
  return folly::coro::makePromiseContract<T>();
}

template <typename T>
inline Awaitable<std::vector<T>> coro_join_all(std::vector<folly::Future<T>>&& all) {
  co_return co_await folly::coro::collectAllRange(std::move(all));
}

#endif
}  // namespace ispine

// Copyright (c) 2022, Tencent Inc.
// All rights reserved.
// Created on 2022/06/23 12:01:37
// Authors: qiyingwang (qiyingwang@tencent.com)

#pragma once
#include <tuple>
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
struct AwaitToken {};
constexpr AwaitToken use_awaitable;
struct SyncToken {};
constexpr SyncToken use_sync;

#if ISPINE_HAS_COROUTINES
constexpr AwaitToken use_adaptable;
#else
constexpr SyncToken use_adaptable;
#endif

#if ISPINE_HAS_COROUTINES
template <typename T>
using AdaptiveWait = folly::coro::Task<T>;
#else
template <typename T>
using AdaptiveWait = T;
#endif

#if ISPINE_HAS_COROUTINES
template <typename T>
using Awaitable = folly::coro::Task<T>;

template <typename T>
using CoroPromise = folly::coro::Promise<T>;

template <typename T>
using CoroFuture = folly::coro::Future<T>;

template <typename T>
using CoroSemiFuture = folly::SemiFuture<T>;

template <typename EX, typename T>
auto coro_spawn(EX ex, Awaitable<T>&& t) {
  if constexpr (std::is_pointer_v<EX>) {
    using EXClass = typename std::remove_pointer<EX>::type;
    if constexpr (std::is_same_v<EXClass, folly::InlineExecutor>) {
      return std::move(t).scheduleOn(ex).startInlineUnsafe();
    } else if constexpr (std::is_same_v<EXClass, folly::QueuedImmediateExecutor>) {
      return std::move(t).scheduleOn(ex).startInlineUnsafe();
    } else {
      return std::move(t).scheduleOn(ex).start();
    }
  } else {
    if (!dynamic_cast<folly::InlineExecutor*>(ex.get())) {
      return std::move(t).scheduleOn(ex).start();
    } else {
      return std::move(t).scheduleOn(ex).startInlineUnsafe();
    }
  }
}

template <typename EX, typename F>
auto coro_spawn(EX ex, F&& f) {
  if constexpr (std::is_pointer_v<EX>) {
    using EXClass = typename std::remove_pointer<EX>::type;
    if constexpr (std::is_same_v<EXClass, folly::InlineExecutor>) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe();
    } else if constexpr (std::is_same_v<EXClass, folly::QueuedImmediateExecutor>) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe();
    } else {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).start();
    }
  } else {
    if (!dynamic_cast<folly::InlineExecutor*>(ex.get())) {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).start();
    } else {
      return folly::coro::co_invoke(std::move(f)).scheduleOn(ex).startInlineUnsafe();
    }
  }
}

template <typename F>
auto coro_spawn(F&& f) {
  folly::QueuedImmediateExecutor* ex = &(folly::QueuedImmediateExecutor::instance());
  return coro_spawn(ex, std::move(f));
}

template <typename T>
std::pair<CoroPromise<T>, CoroFuture<T>> make_coro_promise_contract() {
  return folly::coro::makePromiseContract<T>();
}

template <typename... SemiAwaitables>
auto coro_join_all(SemiAwaitables&&... awaitables) -> folly::coro::Task<
    std::tuple<folly::coro::detail::collect_all_component_t<folly::remove_cvref_t<SemiAwaitables>>...>> {
  co_return co_await folly::coro::collectAll(static_cast<SemiAwaitables&&>(awaitables)...);
}

template <typename T>
Awaitable<std::vector<T>> coro_join_all(std::vector<folly::SemiFuture<T>>&& all) {
  co_return co_await folly::coro::collectAllRange(std::move(all));
}

#else
#define co_await
#define co_return return
#endif

template <typename T, typename Token>
struct AdaptiveFuncReturnType {
  using return_type = T;
};
#if ISPINE_HAS_COROUTINES
template <typename T>
struct AdaptiveFuncReturnType<T, AwaitToken> {
  using return_type = Awaitable<T>;
};
#endif

}  // namespace ispine

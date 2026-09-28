// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_SCOPE_GUARD_H
#define BASE_MEMORY_SCOPE_GUARD_H

#include <functional>
#include <memory>

#include "base/core/macros.h"

#define ON_SCOPE_EXIT          \
  auto ANONYMOUS_VAR(_exit_) = \
      ::base::detail::ScopeGuardDriver() + [&]() noexcept

#define MAKE_SCOPE_GUARD ::base::detail::ScopeGuardDriver() + [&]() noexcept

namespace base {
template <typename F>
class ScopeGuardImpl {
 public:
  explicit ScopeGuardImpl(F&& fn) noexcept
      : fn_(std::forward<F>(fn)), dismissed_(false) {}

  ~ScopeGuardImpl() {
    if (!dismissed_) {
      fn_();
    }
  }

  void dismiss() noexcept { dismissed_ = true; }

  ScopeGuardImpl(const ScopeGuardImpl&) = delete;
  ScopeGuardImpl& operator=(const ScopeGuardImpl&) = delete;
  ScopeGuardImpl(ScopeGuardImpl&& other) noexcept
      : fn_(std::move(other.fn_)), dismissed_(other.dismissed_) {
    other.dismissed_ = true;
  }
  ScopeGuardImpl& operator=(ScopeGuardImpl&&) = delete;

 private:
  F fn_;
  bool dismissed_;
};

class ScopeGuard {
 private:
  struct Base {
    virtual ~Base() = default;
    virtual void execute() = 0;
  };

  template <typename F>
  struct Impl : Base {
    F fn;
    explicit Impl(F&& f) : fn(std::forward<F>(f)) {}
    void execute() override { fn(); }
  };

  std::unique_ptr<Base> impl_;
  bool dismissed_{false};

 public:
  template <typename F>
  explicit ScopeGuard(F&& fn)
      : impl_(std::make_unique<Impl<std::decay_t<F>>>(std::forward<F>(fn))) {}

  ~ScopeGuard() {
    if (!dismissed_ && impl_) {
      impl_->execute();
    }
  }

  void dismiss() noexcept { dismissed_ = true; }

  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard(ScopeGuard&& other) noexcept
      : impl_(std::move(other.impl_)), dismissed_(other.dismissed_) {
    other.dismissed_ = true;
  }
};

namespace detail {
struct ScopeGuardDriver {};
template <typename F>
auto operator+(ScopeGuardDriver, F&& fn) {
  return ScopeGuardImpl<std::decay_t<F>>(std::forward<F>(fn));
}
}  // namespace detail
}  // namespace base

#endif  // BASE_MEMORY_SCOPE_GUARD_H

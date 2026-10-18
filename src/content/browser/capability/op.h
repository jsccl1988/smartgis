// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One runtime verb slot. The shell binds a callable after the host exists, so
// the slot cannot be a bare template parameter. `op_callable` is the gate:
// assignment accepts only an invocable with a compatible result. There is no
// virtual capability bag and no `std::function`.

#ifndef CONTENT_BROWSER_CAPABILITY_OP_H_
#define CONTENT_BROWSER_CAPABILITY_OP_H_

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace content {
namespace detail {

template <typename F, typename R, typename... Args>
concept op_callable =
    std::invocable<F&, Args...> &&
    (std::is_void_v<R> ||
     std::convertible_to<std::invoke_result_t<F&, Args...>, R>);

// Type-erased callable constrained by `op_callable`. Empty is false and
// returns a value-initialized result if invoked anyway.
template <typename Signature>
class Op;

template <typename R, typename... Args>
class Op<R(Args...)> {
 public:
  Op() = default;
  Op(const Op&) = default;
  Op(Op&&) noexcept = default;
  Op& operator=(const Op&) = default;
  Op& operator=(Op&&) noexcept = default;
  ~Op() = default;

  template <typename F>
    requires op_callable<std::decay_t<F>, R, Args...> &&
             (!std::same_as<std::decay_t<F>, Op>)
  Op(F&& fn) {
    bind(std::forward<F>(fn));
  }

  template <typename F>
    requires op_callable<std::decay_t<F>, R, Args...> &&
             (!std::same_as<std::decay_t<F>, Op>)
  Op& operator=(F&& fn) {
    bind(std::forward<F>(fn));
    return *this;
  }

  explicit operator bool() const { return static_cast<bool>(state_); }

  R operator()(Args... args) const {
    if (!thunk_ || !state_) {
      if constexpr (std::is_void_v<R>) {
        return;
      } else {
        return R{};
      }
    }
    return thunk_(state_.get(), args...);
  }

 private:
  using Thunk = R (*)(void*, Args...);

  template <typename F>
  void bind(F&& fn) {
    using Fn = std::decay_t<F>;
    thunk_ = [](void* p, Args... args) -> R {
      Fn& target = *static_cast<Fn*>(p);
      if constexpr (std::is_void_v<R>) {
        target(args...);
      } else {
        return static_cast<R>(target(args...));
      }
    };
    state_ = std::shared_ptr<void>(new Fn(std::forward<F>(fn)),
                                   [](void* p) { delete static_cast<Fn*>(p); });
  }

  Thunk thunk_ = nullptr;
  // Shared so a copied host keeps the same verb. Rebind replaces this side only.
  std::shared_ptr<void> state_;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_OP_H_

// Copyright (c) 2022 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_FUTURE_H
#define BASE_SYNCHRONIZATION_FUTURE_H

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <exception>
#include <forward_list>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace base {
template <typename T, typename M, typename C>
class future_state {
  static_assert(
      !std::is_void<T>::value,
      "void type should use future_state<void, M, C> specialization.");
 public:
  using mutex_type = M;
  using condition_variable_type = C;

  future_state() : is_available(false) {}
  future_state(const future_state& that) = delete;
  future_state& operator=(const future_state& that) = delete;
  future_state(future_state&& that) = delete;
  future_state& operator=(future_state&& that) = delete;

  ~future_state() {
    if (is_available) {
      reinterpret_cast<T*>(&data)->~T();
    }
  }

  template <typename... Args>
  void set_value(Args&&... args) {
    std::unique_lock<M> ul(mtx);
    assert(!is_available && !is_complete.load(std::memory_order_relaxed));
    try {
      new (reinterpret_cast<T*>(&data)) T{std::forward<Args&&>(args)...};
      is_available = true;
    } catch (...) {
      eptr = std::current_exception();
    }
    is_complete.store(true, std::memory_order_release);
    ul.unlock();

    completed.notify_all();
    notify_wait_queue();
  }

  void set_exception(std::exception_ptr exc) {
    std::unique_lock<M> ul(mtx);
    assert(!is_available && !is_complete.load(std::memory_order_relaxed));
    eptr = std::move(exc);
    is_complete.store(true, std::memory_order_release);
    ul.unlock();

    completed.notify_all();
    notify_wait_queue();
  }

  template <typename Exception>
  void set_exception(Exception&& exc) {
    set_exception(std::make_exception_ptr(std::forward<Exception>(exc)));
  }

  T& get() {
    std::unique_lock<M> ul(mtx);
    completed.wait(ul, [this]() -> bool { return is_complete.load(std::memory_order_acquire); });

    if (eptr) {
      std::rethrow_exception(eptr);
    }

    return *(reinterpret_cast<T*>(&data));
  }

  void wait() {
    std::unique_lock<M> ul(mtx);
    completed.wait(ul, [this]() -> bool { return is_complete.load(std::memory_order_acquire); });

    if (eptr) {
      std::rethrow_exception(eptr);
    }
  }

  bool is_ready() const {
    return is_complete.load(std::memory_order_acquire);
  }

  void await(C& cv) {
    std::unique_lock<M> ul(mtx);
    wait_queue.push_back(&cv);
  }

 private:
  C completed;
  M mtx;
  alignas(T) unsigned char data[sizeof(T) > 0 ? sizeof(T) : 1];
  bool is_available;
  std::atomic<bool> is_complete{false};
  std::vector<C*> wait_queue;
  std::exception_ptr eptr;

  void notify_wait_queue() {
    if (!wait_queue.empty()) {
      for (auto* cv : wait_queue) {
        cv->notify_one();
      }
      wait_queue.clear();
    }
  }
};

template <typename M, typename C>
class future_state<void, M, C> {
 public:
  using mutex_type = M;
  using condition_variable_type = C;

  future_state() {}
  future_state(const future_state& that) = delete;
  future_state& operator=(const future_state& that) = delete;
  future_state(future_state&& that) = delete;
  future_state& operator=(future_state&& that) = delete;

  ~future_state() = default;

  void set_value() {
    std::unique_lock<M> ul(mtx);
    assert(!is_complete.load(std::memory_order_relaxed));
    is_complete.store(true, std::memory_order_release);
    ul.unlock();

    completed.notify_all();
    notify_wait_queue();
  }

  void set_exception(std::exception_ptr exc) {
    std::unique_lock<M> ul(mtx);
    assert(!is_complete.load(std::memory_order_relaxed));
    eptr = std::move(exc);
    is_complete.store(true, std::memory_order_release);
    ul.unlock();

    completed.notify_all();
    notify_wait_queue();
  }

  template <typename Exception>
  void set_exception(Exception&& exc) {
    set_exception(std::make_exception_ptr(std::forward<Exception>(exc)));
  }

  void get() {
    std::unique_lock<M> ul(mtx);
    completed.wait(ul, [this]() -> bool { return is_complete.load(std::memory_order_acquire); });

    if (eptr) {
      std::rethrow_exception(eptr);
    }
  }

  void wait() {
    std::unique_lock<M> ul(mtx);
    completed.wait(ul, [this]() -> bool { return is_complete.load(std::memory_order_acquire); });

    if (eptr) {
      std::rethrow_exception(eptr);
    }
  }

  bool is_ready() const {
    return is_complete.load(std::memory_order_acquire);
  }

  void await(C& cv) {
    std::unique_lock<M> ul(mtx);
    wait_queue.push_back(&cv);
  }

 private:
  C completed;
  M mtx;
  std::atomic<bool> is_complete{false};
  std::vector<C*> wait_queue;
  std::exception_ptr eptr;

  void notify_wait_queue() {
    if (!wait_queue.empty()) {
      for (auto* cv : wait_queue) {
        cv->notify_one();
      }
      wait_queue.clear();
    }
  }
};

template <typename T, typename M, typename C>
class shared_future {
 public:
  using value_type = T;
  shared_future() = default;
  shared_future(std::shared_ptr<future_state<value_type, M, C>> state)
      : shared_state(std::move(state)) {}

  void swap(shared_future& rhs) { shared_state.swap(rhs.shared_state); }

  bool valid() const { return shared_state != nullptr; }
  bool ready() const { return valid() && shared_state->is_ready(); }

  void wait() { shared_state->wait(); }

  template <typename U = T>
  typename std::enable_if<!std::is_void<U>::value, const U&>::type get() {
    wait();
    return shared_state->get();
  }

  template <typename U = T>
  typename std::enable_if<std::is_void<U>::value, void>::type get() {
    wait();
    shared_state->get();
  }

 private:
  std::shared_ptr<future_state<value_type, M, C>> shared_state;
};

template <typename T, typename M, typename C>
class future {
 public:
  using value_type = T;
  using mutex_type = M;
  using condition_variable_type = C;
  future(std::shared_ptr<future_state<value_type, M, C>> state)
      : shared_state(state) {}

  future(const future&) = delete;
  future& operator=(const future&) = delete;
  future(future&&) = default;
  future& operator=(future&&) = default;

  void swap(future_state<value_type, M, C>& rhs) {
    shared_state.swap(rhs.shared_state);
  }

  shared_future<value_type, M, C> share() noexcept {
    return shared_future<value_type, M, C>(std::move(shared_state));
  }

  void wait() { shared_state->wait(); }

  template <typename U = T>
  typename std::enable_if<!std::is_void<U>::value, U&>::type get() {
    return shared_state->get();
  }

  template <typename U = T>
  typename std::enable_if<std::is_void<U>::value, void>::type get() {
    shared_state->get();
  }

  bool valid() const { return shared_state != nullptr; }
  bool is_ready() const { return shared_state->is_ready(); }
  void await(C& cv) { shared_state->await(cv); }

 private:
  std::shared_ptr<future_state<value_type, M, C>> shared_state;
};

template <typename T, typename M, typename C>
class promise {
 public:
  using value_type = T;
  using mutex_type = M;
  using condition_variable_type = C;

  promise()
      : shared_state(std::make_shared<future_state<value_type, M, C>>()) {}

  promise(const promise&) = delete;
  promise& operator=(const promise&) = delete;
  promise(promise&&) = default;
  promise& operator=(promise&& rhs) {
    promise(std::move(rhs)).swap(*this);
    return *this;
  }

  void swap(promise& rhs) { shared_state.swap(rhs.shared_state); }

  future<value_type, M, C> get_future() {
    return future<value_type, M, C>(shared_state);
  }

  template <typename U = T>
  typename std::enable_if<!std::is_void<U>::value>::type
  set_value(const U& value) {
    shared_state->set_value(value);
  }

  template <typename U = T>
  typename std::enable_if<!std::is_void<U>::value>::type
  set_value(U&& value) {
    shared_state->set_value(std::forward<U>(value));
  }

  template <typename U = T>
  typename std::enable_if<std::is_void<U>::value>::type
  set_value() {
    shared_state->set_value();
  }

  void set_exception(std::exception_ptr exc) {
    shared_state->set_exception(std::move(exc));
  }

  template <typename Exception>
  void set_exception(Exception&& exc) {
    shared_state->set_exception(std::forward<Exception>(exc));
  }

 private:
  std::shared_ptr<future_state<value_type, M, C>> shared_state;
};

template <typename T, typename M, typename C>
inline auto make_ready_future(T&& value) {
  promise<std::decay_t<T>, M, C> p;
  p.set_value(std::forward<T>(value));
  return p.get_future();
}

template <typename M, typename C>
inline future<void, M, C> make_ready_future() {
  promise<void, M, C> p;
  p.set_value();
  return p.get_future();
}

template <typename T, typename M, typename C>
inline auto make_except_future(std::exception_ptr&& e) {
  promise<T, M, C> p;
  p.set_exception(std::move(e));
  return p.get_future();
}

template <typename T, typename M, typename C, typename E>
inline future<T, M, C> make_except_future(E&& e) {
  promise<T, M, C> p;
  p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
  return p.get_future();
}
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_FUTURE_H

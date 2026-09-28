# Synchronization - 同步机制

## 概述

`base/synchronization` 模块提供了丰富的同步原语，包括互斥锁、条件变量、屏障、信号量、Future/Promise 等。该模块提供了高性能的同步机制实现，支持多种并发场景。

## TL;DR

- 提供完整的同步原语集合（mutex、futex、latch、barrier、semaphore、future、waiter、`spin_lock`）
- 基于 futex 的高性能互斥锁实现
- 支持异步编程的 Future/Promise 模式
- 提供缓存行对齐工具（`align`）
- **短临界区**：`spin_lock` + `spin_backoff`（勿用于可能阻塞的长临界区；新代码长临界区仍用 `std::mutex`）

**仓库规范**：新代码中互斥优先使用 `<mutex>` / `std::mutex`；本目录的 `mutex.h` 为既有 futex 实现，下文示例仍展示其用法。**不要**在新代码中推广 `base::mutex`。

## 模块边界（与 `concurrency/`、`execution/`）

| 关切 | `base/synchronization` | `base/concurrency` | `base/execution` |
| --- | --- | --- | --- |
| 主要抽象 | 锁、条件同步、一次性/可重复屏障、计数信号量、自研 `future`/`waiter` | 读多写少数据结构、无锁队列、hazard pointer | 执行器、任务投递、Continuation、与 IO/线程池集成 |
| 典型用法 | 保护共享可变状态；线程阶段对齐 | 高并发读路径、日志/任务队列 | CPU/IO 任务调度、`async`/`when_all` |
| 互斥选型 | 新代码用 `std::mutex`；保留 `base::mutex` 仅供遗留或 futex 专项 | `concurrent<T>` 内部可用 `std::shared_mutex` | 执行器内部自有同步 |

本目录的 **latch/barrier/semaphore** 等可在 **C++11/14/17** 环境提供与 C++20 标准库相近的语义，便于在不升级标准的情况下编写可移植同步逻辑。

## Build & Test Cheatsheet

```bash
# 生成构建文件并编译（同步测例在聚合目标 base_test 中）
gn gen out
ninja -C out //base:base_test

# 运行单元测试
./out/base_test --gtest_filter=TestSynchronization.*
```

## 目录导航

| 路径 | 说明 |
| --- | --- |
| `base/synchronization/mutex.h` | 互斥锁（基于 futex） |
| `base/synchronization/futex.h` | Futex 系统调用封装 |
| `base/synchronization/future.h` | Future/Promise 异步编程 |
| `base/synchronization/latch.h` | 门闩（一次性屏障） |
| `base/synchronization/barrier.h` | 屏障（可重用） |
| `base/synchronization/semaphore.h` | 信号量 |
| `base/synchronization/waiter.h` | 等待器（条件变量封装） |
| `base/synchronization/align.h` | 缓存行对齐工具 |
| `base/synchronization/spin_lock.h` | 短临界区自旋锁（配合 `spin_backoff.h`） |
| `base/synchronization/spin_backoff.h` | spin→yield 退避 |

## 核心组件

### 1. mutex - 互斥锁

基于 futex 实现的高性能互斥锁，提供类似 `std::mutex` 的接口。

**主要特性**：
- 基于 Linux futex 系统调用
- 无竞争情况下零系统调用开销
- 支持 `lock()`、`try_lock()`、`unlock()`

**使用示例**：
```cpp
#include "base/synchronization/mutex.h"

base::mutex mtx;
{
    std::lock_guard<base::mutex> lock(mtx);
    // 临界区代码
}
```

### 2. futex - Futex 封装

提供 Linux futex 系统调用的 C++ 封装。

**主要功能**：
- `futex_wait()`: 等待条件满足
- `futex_wake()`: 唤醒等待的线程
- `futex_wake_all()`: 唤醒所有等待的线程

**使用示例**：
```cpp
#include "base/synchronization/futex.h"

std::atomic<int> flag(0);
// 等待 flag 变为 1
base::futex_wait(&flag, 0);
// 设置 flag 并唤醒
flag.store(1);
base::futex_wake(&flag, 1);
```

### 3. future/promise - 异步编程

提供 Future/Promise 模式的异步编程支持。

**主要特性**：
- 支持任意类型的 Future
- 支持异常传播
- 支持 `then()` 链式调用
- 支持 `wait()` 和 `get()` 操作

**使用示例**：
```cpp
#include "base/synchronization/future.h"

base::promise<int> p;
auto future = p.get_future();

// 在另一个线程中设置值
std::thread([&p]() {
    p.set_value(42);
}).detach();

// 等待并获取值
int value = future.get();
```

### 4. latch - 门闩

一次性屏障，用于等待多个线程到达某个点。

**主要特性**：
- 一次性使用，计数到零后不可重用
- 支持 `count_down()` 和 `wait()`

**使用示例**：
```cpp
#include "base/synchronization/latch.h"

base::latch latch(3);  // 等待 3 个线程

std::vector<std::thread> threads;
for (int i = 0; i < 3; ++i) {
    threads.emplace_back([&latch]() {
        // 执行任务
        latch.count_down();
    });
}

latch.wait();  // 等待所有线程完成
```

### 5. barrier - 屏障

可重用的屏障，支持多轮同步。

**主要特性**：
- 可重用，支持多轮同步
- 支持 `arrive_and_wait()` 操作

**使用示例**：
```cpp
#include "base/synchronization/barrier.h"

base::barrier barrier(3);  // 3 个线程

std::vector<std::thread> threads;
for (int i = 0; i < 3; ++i) {
    threads.emplace_back([&barrier]() {
        for (int round = 0; round < 10; ++round) {
            // 执行任务
            barrier.arrive_and_wait();  // 等待所有线程完成本轮
        }
    });
}
```

### 6. semaphore - 信号量

提供计数信号量，用于控制资源访问。

**主要特性**：
- 支持 `acquire()` 和 `release()` 操作
- 支持超时版本的 `try_acquire()`

**使用示例**：
```cpp
#include "base/synchronization/semaphore.h"

base::semaphore sem(5);  // 最多 5 个并发

sem.acquire();  // 获取资源
// 使用资源
sem.release();  // 释放资源
```

### 7. waiter - 等待器

提供条件变量的封装，简化等待/通知模式。

**主要特性**：
- 封装条件变量操作
- 支持超时等待
- 线程安全的等待/通知

**使用示例**：
```cpp
#include "base/synchronization/waiter.h"

base::waiter waiter;
bool ready = false;

// 等待线程
std::thread([&waiter, &ready]() {
    waiter.wait([&ready]() { return ready; });
    // 条件满足，继续执行
}).detach();

// 通知线程
ready = true;
waiter.notify_all();
```

### 8. align - 缓存行对齐

提供缓存行对齐工具，避免 false sharing。

**主要特性**：
- 自动计算缓存行大小
- 提供对齐宏和工具函数

**使用示例**：
```cpp
#include "base/synchronization/align.h"

// 对齐到缓存行
alignas(BASE_CACHE_LINE_SIZE) int counter;
```

### 9. spin_lock / spin_backoff — 短临界区

```cpp
#include "base/synchronization/spin_lock.h"

base::spin_lock lock;
{
  base::scoped_spin_lock<base::spin_lock> g(lock);
  // very short critical section only
}
```

Prefer `std::mutex` when the critical section may block or run long. `spin_backoff`
is for busy-wait loops (queue backpressure), not a mutex substitute.

## 性能对比

### 与标准库实现对比

本实现与 C++ 标准库（std::mutex, std::latch, std::barrier, std::semaphore, std::future）的性能对比：

#### mutex 性能特点

- **无竞争场景**: 基于 futex 实现，零系统调用开销，性能与 `std::mutex` 相当或略优
- **竞争场景**: 在高竞争情况下，futex 实现通常比 `std::mutex` 有更好的可扩展性
- **内存占用**: 仅一个 `atomic<int>`，比 `std::mutex` 更轻量

#### latch/barrier 性能特点

- **实现方式**: 基于 `std::mutex` + `std::condition_variable`，与标准库实现类似
- **性能**: 与 `std::latch`/`std::barrier` 性能相当
- **兼容性**: 支持 C++11，不依赖 C++20

#### semaphore 性能特点

- **实现方式**: 基于 `std::mutex` + `std::condition_variable`
- **性能**: 与 `std::counting_semaphore` 性能相当
- **兼容性**: 支持 C++11，不依赖 C++20

#### future/promise 性能特点

- **实现方式**: 基于模板化设计，支持自定义 mutex 和 condition_variable 类型
- **性能**: 与 `std::future`/`std::promise` 性能相当
- **灵活性**: 可配置底层同步原语，适应不同场景需求

### 性能测试

运行性能基准测试：

```bash
ninja -C out //base:base_benchmark
# base_benchmark 聚合多模块；同步相关见 BM_Mutex_* / BM_Latch_* / BM_Std* 等
./out/base_benchmark --benchmark_filter=BM_Mutex
```

基准测试包含以下对比项：
- `BM_Mutex_Uncontended` vs `BM_StdMutex_Uncontended`
- `BM_Mutex_Contended` vs `BM_StdMutex_Contended`
- `BM_Latch_CountDown_Wait` vs `BM_StdLatch_CountDown_Wait`
- `BM_Barrier_ArriveAndWait` vs `BM_StdBarrier_ArriveAndWait`
- `BM_Semaphore_*` vs `BM_StdSemaphore_*`
- `BM_Future_Promise_*` vs `BM_StdFuture_Promise_*`

## 设计要点

### 性能优化

- `mutex` 基于 futex，无竞争情况下零系统调用开销
- 使用原子操作和内存序优化性能
- 缓存行对齐避免 false sharing

### 线程安全

所有同步原语都是线程安全的，可以在多线程环境中安全使用。

### 异常安全

Future/Promise 支持异常传播，确保异常安全。

## 使用场景

1. **互斥访问**: 使用 `mutex` 保护共享资源
2. **异步编程**: 使用 `future/promise` 进行异步操作
3. **线程同步**: 使用 `latch`、`barrier` 同步多个线程
4. **资源控制**: 使用 `semaphore` 控制并发访问
5. **条件等待**: 使用 `waiter` 实现条件等待模式

## 依赖

- `base/core/macros.h`: 基础宏定义
- C++11 标准库（`<atomic>`, `<mutex>`, `<condition_variable>` 等）

## 注意事项

- `mutex` 基于 Linux futex，在其他平台上可能需要不同的实现
- `latch` 是一次性的，`barrier` 可以重用
- 使用 `future` 时注意异常处理
- 缓存行对齐会增加内存使用，但可以显著提升多线程性能


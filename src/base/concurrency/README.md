# base/concurrency 并发原语库

## 架构总览（速读）

| 类别 | 组件 | 语义要点 |
| --- | --- | --- |
| 读多写少（共享 `T`） | `concurrent<T>` | `std::shared_mutex` 包装，读写比中等、实现简单 |
|  | `left_right<T>` | 双缓冲：读路径 **wait-free**，写更新非活跃副本后切换；内存约 **2×** |
|  | `rcu_ptr<T>` | `shared_ptr<const T>` 快照读，适合**大对象**或需 COW 的少写场景 |
| 生产者–消费者 | `NonblockingQueue` / `BlockingQueue` | 基于 moodycamel 的 MPMC；阻塞版用于线程池/背压 |
| 只追加、周期性消费 | `PushOnlyQueue` | 无锁追加 + `visit`/`clear`（弹出式遍历） |
| 只追加、显式 drain | `AppendOnlyConcurrentQueue` / `concurrent_append_buffer` | 热路径 `enqueue`/`push`，单线程 `gc`/`drain`；供 span instant 等 fan-in |
| 分片写互斥（调试） | `partition_exclusivity` | 每 partition 独立 CAS；与全局 `spin_lock` 不同 |
| lock-free 回收 | `hazard_ptr` | 延迟回收，配合原子指针避免 UAF |

**选型提示（追加类队列）**：

| 需求 | 选 |
| --- | --- |
| 多生产者追加，周期性 `visit` 再 `clear` | `PushOnlyQueue` |
| 多生产者追加，导出时一次 `drain`/`gc`（不与二次 drain 并发） | `concurrent_append_buffer`（或底层 `AppendOnlyConcurrentQueue`） |
| 每 shard 独占写、无需全局锁 | `partition_exclusivity` + 直接写槽 |

**相关模块**：底层锁与 latch/barrier / `spin_lock` 等见 [`base/synchronization`](../synchronization/README.md)；任务投递与线程池见 [`base/execution`](../execution/README.md)。队列与 `Pipeline` 的配合见 [`base/execution/pipeline`](../execution/pipeline/pipeline.h)。热路径 span 见 [`base/trace`](../trace/README.md)。

## 本文档结构（深链）

| 目标 | 跳转 |
| --- | --- |
| 选型决策树、对比表 | [快速选择指南](#快速选择指南) |
| API 与队列签名 | [核心组件](#核心组件) |
| Left-Right / Hazard / RCU 原理摘要 | [核心原理](#核心原理) |
| 按业务场景举例 | [使用场景](#使用场景) |
| 读写比、数据量、设备相关讨论 | [性能分析与优化](#性能分析与优化) |
| 跑测例 | [测试与示例](#测试与示例)（`--gtest_filter=TestConcurrency.*`） |

**构建**：`gn gen out && ninja -C out //base:base_test`。

---

## 目录

- [架构总览（速读）](#架构总览速读)
- [本文档结构（深链）](#本文档结构深链)
- [概述](#概述)
- [快速开始](#快速开始)
- [快速选择指南](#快速选择指南)
- [核心组件](#核心组件)
  - [concurrent<T>](#concurrentt-并发包装器)
  - [队列组件](#队列组件)
  - [hazard_ptr](#hazard_ptr-内存安全回收)
  - [left_right](#left_right-双缓冲读写分离)
  - [rcu_ptr](#rcu_ptr-rcu-风格指针)
- [核心原理](#核心原理)
- [使用场景](#使用场景)
  - [读多写少场景](#读多写少场景)
  - [生产者-消费者场景](#生产者-消费者场景)
  - [Lock-free 数据结构](#lock-free-数据结构)
  - [配置与元数据管理](#配置与元数据管理)
  - [实时系统场景](#实时系统场景)
- [性能分析与优化](#性能分析与优化)
- [测试与示例](#测试与示例)
- [文件清单](#文件清单)

---

## 概述

`base/concurrency` 提供一组轻量且实用的并发/并行原语和容器，覆盖常见的并发场景：

- **读多写少的并发访问包装**：`concurrent<T>`, `left_right<T>`, `rcu_ptr<T>`
- **无锁/阻塞队列**：`NonblockingQueue`, `BlockingQueue`, `PushOnlyQueue`
- **Hazard Pointer 回收**：解决 lock-free 数据结构的内存安全问题
- **Left-Right 读写策略**：双缓冲实现 wait-free 读取
- **RCU 风格指针**：基于 `shared_ptr` 的轻量 RCU 实现

## 业界其他并发原语和容器

除了本库提供的组件外，业界还有以下常见的并发/并行原语和容器：

### 同步原语

- **互斥锁（Mutex）**：`std::mutex`, `std::recursive_mutex`, `std::timed_mutex`
- **读写锁（RWLock）**：`std::shared_mutex`, `std::shared_timed_mutex`
- **自旋锁（Spinlock）**：基于 CAS 的轻量锁，适合短临界区
- **信号量（Semaphore）**：`std::counting_semaphore`（C++20），控制并发访问数量
- **条件变量（Condition Variable）**：`std::condition_variable`, `std::condition_variable_any`
- **屏障（Barrier）**：`std::barrier`（C++20），同步多个线程到达某点
- **门闩（Latch）**：`std::latch`（C++20），一次性屏障
- **Future/Promise**：`std::future`, `std::promise`, `std::async`，异步编程

### 原子操作与内存序

- **原子类型**：`std::atomic<T>`, `std::atomic_flag`
- **原子操作**：`load`, `store`, `exchange`, `compare_exchange_weak/strong`, `fetch_add/sub`
- **内存序**：`memory_order_relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`
- **原子指针**：`std::atomic<T*>`, 用于 lock-free 数据结构

### 无锁数据结构

- **Lock-free 栈**：基于 CAS 的单向链表栈
- **Lock-free 队列**：
  - **Michael & Scott 队列**：经典 MPMC 无锁队列
  - **Bounded MPMC 队列**：固定容量环形缓冲区
  - **SPSC 队列**：单生产者单消费者，最高性能
- **Lock-free 哈希表**：
  - **ConcurrentHashMap**：分段锁或 lock-free 桶
  - **Lock-free Hash Map**：基于 CAS 的开放寻址或链式哈希
- **Lock-free 跳表（Skip List）**：有序并发数据结构
- **Lock-free 链表**：基于 CAS 的链表操作
- **Lock-free 树**：B+树、红黑树的 lock-free 变体

### 并发容器（标准库与第三方）

#### C++ 标准库（C++11+）

- **`std::atomic<T>`**：原子类型
- **`std::mutex` 系列**：互斥锁
- **`std::shared_mutex`**：读写锁
- **`std::condition_variable`**：条件变量
- **`std::future`/`std::promise`**：异步编程

#### 第三方库

- **Intel TBB（Threading Building Blocks）**：
  - `tbb::concurrent_hash_map`：并发哈希表
  - `tbb::concurrent_vector`：并发向量
  - `tbb::concurrent_queue`：并发队列
  - `tbb::concurrent_unordered_map`：并发无序映射
  - `tbb::parallel_for`：并行循环
  - `tbb::parallel_reduce`：并行归约
  - `tbb::flow_graph`：数据流图

- **Folly（Facebook）**：
  - `folly::ConcurrentHashMap`：高性能并发哈希表
  - `folly::MPMCQueue`：多生产者多消费者队列
  - `folly::AtomicHashMap`：原子哈希映射
  - `folly::LockFreeRingBuffer`：无锁环形缓冲区
  - `folly::Synchronized`：同步包装器

- **Boost**：
  - `boost::lockfree::queue`：无锁队列
  - `boost::lockfree::stack`：无锁栈
  - `boost::lockfree::spsc_queue`：SPSC 队列
  - `boost::sync_queue`：同步队列
  - `boost::concurrent_flat_map`（C++17）：并发扁平映射

- **libcds（Concurrent Data Structures）**：
  - 多种 lock-free 数据结构实现
  - 多种内存回收机制（Hazard Pointer, Epoch-based, 等）

- **moodycamel::ConcurrentQueue**：
  - 高性能 MPMC 无锁队列（本库已使用）

### 内存回收机制

- **Hazard Pointer**：本库已实现，适合 lock-free 数据结构
- **Epoch-based Reclamation（EBR）**：基于纪元的回收，性能优于 Hazard Pointer
- **Quiescent State Based Reclamation（QSBR）**：基于静止状态的回收
- **Reference Counting**：`std::shared_ptr`, `std::weak_ptr`
- **Garbage Collection**：自动内存管理（Java, Go 等语言）

### 并行算法与数据结构

- **并行排序**：`std::execution::par`（C++17），`tbb::parallel_sort`
- **并行归约**：`tbb::parallel_reduce`, `std::reduce`（C++17）
- **并行变换**：`std::transform`（并行执行策略）
- **并行查找**：`std::find_if`（并行执行策略）
- **并行累加**：`std::accumulate`（并行执行策略）
- **工作窃取队列**：`tbb::task_arena`, `tbb::task_group`

### 高级并发模式

- **Actor 模型**：消息传递并发（Akka, Erlang）
- **CSP（Communicating Sequential Processes）**：通道通信
- **协程（Coroutines）**：`std::coroutine`（C++20），异步/等待
- **Structured Concurrency**：结构化并发（C++23 提案）
- **Transactional Memory**：事务内存（硬件/软件支持）
- **Lock-free 引用计数**：`std::atomic<std::shared_ptr>`（C++20）

### 特定场景优化

- **无锁内存分配器**：`tbb::cache_aligned_allocator`, `jemalloc`
- **线程局部存储**：`thread_local`, `pthread_key_t`
- **NUMA 感知**：NUMA 节点亲和性优化
- **CPU 缓存优化**：缓存行对齐（`alignas(64)`），避免伪共享
- **内存屏障**：`std::atomic_thread_fence`, CPU 特定指令（`mfence`, `lfence`, `sfence`）

### 并发编程框架

- **OpenMP**：共享内存并行编程
- **MPI（Message Passing Interface）**：分布式内存并行
- **CUDA/OpenCL**：GPU 并行计算
- **std::thread**：C++11 线程库
- **std::execution**（C++17/20）：执行策略（并行、向量化）

### 性能分析工具

- **Valgrind Helgrind**：检测数据竞争
- **ThreadSanitizer（TSan）**：运行时数据竞争检测
- **Intel Inspector**：并发错误检测
- **perf**：Linux 性能分析工具

---

## 快速开始

### 包含头文件

```cpp
#include "base/concurrency/concurrent.h"    // 并发包装器
#include "base/concurrency/queue.h"         // 队列组件
#include "base/concurrency/hazard_ptr.h"     // Hazard Pointer
#include "base/concurrency/left_right.h"    // Left-Right 模式
#include "base/concurrency/rcu_ptr.h"       // RCU 指针
```

### 编译要求

- C++11/14/17 编译器
- 支持 `std::atomic`, `std::shared_ptr`, 线程库等现代特性
- `rcu_ptr` 需要 C++20（使用 `std::atomic<std::shared_ptr>`）

### 最小示例

```cpp
// 示例 1: 使用 left_right 实现配置管理
#include "base/concurrency/left_right.h"

base::left_right<std::map<std::string, std::string>> config;

// 读者：wait-free 读取
config.read([&](const auto& c) {
  std::cout << c.at("key") << std::endl;
});

// 写者：更新配置
config.update([&](auto& c) {
  c["key"] = "new_value";
});
```

```cpp
// 示例 2: 使用队列实现生产者-消费者
#include "base/concurrency/queue.h"

base::NonblockingQueue<int> queue;

// 生产者
queue.push(42);

// 消费者
int value;
if (queue.pop(value)) {
  std::cout << value << std::endl;
}
```

## 快速选择指南

根据你的使用场景快速选择合适的组件：

### 决策树

```
需要什么功能？
├─ 读多写少的共享数据？
│  ├─ 读写比 > 100:1 且数据 < 1MB？
│  │  └─ 使用 left_right ⭐⭐⭐⭐⭐
│  ├─ 读写比 10:1 ~ 100:1 或数据 > 1MB？
│  │  └─ 使用 rcu_ptr ⭐⭐⭐⭐
│  └─ 读写比 < 10:1？
│     └─ 使用 concurrent ⭐⭐⭐
│
├─ 生产者-消费者模式？
│  ├─ 需要阻塞等待？
│  │  └─ 使用 BlockingQueue ⭐⭐⭐⭐
│  ├─ 高吞吐无阻塞？
│  │  └─ 使用 NonblockingQueue ⭐⭐⭐⭐⭐
│  ├─ 只追加，单消费者 visit/clear？
│  │  └─ 使用 PushOnlyQueue ⭐⭐⭐⭐
│  └─ 只追加，导出时 drain/gc？
│     └─ 使用 concurrent_append_buffer ⭐⭐⭐⭐
│
├─ 分片独占写（无全局锁）？
│  └─ 使用 partition_exclusivity ⭐⭐⭐⭐
│
├─ Lock-free 数据结构内存管理？
│  └─ 使用 hazard_ptr ⭐⭐⭐⭐⭐
│
└─ 不确定？
   └─ 查看下面的详细场景说明
```

### 快速对比表

| 场景 | 推荐方案 | 读性能 | 写性能 | 内存开销 |
|------|----------|--------|--------|----------|
| **读多写少（> 50:1），小数据（< 1MB）** | `left_right` | ⭐⭐⭐⭐⭐ wait-free | ⭐⭐⭐ | 2x 数据 |
| **读多写少（> 10:1），大数据（> 1MB）** | `rcu_ptr` | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | 引用计数 |
| **中等读写比（< 10:1）** | `concurrent` | ⭐⭐⭐ | ⭐⭐⭐ | 最小 |
| **高吞吐生产者-消费者** | `NonblockingQueue` | - | ⭐⭐⭐⭐⭐ | 低 |
| **需要阻塞等待** | `BlockingQueue` | - | ⭐⭐⭐⭐ | 低 |
| **Lock-free 数据结构** | `hazard_ptr` | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | 每线程 1 指针 |

---

## 核心组件

### `concurrent<T>` 并发包装器

一个通用并发包装器，为被包装资源提供安全的读/写访问器（accessor）。

**API**:
```cpp
// 读访问：返回持锁的只读访问器，析构时自动释放锁
auto ra = base::concurrent<T>::make_read_accessor(obj);

// 写访问：返回持锁的写访问器，可修改资源
auto wa = base::concurrent<T>::make_write_accessor(obj);
```

**特点**:
- 基于读写锁（`std::shared_mutex`）
- 支持任意容器类型（`std::map`, `std::vector` 等）
- RAII 自动管理锁生命周期

**适用场景**: 简单场景、中等读写比（< 10:1）、小规模数据

---

### 队列组件

#### `NonblockingQueue<T>`

基于 moodycamel 的并发队列封装，提供高吞吐的无阻塞操作。

**API**:
```cpp
queue.push(item);                    // 非阻塞推入
queue.pop(item);                     // 非阻塞弹出
queue.push_bulk(items, count);       // 批量推入
queue.pop_bulk(items, count);       // 批量弹出
queue.size_approx();                 // 近似大小
```

**特点**:
- 无锁实现，高吞吐（单生产者单消费者可达 100M+ ops/s）
- 支持多生产者多消费者
- 批量操作减少系统调用

**适用场景**: 日志系统、事件总线、高吞吐数据管道

#### `BlockingQueue<T>`

基于 moodycamel 的阻塞队列，支持等待弹出。

**API**:
```cpp
queue.push(item);                                    // 非阻塞推入
queue.wait_pop(item);                               // 阻塞等待弹出
queue.wait_pop_timed(item, timeout);                // 超时等待弹出
```

**特点**:
- 支持阻塞等待，适合工作线程池
- 延迟略高于 `NonblockingQueue`（微秒级）

**适用场景**: 任务调度器、工作线程池、需要阻塞等待的场景

#### `PushOnlyQueue<T>`

自实现的无锁单向推入链表，只支持追加操作。

**API**:
```cpp
queue.push(item);                    // 无锁追加
queue.visit(fn);                     // 遍历所有元素
queue.clear();                       // 清空队列
```

**特点**:
- 写入延迟极低（< 10ns）
- 只追加，适合单消费者或周期性遍历

**适用场景**: 审计日志、指标收集、只追加场景

---

### `hazard_ptr` 内存安全回收

解决 lock-free 数据结构中"ABA 问题"和"use-after-free"的关键技术。

**核心原理**: 读者在访问对象前，将指针注册到全局 hazard list 中；写者在删除对象前，检查该对象是否在 hazard list 中，只有在没有任何线程持有该指针时才真正删除。

**API**:
```cpp
// 获取持有者，保护指针直到 holder 析构
auto holder = base::hazard_ptr<T>::acquire(atomic_ptr);

// 更新指针并退役旧对象（默认删除器）
base::hazard_ptr<T>::update(atomic_ptr, new_ptr);

// 更新指针并退役旧对象（自定义删除器）
base::hazard_ptr<T>::update(atomic_ptr, new_ptr, deleter);

// 执行显式回收
base::hazard_ptr<T>::reclaim();
```

**内存安全保证**:
- **双重检查模式**: 确保获取到一致的指针快照
- **延迟回收**: 退役对象加入 retire-list，等待所有持有者释放后才回收
- **阈值触发**: 当退役对象数量达到阈值（1000）时自动触发回收

**性能优化**:
- 资源池复用，避免频繁分配/释放
- 内存序优化（acquire-release 替代 seq_cst）
- 移动语义支持，减少复制
- 批量回收，提高效率

**适用场景**: Lock-free 栈、链表、队列、哈希表、跳表等需要手动内存管理的并发数据结构

---

### `left_right` 双缓冲读写分离

通过双缓冲（double buffering）实现读写分离，维护两个数据副本（left/right）。

**核心原理**:
- **读者 wait-free**: 只需原子读取指示器，时间复杂度 O(1)
- **写者不阻塞读者**: 更新非活跃副本，读者继续访问活跃副本
- **新数据立即可见**: 版本切换后，新读者立即看到更新后的数据

**版本切换流程**:
1. 写者获取写锁（互斥，保证单写者）
2. 更新非活跃副本（如当前读者在 left，则更新 right）
3. 切换指示器，引导新读者访问更新后的副本
4. 等待旧版本读者全部退出（通过 read_indicator 计数）
5. 更新旧副本，完成同步

**API**:
```cpp
base::left_right<T> lr(initial_value);

// 读操作：wait-free
lr.read([&](const auto& data) {
  // 只读访问
});

// 写操作：不阻塞读者
lr.update([&](auto& data) {
  // 修改数据
});
```

**性能优化**:
- 内存序优化（acquire-release 替代 seq_cst）
- 三级等待策略（自旋 → yield → 指数退避）
- 缓存对齐（64 字节），避免伪共享
- 位运算优化版本索引计算

**适用场景**: 配置数据、路由表、缓存、元数据等读多写少（读写比 > 50:1，数据 < 1MB）的场景

---

### `rcu_ptr` RCU 风格指针

使用 `std::shared_ptr<const T>` 封装的轻量 RCU 风格读写。

**API**:
```cpp
base::rcu_ptr<T> ptr;

// 读操作：返回 shared_ptr 快照
auto snapshot = ptr.read();

// 原子替换
ptr.reset(new_ptr);

// 复制-修改-原子替换
ptr.copy_update([&](auto* copy) {
  // 修改 copy
});
```

**特点**:
- 基于 `shared_ptr`，自动内存管理
- Copy-on-write 机制，平衡读写性能
- 支持大对象和复杂结构
- 零停机更新（创建新对象后原子替换）

**适用场景**: 大对象、复杂结构、读多写少（读写比 > 10:1，数据 > 1MB）的场景

---

## 核心原理

### Left-Right 模式

通过**双缓冲**实现读写分离：维护两个数据副本，读者访问活跃副本，写者更新非活跃副本后切换。

**关键保证**:
- 读者 wait-free（O(1) 原子读取指示器）
- 写者不阻塞读者（更新非活跃副本）
- 新数据立即可见（版本切换后新读者看到更新）

**版本切换流程**: 更新非活跃副本 → 切换指示器 → 等待旧读者退出 → 更新旧副本完成同步

### Hazard Pointer

解决 lock-free 数据结构中的**内存回收问题**：读者访问前注册指针到全局 hazard list，写者删除前检查是否被保护。

**双重检查模式**: 确保获取一致的指针快照，避免访问已释放内存

**延迟回收机制**: 退役对象加入 retire-list，达到阈值（1000）时批量回收，检查是否在活跃 hazard list 中

### RCU (Read-Copy-Update)

**核心思想**: 读者获取数据的快照（`shared_ptr`），写者创建新版本后原子替换，旧版本由引用计数自动回收。

**优势**:
- 读者无锁，性能高
- 支持零停机更新
- 自动内存管理

---

## 使用场景

### 读多写少场景

#### 场景 1: 配置管理

**典型场景**: 应用配置、功能开关、动态参数

**特点**: 读写比 > 1000:1，数据量 < 10KB

**推荐方案**: `left_right`

```cpp
base::left_right<std::map<std::string, std::string>> config;

// 读者：wait-free，高频访问
config.read([&](const auto& c) {
  auto value = c.find("key");
  if (value != c.end()) {
    // 使用配置
  }
});

// 写者：低频更新，不阻塞读者
config.update([&](auto& c) {
  c["key"] = "value";
});
```

#### 场景 2: 服务发现与路由表

**典型场景**: 微服务注册中心、负载均衡路由表、API 网关

**特点**: 读写比 > 100:1，数据量中等（数千到数万条）

**推荐方案**: `left_right`（小规模）或 `rcu_ptr`（大规模）

```cpp
// 小规模路由表
base::left_right<std::unordered_map<std::string, RouteInfo>> route_table;

// 大规模服务注册表
base::rcu_ptr<std::unordered_map<std::string, std::vector<ServiceInstance>>> service_registry;
```

#### 场景 3: 索引与查找表

**典型场景**: 倒排索引、前缀树、搜索引擎索引

**特点**: 读写比 > 100:1，数据量可能很大（GB 级）

**推荐方案**:
- 小到中等索引（< 100MB）：`left_right`
- 大索引（> 100MB）：`rcu_ptr`

```cpp
// 小规模索引
base::left_right<InvertedIndex> search_index;

// 大规模索引
base::rcu_ptr<LargeIndex> large_index;
```

### 生产者-消费者场景

#### 场景 4: 日志系统

**典型场景**: 高吞吐日志、事件总线、数据采集

**特点**: 多生产者，单消费者批量处理

**推荐方案**: `NonblockingQueue`

```cpp
base::NonblockingQueue<LogEntry> log_queue;

// 生产者：多个线程并发写入
void log_worker() {
  LogEntry entry;
  // ... 填充 entry
  log_queue.push(entry);  // 无阻塞，高吞吐
}

// 消费者：批量处理
void log_processor() {
  std::vector<LogEntry> batch;
  batch.reserve(1000);

  while (running) {
    size_t count = log_queue.pop_bulk(batch.begin(), 1000);
    if (count > 0) {
      write_to_disk(batch.data(), count);
      batch.clear();
    }
  }
}
```

#### 场景 5: 任务调度器

**典型场景**: 工作线程池、任务队列

**特点**: 需要阻塞等待任务

**推荐方案**: `BlockingQueue`

```cpp
base::BlockingQueue<Task> task_queue;

// 生产者：快速提交任务
void submit_task(const Task& task) {
  task_queue.push(task);
}

// 消费者：工作线程阻塞等待
void worker_thread() {
  Task task;
  while (true) {
    if (task_queue.wait_pop_timed(task, std::chrono::seconds(5))) {
      process_task(task);
    } else {
      if (should_shutdown()) break;
    }
  }
}
```

#### 场景 6: 指标收集

**典型场景**: 审计日志、性能指标、实时统计

**特点**: 只追加，定期批量处理

**推荐方案**: `PushOnlyQueue`

```cpp
base::PushOnlyQueue<Metric> metric_queue;

// 生产者：快速记录指标
void record_metric(const std::string& name, double value) {
  Metric m{name, value, get_timestamp()};
  metric_queue.push(m);  // 无锁追加，极低延迟
}

// 消费者：定期聚合
void aggregate_metrics() {
  std::map<std::string, std::vector<double>> metrics;

  metric_queue.visit([&metrics](const Metric& m) {
    metrics[m.name].push_back(m.value);
  });

  // 计算统计信息
  // ...

  metric_queue.clear();
}
```

### Lock-free 数据结构

#### 场景 7: Lock-free 栈/链表

**典型场景**: 高性能数据结构、无锁编程

**特点**: 需要安全的内存回收

**推荐方案**: `hazard_ptr`

```cpp
std::atomic<Node*> head{nullptr};

// 读者：保护指针，安全访问
auto holder = base::hazard_ptr<Node>::acquire(head);
if (holder) {
  int value = holder->data;
}

// 写者：更新并退役旧节点
base::hazard_ptr<Node>::update(head, new_node);

// 定期回收
base::hazard_ptr<Node>::reclaim();
```

### 配置与元数据管理

#### 场景 8: 配置热更新

**典型场景**: 功能开关、限流规则、A/B 测试配置

**特点**: 读写比 > 1000:1，要求零停机更新

**推荐方案**: `left_right` 或 `rcu_ptr`

```cpp
// 功能开关
base::left_right<std::unordered_map<std::string, FeatureConfig>> feature_flags;

// 应用配置（大数据量）
base::rcu_ptr<AppConfig> app_config;

// 热更新：不阻塞请求
void hot_reload_config(const AppConfig& new_config) {
  auto new_config_ptr = std::make_shared<AppConfig>(new_config);
  app_config.reset(new_config_ptr);  // 原子替换
}
```

#### 场景 9: 缓存与元数据

**典型场景**: 分布式缓存元数据、特征存储元数据

**特点**: 读写比 > 100:1，数据量小到中等

**推荐方案**:
- 小数据量（< 1MB）：`left_right`
- 大数据量或复杂结构：`rcu_ptr`

### 实时系统场景

#### 场景 10: 实时监控与统计

**典型场景**: APM、Prometheus 指标、实时告警

**特点**: 读写比 5:1 到 20:1

**推荐方案**: `concurrent`（读写锁）

```cpp
base::concurrent<std::map<std::string, MetricValue>> metrics;

// 写者：高频更新
void record_metric(const std::string& name, double value) {
  auto wa = base::concurrent<std::map<std::string, MetricValue>>::make_write_accessor(metrics);
  auto& metric = (*wa)[name];
  metric.count++;
  metric.sum += value;
}

// 读者：监控面板查询
std::map<std::string, MetricValue> get_all_metrics() {
  auto ra = base::concurrent<std::map<std::string, MetricValue>>::make_read_accessor(metrics);
  return *ra;
}
```

#### 场景 11: 游戏服务器状态同步

**典型场景**: 多人在线游戏、实时对战

**特点**: 读写比 > 50:1，延迟要求 < 1ms

**推荐方案**: `left_right` 或 `rcu_ptr`

```cpp
// 游戏房间状态
base::left_right<GameRoomState> room_state;

// 游戏循环：60 FPS
void game_loop() {
  while (running) {
    room_state.read([&](const auto& state) {
      simulate_physics(state);
      check_collisions(state);
      broadcast_to_clients(state);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
}
```

#### 场景 12: 实时数据流处理

**典型场景**: 流式数据处理、事件流聚合、CEP

**特点**: 极高写频率，需要低延迟处理

**推荐方案**: `PushOnlyQueue` + `left_right`

```cpp
// 使用 PushOnlyQueue 收集事件
base::PushOnlyQueue<Event> event_queue;

// 使用 left_right 存储窗口聚合结果
base::left_right<std::map<std::string, WindowStats>> window_stats;

// 生产者：快速写入
void ingest_event(const Event& event) {
  event_queue.push(event);
}

// 消费者：定期聚合
void process_window() {
  // 收集并处理事件
  // 更新窗口统计
  window_stats.update([&](auto& stats) {
    // 更新统计
  });
}
```

---

## 性能分析与优化

### 性能特征对比

| 组件 | 读操作 | 写操作 | 内存开销 | 适用读写比 |
|------|--------|--------|----------|------------|
| `left_right` | O(1) wait-free | O(1) + 等待读者 | 2x 数据大小 | > 50:1 |
| `hazard_ptr` | O(1) 双重检查 | O(1) + 延迟回收 | 每线程 1 个指针 | 任意 |
| `rcu_ptr` | O(1) shared_ptr | O(1) copy-on-write | 引用计数 | > 10:1 |
| `concurrent` | 读写锁 | 读写锁 | 最小 | < 10:1 |
| `NonblockingQueue` | - | 无锁，高吞吐 | 低 | - |
| `BlockingQueue` | - | 阻塞等待 | 低 | - |
| `PushOnlyQueue` | - | 无锁追加 | 低 | - |

### 读写比选择指南

| 读写比 | 推荐方案 | 原因分析 |
|--------|----------|----------|
| **> 100:1** | `left_right` | wait-free 读取，写操作等待成本可接受 |
| **50:1 ~ 100:1** | `left_right` 或 `rcu_ptr` | 根据数据大小选择：小数据用 `left_right`，大数据用 `rcu_ptr` |
| **20:1 ~ 50:1** | `rcu_ptr` | copy-on-write 平衡读写性能，避免写操作长时间等待 |
| **10:1 ~ 20:1** | `rcu_ptr` 或 `concurrent` | 读写锁开始有竞争力，但 `rcu_ptr` 仍可能更优 |
| **5:1 ~ 10:1** | `concurrent` | 读写锁适合中等读写比，实现简单 |
| **< 5:1** | `concurrent` | 写操作频繁，互斥锁或读写锁更合适 |

### 数据量选择指南

| 数据大小 | 推荐方案 | 内存开销 | 选择理由 |
|----------|----------|----------|----------|
| **< 1KB** | `left_right` | 2KB | 双倍内存可忽略，wait-free 读取优势明显 |
| **1KB ~ 10KB** | `left_right` | 2x | 内存开销可接受，优先考虑读性能 |
| **10KB ~ 100KB** | `left_right` 或 `rcu_ptr` | 2x 或 引用计数 | 根据读写比选择 |
| **100KB ~ 1MB** | `rcu_ptr` | 引用计数 | 避免双倍内存，copy-on-write 更灵活 |
| **1MB ~ 10MB** | `rcu_ptr` | 引用计数 | 双倍内存成本显著，`rcu_ptr` 更经济 |
| **> 10MB** | `rcu_ptr` | 引用计数 | 必须避免双倍内存，`rcu_ptr` 唯一选择 |

### 设备差异分析

#### 内存（MEM）

**特性**: 访问延迟 ~100ns，带宽 GB/s 级

**推荐**:
- 小数据（< 1MB）：`left_right` - 充分利用 wait-free 读取
- 大数据（> 1MB）：`rcu_ptr` - 避免双倍内存开销

#### SSD（固态硬盘）

**特性**: 访问延迟 ~100μs，写入有磨损

**推荐**: `rcu_ptr` - 最小化 SSD 写入，支持增量更新

**优化建议**:
- 使用写缓冲：批量写入 SSD
- 异步持久化：后台线程定期同步
- 考虑使用内存映射文件（mmap）结合 `rcu_ptr`

#### GPU 内存（GPUMEM）

**特性**: 访问延迟 ~100ns + PCIe，传输成本高

**推荐**: `rcu_ptr` + GPU 优化

**优化建议**:
- 批量传输：累积多个更新后一次性传输
- 异步传输：使用 CUDA streams
- 零拷贝：使用 CUDA unified memory

### 性能优化要点

1. **内存序优化**: 使用 acquire-release 替代 seq_cst，性能提升 20-30%
2. **缓存对齐**: `alignas(64)` 避免伪共享，多核性能提升 20-40%
3. **调优建议**:
   - `hazard_ptr`: 根据退役速率调整阈值（500-2000），高负载时手动触发回收
   - `left_right`: 写操作频率 < 10%，数据 < 1MB，否则考虑 `rcu_ptr`
   - `NonblockingQueue`: 使用批量操作（`push_bulk`/`pop_bulk`）提高吞吐

---

## 测试与示例

### 测试文件

`base/testing/concurrency/concurrency_unittest.cpp` 包含完整示例与单元测试：

- `test_concurrent`: 演示 `base::concurrent` 的读写协作
- `test_nonblocking_queue` / `test_blocking_queue`: 队列的生产者-消费者示例
- `test_hazard_ptr`: hazard pointer 的高并发写/读压力测试与回收演示
- `test_left_right`: Left-Right 模式的并发读写正确性验证
- `test_rcu_ptr`: RCU 指针的并发读写示例

### 运行测试

测例在聚合目标 `//base:base_test` 中。

```bash
gn gen out
ninja -C out //base:base_test
./out/base_test --gtest_filter=TestConcurrency.*
```

---

## 文件清单

- `concurrent.h`: 并发包装器与 accessor 定义
- `queue.h`: 无锁/阻塞队列、`PushOnlyQueue`、`AppendOnlyConcurrentQueue`
- `append_buffer.h`: `concurrent_append_buffer`（drain 语义包装）
- `partition.h`: `partition_exclusivity` / `scoped_partition_write`
- `hazard_ptr.h`: hazard pointer 回收实现
- `left_right.h`: Left-Right 读写策略实现
- `rcu_ptr.h`: 基于 `shared_ptr` 的 RCU 风格封装
- `testing/concurrency/concurrency_unittest.cpp`: 单元测试与示例
- `../testing/concurrency/append_spin_unittest.cpp`: append/drain、partition、spin、codec
- `doc/concurrency.xmind`: 逻辑结构思维导图（仓库内的设计文档）

---

**文档修订：** 2026-09-05（补充 `concurrent_append_buffer` / `partition` 与 `PushOnlyQueue` 选型；正文 API 以源码为准。）

# base/container - 高级容器和数据结构

## 概述

`base/container` 模块提供了一系列高性能的容器和数据结构，超越标准 STL。包括双缓冲、LRU 缓存、环形缓冲、有序映射、张量等专用数据结构，支持现代 C++ 特性（concepts、constexpr 等）。

## 核心容器

### 1. double_buffer - 双缓冲

**用途**: 无锁的读写分离，适合频繁更新的数据

```cpp
template <typename T>
class double_buffer {
    // 检查是否有有效数据
    bool empty() const;

    // 加载当前数据
    std::shared_ptr<T> load() const;

    // 更新数据（写入另一个缓冲）
    void update(T&& data);
};
```

**特点**:
- 原子操作，不需要显式锁
- 读操作永远成功（不阻塞）
- 写操作快速（O(1)，不等待读）
- 适合配置、缓存、统计等频繁读写的场景

**使用示例**:
```cpp
base::double_buffer<std::vector<int>> cache;

// 线程 1: 读取
auto data = cache.load();
for (int x : *data) {
    process(x);
}

// 线程 2: 更新
std::vector<int> new_data = {1, 2, 3, 4, 5};
cache.update(std::move(new_data));
```

### 2. lru_cache - LRU 缓存

**用途**: 容量有限的高速缓存，自动淘汰最少使用的项

```cpp
template <typename K, typename T>
class lru_cache {
    // 放入缓存
    void put(const K& key, const T& value);

    // 获取缓存值（不存在抛出异常）
    const T& get(const K& key);

    // 检查是否存在
    bool exists(const K& key) const;

    // 获取缓存大小
    size_t size() const;
};
```

**特点**:
- O(1) 插入、查询、删除
- 自动 LRU 淘汰
- 线程不安全（需外部同步）
- 固定最大容量

**使用示例**:
```cpp
base::lru_cache<std::string, std::string> cache(1000);

// 缓存查询结果
if (!cache.exists(key)) {
    auto result = expensive_query(key);
    cache.put(key, result);
}

auto cached = cache.get(key);
```

### 3. ring_buffer - 环形缓冲

**用途**: 固定大小的环形队列，FIFO 操作

```cpp
template <typename T, size_t N>
class ring_buffer {
    // 将元素加入缓冲
    bool push(T&& t);
    bool push(const T& t);

    // 从缓冲取出元素
    std::optional<T> pop();

    // 检查大小和是否为空
    auto size() const noexcept;
    bool empty() const noexcept;
};
```

**特点**:
- 固定大小，编译期确定（模板参数）
- 原子 push/pop，支持多线程
- 满时 push 返回 false
- 空时 pop 返回 std::nullopt

**使用示例**:
```cpp
base::ring_buffer<int, 1024> buffer;

// 生产者线程
for (int i = 0; i < 10000; ++i) {
    if (!buffer.push(i)) {
        // 缓冲满，等待消费者
        std::this_thread::yield();
    }
}

// 消费者线程
while (true) {
    auto item = buffer.pop();
    if (item) {
        process(*item);
    }
}
```

### 4. ordered_map - 有序映射

**用途**: 保持插入顺序的 O(1) 映射

```cpp
template <class Key, class T, class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>>
class ordered_map {
    bool empty() const;
    size_t size() const;

    iterator find(const Key& key);
    std::pair<iterator, bool> insert(iterator it, value_type val);
    iterator erase(iterator it);

    void move(iterator from, iterator to);  // 改变插入顺序
    void clear();
};
```

**特点**:
- 保持插入顺序
- O(1) 查找、插入、删除
- 支持通过迭代器移动元素
- 非线程安全

**使用示例**:
```cpp
base::ordered_map<std::string, int> map;

map.insert(map.end(), {"a", 1});
map.insert(map.end(), {"b", 2});
map.insert(map.end(), {"c", 3});

// 迭代按插入顺序
for (auto& [key, value] : map) {
    std::cout << key << ": " << value << std::endl;
}
// 输出: a: 1, b: 2, c: 3
```

### 5. tensor - 多维张量

**用途**: 编译期确定维度的多维数组，支持高效遍历

```cpp
template <typename T, size_t M, size_t... Ms>
requires(M > 0)
struct tensor {
    static constexpr size_t arity = M;

    auto& operator[](size_t k);
    auto& operator[](size_t i, Is&&... is);  // 多维索引

    auto front();
    auto back();

    auto begin();
    auto end();

    // 遍历所有元素，支持访问状态回调
    template <typename Fn, typename StatusFn>
    constexpr decltype(auto) for_each(Fn&& fn, StatusFn&& status_fn);
};
```

**特点**:
- 编译期维度检查
- constexpr 支持
- 多维索引语法
- 状态回调机制

**使用示例**:
```cpp
// 定义 3x4x5 的张量
base::tensor<int, 3, 4, 5> t3d;

// 访问元素
t3d[0][1][2] = 42;
int val = t3d[0, 1, 2];

// 遍历所有元素
t3d.for_each(
    [](int& val) { val *= 2; },  // 元素处理
    [](base::VisitStatus status) {  // 状态回调
        if (status == base::VisitStatus::dim_begin) {
            std::cout << "[";
        } else if (status == base::VisitStatus::dim_end) {
            std::cout << "]\n";
        }
    }
);
```

### 6. enumerate - 带索引的容器迭代

**用途**: Python 风格的 enumerate，获取元素索引和值

```cpp
template <Container T>
constexpr auto enumerate(T&& container);
```

**特点**:
- 零开销抽象
- constexpr 支持
- 兼容所有 STL 容器
- 基于 Concepts 约束

**使用示例**:
```cpp
std::vector<std::string> names = {"Alice", "Bob", "Charlie"};

for (auto [idx, name] : base::enumerate(names)) {
    std::cout << idx << ": " << name << std::endl;
}
// 输出:
// 0: Alice
// 1: Bob
// 2: Charlie
```

### 7. BloomFilter（`bloom_filter.h`）

**用途**：对 `uint64_t` 键的简易布隆过滤器，用于**前置过滤**（允许假阳性、不可依赖假阴性语义以外的保证）。

**API 概要**：`init(bit_size, hash_count)` → `add(key)` / `might_contain(key)` / `clear()`。未 `init` 时 `might_contain` 约定为不拦截（避免空过滤器误杀）。

**线程安全**：非线程安全，多线程需外部同步或每线程一份实例。

### 8. variant_tree（`variant_tree.h`）

**用途**：在 `std::variant` 上扩展出「叶子类型… **或** `vector<variant_tree>` 子树」的递归结构，适合配置树、表达式树等半结构化数据。

**API 概要**：`variant_tree<Ts...>` 继承自 `variant<Ts..., vector<variant_tree<Ts...>>>`；`reduce_tree` 配合 `base::traits::overloaded` 对叶子与分支分别归约。

**线程安全**：类型本身未内置同步；若共享需外部保护。

### 9. 其他头文件

| 头文件 | 说明 |
| --- | --- |
| `comprehension.h` | 列表推导式风格辅助 |
| `concept.h` | 容器相关 concepts |
| `iterator.h` | 迭代器适配与工具 |
| `reserved.h` | 预留容量等辅助 |

**与 `concurrency` 的关系**：本目录 `double_buffer` 为原子快照式双缓冲；读多写少且需 wait-free 读路径时还可对比 `base/concurrency/left_right.h`、`rcu_ptr.h` 的语义与内存开销（见 `concurrency/README.md`）。

## 设计模式

### RAII 原则
所有容器都遵循 RAII，自动管理资源生命周期

### 零拷贝/移动语义
广泛使用右值引用和移动构造

### constexpr 优先
尽可能使用 constexpr，支持编译期计算

### Concepts 约束
使用 C++20 Concepts 进行类型检查

## 性能特性

| 容器 | 查找 | 插入 | 删除 | 备注 |
|------|------|------|------|------|
| double_buffer | O(1) | O(1) | - | 原子操作 |
| lru_cache | O(1) | O(1) | O(1) | 需同步 |
| ring_buffer | O(1) | O(1) | O(1) | 固定大小 |
| ordered_map | O(1) | O(1) | O(1) | 保持顺序 |
| tensor | O(1) | O(1) | - | 固定大小 |

## 线程安全性

| 容器 | 线程安全 | 备注 |
|------|---------|------|
| double_buffer | ✓ | 原子操作 |
| lru_cache | ✗ | 需外部同步 |
| ring_buffer | ✓ | 原子操作 |
| ordered_map | ✗ | 需外部同步 |
| tensor | ✗ | 需外部同步 |

## 使用建议

1. **频繁读写配置**: 使用 `double_buffer`
2. **热数据缓存**: 使用 `lru_cache`
3. **生产者-消费者**: 使用 `ring_buffer`
4. **有序键值对**: 使用 `ordered_map`
5. **多维数据**: 使用 `tensor`
6. **带索引遍历**: 使用 `enumerate`

## 与其他模块的关系

| 模块 | 用途 |
|------|------|
| `base/traits` | Concepts 和特性检测 |
| `base/concurrency` | 并发原语 |
| `base/tuple` | 元组操作 |
| `base/memory` | 内存管理 |

## 许可证

Copyright (c) 2018-2026 The Mogu Authors. All rights reserved.

---

**最后更新：** 2026-04-03
**版本：** 1.0.0
**状态：** 生产就绪

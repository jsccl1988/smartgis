# Tuple 模块文档

## 概述

`base/tuple` 模块提供了对 `std::tuple` 的扩展工具与便捷包装，包括编译期 tuple 类型操作、命名/标记元组以及函数式算法辅助。该模块主要用于模板元编程场景，让在编译期操作多种元素类型变得便利。

## 构建与测试

测例在 `//base:base_test` 中（`tuple/*_unittest.cpp`）。

```bash
gn gen out
ninja -C out //base:base_test
./out/base_test --gtest_filter=TestTuple.*:TestNamedTuple.*:TestTaggedTuple.*:TestAliasedTuple.*
```

## 快速开始

### 包含头文件

```cpp
#include "base/tuple/tuple.h"
```

### 基本使用

```cpp
using namespace base;

// 1. 标准 tuple 操作
auto t = std::make_tuple(1, 2.0, "hello");
base::tuple::for_each(t, [](auto&& v) {
    std::cout << v << std::endl;
});

// 2. 命名元组（通过字符串字面量访问）
auto record = make_named_tuple<"Trade">("price"_t = 42, "size"_t = 100);
std::cout << record["price"_t] << std::endl;  // 42

// 3. 标记元组（通过类型标签访问）
struct PriceTag {};
struct SizeTag {};
auto trade = tagged_tuple{
    tag_resolver<PriceTag> = 42,
    tag_resolver<SizeTag> = 100
};
std::cout << tag_resolver<PriceTag>(trade) << std::endl;  // 42
```

## 核心组件

### 1. traits.h - Tuple 类型特征

提供编译期 tuple 类型操作，包括切片、反转、push/pop 等。

**主要功能：**

```cpp
using Tuple = std::tuple<int, double, float>;

// 获取元素数量
static_assert(tuple_tratis<Tuple>::arity == 3);

// 类型选择
using First = tuple_tratis<Tuple>::select_t<0>;  // int

// 类型切片
using Slice = tuple_tratis<Tuple>::range_t<1, 2>;  // std::tuple<double, float>

// 反转
using Reversed = tuple_tratis<Tuple>::reverse_t;  // std::tuple<float, double, int>

// 头部/尾部
using Front = tuple_tratis<Tuple>::front_t;  // std::tuple<int>
using Tail = tuple_tratis<Tuple>::tail_t;    // std::tuple<double, float>

// Push/Pop 操作
using Pushed = tuple_tratis<Tuple>::push_front_t<char>;  // std::tuple<char, int, double, float>
using Popped = tuple_tratis<Tuple>::pop_front_t;         // std::tuple<double, float>
```

**使用示例：**

```cpp
using Tuple = std::tuple<int, double, float>;
using Reversed = tuple_tratis<Tuple>::reverse_t;
static_assert(std::is_same_v<Reversed, std::tuple<float, double, int>>);
```

### 2. named_tuple.h - 命名元组

通过字符串字面量访问 tuple 元素，提供类似结构体的访问方式。

**核心特性：**
- 使用 C++20 `fstring` 实现编译期字符串
- 支持嵌套命名元组
- 支持 JSON 序列化

**使用示例：**

```cpp
// 创建命名元组
auto record = make_named_tuple<"Trade">("price"_t = 42, "size"_t = 100);

// 访问元素
std::cout << record["price"_t] << std::endl;  // 42
record["size"_t] = 200;

// 赋值操作
auto nt = make_named_tuple("price"_t = int{}, "size"_t = std::size_t{});
nt.assign(42, 99u);  // 按位置赋值
nt.assign("price"_t = 11, "size"_t = 1234ul);  // 按名称赋值

// 嵌套使用
auto name = make_named_tuple("first"_t, "last"_t);
auto person = make_named_tuple<"Person">("name"_t = name, "age"_t);
person["name"_t]["first"_t] = "John"sv;
person["name"_t]["last"_t] = "Doe"sv;
person["age"_t] = 30;

// JSON 序列化
std::cout << base::named::jsonify(record) << std::endl;
```

### 3. tagged_tuple.h - 标记元组

通过类型标签访问 tuple 元素，类型安全且支持编译期优化。

**核心特性：**
- 使用类型标签而非字符串，编译期开销更小
- 支持 JSON 序列化
- 支持转换为字符串列表

**使用示例：**

```cpp
// 定义标签类型
class PriceTag {};
class SizeTag {};
class SymbolTag {};

// 创建标记元组
auto trade = tagged_tuple{
    tag_resolver<PriceTag> = 42.5,
    tag_resolver<SizeTag> = 100,
    tag_resolver<SymbolTag> = std::string("AAPL")
};

// 访问元素
auto price = tag_resolver<PriceTag>(trade);
std::cout << price << std::endl;  // 42.5

// 遍历元素
base::tagged::for_each(trade, [](auto&& element) {
    std::cout << element.tag_name << ": " << element.value << std::endl;
});

// JSON 序列化
std::cout << base::tagged::jsonify(trade) << std::endl;

// 转换为字符串列表
auto strings = base::tagged::as_string_list(trade);
```

### 4. aliased_tuple.h - 别名元组

通过类型别名访问 tuple 元素，在编译期检查别名唯一性。

**使用示例：**

```cpp
using namespace base;

// 定义别名
using user_t = aliased_tuple<
    aliased<class name, std::string>,
    aliased<class age, int>
>;

// 创建和使用
user_t user{"Alice", 25};
std::cout << get<name>(user) << std::endl;  // Alice
std::cout << get<age>(user) << std::endl;   // 25

// 仍然支持索引访问
std::cout << get<0>(user) << std::endl;  // Alice

// 与标准 tuple 互转
std::tuple<std::string, int> regular_tuple{user};
user_t another_user{regular_tuple};
```

### 5. algorithm.h - Tuple 算法

提供函数式风格的 tuple 操作，包括 map、filter、for_each 等。

**核心算法：**

#### 5.1 遍历操作

```cpp
auto t = std::make_tuple(1, 2.0, "hello");

// for_each - 遍历所有元素
base::tuple::for_each(t, [](auto&& v) {
    std::cout << v << std::endl;
});

// enumerate - 带索引的遍历
base::tuple::enumerate(t, [](auto&& pair) {
    auto value = std::get<0>(pair);
    auto index = std::get<1>(pair);
    std::cout << index::value << ": " << value << std::endl;
});

// for_each_with_n - 模板参数索引
base::tuple::for_each_with_n(t, []<size_t N>(auto&& v) {
    std::cout << N << ": " << v << std::endl;
});
```

#### 5.2 转换操作

```cpp
// map - 转换每个元素
auto doubled = base::tuple::map(
    std::make_tuple(1, 2, 3),
    [](auto n) { return n * 2; }
);
// 结果: std::tuple<int, int, int>(2, 4, 6)

// filter - 编译期过滤（consteval）
constexpr auto filtered = base::tuple::filter(
    []() { return std::tuple(1, 2, 3); },
    [](auto i) { return i < 2; }
);
// 结果: std::tuple(1)
```

#### 5.3 查找操作

```cpp
auto t = std::make_tuple(2, 3, 5, 6, 9);

// find_if - 查找第一个满足条件的索引
auto index = base::tuple::find_if(t, [](int n) {
    return n % 2 == 0;
});
// 返回: 0 (第一个偶数是 2，索引为 0)

// all_of - 是否所有元素都满足条件
bool all_odd = base::tuple::all_of(t, [](int n) {
    return n % 2 == 1;
});
// 返回: false

// any_of - 是否有元素满足条件
bool has_even = base::tuple::any_of(t, [](int n) {
    return n % 2 == 0;
});
// 返回: true

// none_of - 是否没有元素满足条件
bool no_even = base::tuple::none_of(t, [](int n) {
    return n % 2 == 0;
});
// 返回: false
```

#### 5.4 类型操作

```cpp
// tuple_index - 获取类型在 tuple 中的索引
auto t = std::make_tuple(1, 2.0, "hello");
auto idx = base::tuple::tuple_index<double>(t);  // 1

// select - 选择指定索引的元素
auto selected = base::tuple::select(
    t,
    std::index_sequence<0, 2>{}
);
// 结果: std::tuple<int, const char*>

// 连接操作符
auto t1 = std::make_tuple(1, 2);
auto t2 = std::make_tuple(3, 4);
auto combined = t1 | t2;  // std::tuple(1, 2, 3, 4)
```

#### 5.5 类型检查

```cpp
// is_unique_elements - 检查 tuple 元素类型是否唯一
using unique_t = std::tuple<int, long, std::string>;
using non_unique_t = std::tuple<int, int, int>;

static_assert(base::tuple::is_unique_elements_v<unique_t>);
static_assert(!base::tuple::is_unique_elements_v<non_unique_t>);

// is_subset_of - 检查类型子集关系
using t1 = std::tuple<int, double>;
using t2 = std::tuple<int, double, float>;
static_assert(base::tuple::is_subset_of_v<t1, t2>);
```

#### 5.6 其他工具

```cpp
// jsonify - JSON 序列化
auto t = std::make_tuple(1, 2.0, "hello");
std::cout << base::tuple::jsonify(t) << std::endl;

// to_array - 转换为 variant 数组
auto arr = base::tuple::to_array(t);
// 结果: std::array<std::variant<int, double, const char*>, 3>

// unfold - 展开操作
base::tuple::unfold<5>([]() {
    std::cout << "Hello" << std::endl;
});
// 打印 5 次 "Hello"
```

### 6. concept.h - C++20 Concepts

提供 tuple-like 类型的概念检查。

```cpp
// tuple_like - 检查类型是否类似 tuple
static_assert(tuple_like<std::tuple<>>);
static_assert(tuple_like<std::tuple<int, int>>);
static_assert(tuple_like<std::pair<int, int>>);
static_assert(tuple_like<std::array<int, 3>>);
static_assert(!tuple_like<int>);
```

## 使用场景

### 1. 编译期类型操作

当需要在模板代码中操作多种类型时，使用 `tuple_tratis` 进行类型变换：

```cpp
template<typename... Ts>
using reversed_types = tuple_tratis<std::tuple<Ts...>>::reverse_t;
```

### 2. 结构化数据访问

使用 `named_tuple` 或 `tagged_tuple` 替代普通 tuple，提高代码可读性：

```cpp
// 替代 std::tuple<int, int, std::string>
auto trade = make_named_tuple(
    "price"_t = 42,
    "size"_t = 100,
    "symbol"_t = "AAPL"
);
// 更清晰的访问方式
auto price = trade["price"_t];
```

### 3. 函数式编程

使用算法函数进行 tuple 的转换和操作：

```cpp
auto processed = base::tuple::map(
    input_tuple,
    [](auto&& v) { return process(v); }
);
```

### 4. 序列化/反序列化

利用 JSON 序列化功能：

```cpp
auto data = make_named_tuple("name"_t = "John", "age"_t = 30);
std::string json = base::named::jsonify(data);
```

## 性能考虑

1. **编译期开销**：这些工具主要是编译期元编程，会增加编译时间和模板实例化深度
2. **运行时性能**：大部分操作在编译期完成，运行时开销很小
3. **内存布局**：`named_tuple` 和 `tagged_tuple` 使用继承而非组合，内存布局与标准 tuple 相同
4. **建议**：在性能敏感场景中，优先使用 `tagged_tuple`（类型标签）而非 `named_tuple`（字符串）

## 注意事项

1. **编译器支持**：需要 C++20 支持（特别是 `fstring` 和 concepts）
2. **模板深度**：复杂操作可能导致模板实例化深度增加，注意编译器限制
3. **类型安全**：命名访问在编译期检查，但运行时错误可能难以调试
4. **可维护性**：元编程代码可读性较差，建议添加详细注释

## 测试示例

详细的测试用例请参考：
- `tuple_unittest.cpp` - 基础 tuple 操作测试
- `named_tuple_unittest.cpp` - 命名元组测试
- `tagged_tuple_unittest.cpp` - 标记元组测试
- `aliased_tuple_unittest.cpp` - 别名元组测试

## 相关模块

- `base/traits` - 类型特征工具
- `base/json` - JSON 序列化支持
- `base/aggregate` - 聚合类型反射（与 tuple 转换）

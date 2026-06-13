# C++ class 与内存模型

## 1. 是什么

`class` 是 C++ 的核心抽象机制，将数据和操作封装为一个类型。它与 C 的 `struct` 同源，但在 C++ 中 `class` 和 `struct` 仅有默认访问控制的区别。

```cpp
// class：默认 private
class Widget {
    int value_;          // private
public:
    int GetValue() const { return value_; }
};

// struct：默认 public（常用于 POD / 聚合）
struct Point {
    double x;            // public
    double y;            // public
};
```

### 1.1 核心概念

| 概念 | 说明 |
|---|---|
| 封装 | 数据成员 `private`，通过 `public` 成员函数暴露受控的访问 |
| 不变式 (invariant) | 构造函数建立有效状态，成员函数维护状态一致性 |
| RAII | 构造时获取资源，析构时释放——C++ 最核心的资源管理范式 |
| `this` 指针 | 每个非静态成员函数内隐式的 `T* const this`，指向调用对象 |

## 2. 内存模型

### 2.1 对象布局基础

C++ 标准只保证：
1. 成员在内存中按声明顺序排列（地址递增）
2. 第一个成员与对象的起始地址相同（无多态时）
3. 同一访问控制段（`public:` / `private:`）内，后声明的成员地址更高

编译器可以：
- 在不同访问控制段之间重排成员
- 插入填充字节（padding）满足对齐要求
- 在末尾补空使 `sizeof(T)` 是最大对齐值的整数倍

### 2.2 对齐与填充

```cpp
struct Mixed {
    char   a;    // 1 byte,   offset 0
    // 3 bytes padding (int 要求 4-byte 对齐)
    int    b;    // 4 bytes,  offset 4
    char   c;    // 1 byte,   offset 8
    // 3 bytes tail padding (使 sizeof = max(alignof) 的整数倍)
};
// sizeof(Mixed) = 12, alignof(Mixed) = 4
```

```
Mixed 内存布局:
┌────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ a  │ pad │ pad │ pad │ b₀  │ b₁  │ b₂  │ b₃  │ c  │ pad │ pad │ pad │
│ 0  │  1  │  2  │  3  │  4  │  5  │  6  │  7  │  8 │  9  │ 10  │ 11  │
└────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
箭头: a (offset 0), b (offset 4), c (offset 8)
```

**优化技巧——按对齐降序排列成员以减小填充：**

```cpp
// ❌ 12 bytes
struct Bad { char a; int b; char c; };

// ✅ 8 bytes
struct Good { int b; char a; char c; };
```

### 2.3 多态与虚函数

一旦类有了虚函数，编译器会插入一个隐藏的**虚表指针 (vptr)**：

```cpp
struct Base {
    virtual void Foo() {}
    int data;
};
// sizeof(Base) = sizeof(vptr) + sizeof(int) + padding
// 64-bit 系统：8 + 4 + 4(pad) = 16
```

```
Base 内存布局（64-bit）:
┌─────────────────┬──────────┬───────────┐
│ vptr (8 bytes)  │ data (4) │ pad (4)   │
│ offset 0        │ offset 8 │ offset 12 │
└─────────────────┴──────────┴───────────┘
```

### 2.4 空基类优化 (EBO / EBCO)

```cpp
struct Empty {};                              // sizeof = 1（C++ 要求每个对象有唯一地址）

struct Derived : Empty {
    int x;
};
// sizeof(Derived) = 4  ← 不是 8！空基类不占空间
```

编译器将空基类的存储与派生类重叠，这就是**空基类优化**。`std::unique_ptr` 默认删除器、`std::allocator` 等都依赖 EBO 实现零开销抽象。

### 2.5 栈分配 vs 堆分配

class 本身**不决定**分配位置——取决于**如何创建对象**。

```cpp
class Widget {
    int value_;
    std::string name_;
public:
    Widget(int v, std::string n) : value_(v), name_(std::move(n)) {}
};

// ① 栈分配（自动存储期）
Widget w(42, "hello");          // 对象在栈上，离开作用域自动析构

// ② 堆分配（动态存储期）
auto* pw = new Widget(42, "hi");// 对象在堆上，必须手动 delete
delete pw;                       // 否则内存泄漏

// ③ 堆分配 + 智能指针（推荐）
auto up = std::make_unique<Widget>(42, "hi");  // 堆上，自动释放
auto sp = std::make_shared<Widget>(42, "hi");  // 堆上，引用计数
```

#### 栈 vs 堆 本质区别

| | 栈 (Stack) | 堆 (Heap) |
|---|---|---|
| 管理方式 | 自动，出作用域即析构 | 手动（或由智能指针托管） |
| 大小 | ~1-8 MB（线程栈），小且快 | 进程级，大但分配/释放较慢 |
| 生命周期 | = 作用域 | 由你控制 |
| 对象位置 | 对象本身在栈上 | 栈上仅有一个指针，对象本体在堆上 |
| 分配速度 | 纳秒级（仅移动 `rsp`） | 微秒级（需查找空闲块） |
| 局部性 | 优秀（连续内存，缓存友好） | 较差（碎片化，可能 cache miss） |

#### 常见写法的分配位置

| 写法 | 对象在哪 | 需要手动释放？ |
|---|---|---|
| `Widget w;` | 栈 | 否，自动 |
| `Widget* pw = new Widget;` | 堆 | 是，`delete pw` |
| `auto up = std::make_unique<Widget>();` | 堆 | 否 |
| `std::vector<Widget> v; v.emplace_back();` | 堆（vector 内部缓冲区） | 否 |
| `std::optional<Widget> o; o.emplace();` | 跟随 optional 所在位置 | 否 |

#### 本项目全部使用栈/值语义

本项目**没有一次 `new`**，所有对象通过栈或值语义管理：

```cpp
// main.cpp — ConsoleApp 在栈上
ConsoleApp app("data/tasks.json");

// TaskRepository — vector<T> 管理元素，无需 new
std::vector<MeasurementTask> tasks_;  // vector 控制块在栈/数据成员中，
                                       // 元素缓冲区在堆上，
                                       // 但每个元素对象内嵌在该缓冲区

// MeasurementTask — optional 内嵌值
std::optional<MeasurementResult> result_;  // 值在 optional 内部，无额外堆分配
```

> **默认原则：能用栈就用栈，需要长生命周期或大对象时用堆（通过智能指针），永远不写裸 `new`/`delete`。**

## 3. 特殊成员函数（Rule of Five）

编译器可为类自动生成六个特殊成员函数：

| 函数 | 签名示例 | 何时自动生成 |
|---|---|---|
| 默认构造 | `T()` | 无任何用户声明的构造函数 |
| 析构 | `~T()` | 无用户声明析构 |
| 拷贝构造 | `T(const T&)` | 无用户声明移动操作 |
| 拷贝赋值 | `T& operator=(const T&)` | 同上 |
| 移动构造 | `T(T&&)` | 无用户声明拷贝/移动/析构 |
| 移动赋值 | `T& operator=(T&&)` | 同上 |

**Rule of Five：** 如果你自定义了五者之一（拷贝构造/拷贝赋值/移动构造/移动赋值/析构），通常需要显式定义全部五个。

**Rule of Zero：** 如果你的类只持有值语义成员或 RAII 资源句柄，则不要声明任何特殊成员函数，全用编译器默认生成。

### 3.1 默认构造函数的生成规则（易错点）

一个常见误解：以为编译器**总会**生成默认构造函数。实际情况是——**一旦你声明了任何构造函数，编译器就不再生成默认构造函数。**

```cpp
// ① 什么都没写 → 编译器生成 T() 默认构造函数
struct A {
    int x = 0;
};
A a;                            // ✅ 调用编译器生成的 A()

// ② 声明了带参构造函数 → 默认构造函数消失
struct B {
    int x;
    B(int val) : x(val) {}      // 声明了构造函数
};
B b;                            // ❌ 编译错误：没有默认构造函数！
B b2(42);                       // ✅ 只能用带参版本

// ③ 想两个都要 → 显式 = default 把默认构造要回来
struct C {
    int x;
    C(int val) : x(val) {}
    C() = default;              // 显式恢复
};
C c1;                           // ✅
C c2(42);                       // ✅
```

**完整的互相影响表：**

| 你写了什么 | 默认构造 | 拷贝构造 | 拷贝赋值 | 移动构造 | 移动赋值 | 析构 |
|---|---|---|---|---|---|---|
| 什么都不写 | ✅ 自动 | ✅ 自动 | ✅ 自动 | ✅ 自动 | ✅ 自动 | ✅ 自动 |
| `T(int)` | **❌ 没了** | ✅ 仍自动 | ✅ 仍自动 | ✅ 仍自动 | ✅ 仍自动 | ✅ 仍自动 |
| `T(const T&)` | **❌ 没了** | 你写了 | ✅ 仍自动 | **❌ 没了** | ✅ 仍自动 | ✅ 仍自动 |
| `T(T&&)` | **❌ 没了** | ❌ 删除 | ❌ 删除 | 你写了 | ❌ 删除 | ✅ 仍自动 |
| `~T()` | ✅ 仍自动* | ✅ 仍自动* | ✅ 仍自动* | **❌ 没了** | **❌ 没了** | 你写了 |

> \* C++11 起标记为 deprecated，C++20 后行为可能有变化。建议：有析构就显式处理五者。

关键规律：
- **声明任何构造函数** → 默认构造函数消失（编译器认为"你不需要默认构造"）
- **声明拷贝构造或拷贝赋值** → 移动操作消失（编译器退回到拷贝，不会生成可能错误的移动）
- **声明析构函数** → 移动操作消失（你管理了资源生命周期，编译器不敢替你生成移动语义）
- **声明移动操作** → 拷贝操作被删除（防止意外的昂贵拷贝）

**本项目的实例：**

```cpp
// MeasurementResult — 声明了带参构造，没有默认构造（故意的）
class MeasurementResult {
public:
    MeasurementResult(double value, std::string unit);
    // 没有 MeasurementResult() —— "空结果"在业务上没有意义
};
// MeasurementResult r;  ← ❌ 编译错误

// MeasurementTask — 同样声明了带参构造，没有默认构造
class MeasurementTask {
public:
    MeasurementTask(int64_t id, std::string name, ...);
    // 任务必须有 id、name 等——不允许"空任务"
};
```

两个类都遵循 **Rule of Zero**（没有自定义拷贝/移动/析构），同时通过**声明带参构造来禁止默认构造**——这是表达"必须提供参数才有意义"的惯用模式。

```cpp
// ✅ Rule of Zero：全部成员都是值语义，编译器自动生成正确实现
class MeasurementResult {
    double value_;
    std::string unit_;    // string 自己管理资源
};

// ⚠️ Rule of Five：管理裸资源
class RawBuffer {
    char* data_;
public:
    RawBuffer(size_t n) : data_(new char[n]) {}
    ~RawBuffer() { delete[] data_; }                    // 自定义析构
    RawBuffer(const RawBuffer& other);                  // → 必须定义拷贝
    RawBuffer& operator=(const RawBuffer& other);       // → 必须定义拷贝赋值
    RawBuffer(RawBuffer&& other) noexcept;              // → 必须定义移动
    RawBuffer& operator=(RawBuffer&& other) noexcept;   // → 必须定义移动赋值
};
```

## 4. 本项目中的实际应用

### 4.1 `MeasurementResult` — 不可变值对象

[`include/MeasurementResult.h`](../../include/MeasurementResult.h)：

```cpp
class MeasurementResult {
public:
    MeasurementResult(double value, std::string unit);

    double Value() const;                // const 访问器，禁止修改
    const std::string& Unit() const;     // 返回 const 引用，避免拷贝

private:
    double value_;       // 8 bytes
    std::string unit_;   // 32 bytes (MSVC std::string SSO)
};
```

**内存布局（MSVC x64）：**

```
MeasurementResult:
┌──────────────┬──────────────────────────────────┐
│ value_ (8B)  │ unit_ (32B, std::string SSO)     │
│ offset 0     │ offset 8                         │
└──────────────┴──────────────────────────────────┘
sizeof(MeasurementResult) = 40
alignof(MeasurementResult) = 8 (来自 std::string 内部的指针对齐)
```

这是一个 **Rule of Zero** 类——所有成员都自行管理生命周期，编译器默认生成的拷贝/移动/析构完全正确。

### 4.2 `MeasurementTask` — 带身份的聚合

[`include/MeasurementTask.h`](../../include/MeasurementTask.h)：

```cpp
class MeasurementTask {
public:
    MeasurementTask(
        std::int64_t id,
        std::string name,
        MeasurementStatus status,
        std::chrono::system_clock::time_point createdTime,
        std::optional<MeasurementResult> result);

    // const 访问器（无 getter 前缀）
    std::int64_t Id() const;
    const std::string& Name() const;
    // ...

    // 具名变异操作（非 setter 前缀）
    void Rename(std::string name);
    void ChangeStatus(MeasurementStatus status);
    void SetResult(MeasurementResult result);
    void ClearResult();

private:
    std::int64_t id_;                                  // 8 bytes
    std::string name_;                                 // 32 bytes (SSO)
    MeasurementStatus status_;                          // 4 bytes (enum class : int)
    // 4 bytes padding (对齐 time_point)
    std::chrono::system_clock::time_point createdTime_; // 8 bytes (通常是一个 int64_t)
    std::optional<MeasurementResult> result_;           // 48 bytes (1 + 40 + 7 padding)
};
```

**内存布局（MSVC x64）估算：**

```
MeasurementTask:
┌─────────┬──────────┬──────────┬──────────┬─────────────────┬──────────────────────────┐
│ id_     │ name_    │ status_  │ pad(4)   │ createdTime_    │ result_                  │
│ 8B      │ 32B      │ 4B       │          │ 8B              │ 48B (bool + Result +pad) │
│ off 0   │ off 8    │ off 40   │ off 44   │ off 48          │ off 56                   │
└─────────┴──────────┴──────────┴──────────┴─────────────────┴──────────────────────────┘
sizeof(MeasurementTask) ≈ 104
```

`std::optional<MeasurementResult>` 的内存模型：
```
┌──────────┬───────────┬─────────────────────────┐
│ has_value│ value_    │ unit_                   │
│ (bool,1B)│ (double,8)│ (string,32B)            │
│ offset 0 │ offset 8  │ offset 16               │
└──────────┴───────────┴─────────────────────────┘
```
`std::optional` 内部是 `bool` + 一块对齐过的内存（大小等于 `T`），而非用指针——这意味着值就内嵌在 optional 对象内，无需额外堆分配。

### 4.3 本项目遵循的 class 规范

| 规范 | 示例 |
|---|---|
| 数据成员全 `private` | `value_`, `id_`, `name_`, ... |
| 访问器 `const` 限定，无 getter 前缀 | `Value()`, `Id()`, `Name()` |
| 变异方法具名动词 | `Rename()`, `ChangeStatus()`, `SetResult()`, `ClearResult()` |
| 成员初始化列表 | `: value_(value), unit_(std::move(unit))` |
| Rule of Zero | 所有成员是值类型或 RAII 句柄，不声明拷构/移构/析构 |
| 非静态成员后缀 `_` | `value_`, `unit_`, `name_` |
| `#pragma once` | 每个头文件顶部 |

## 5. 实用技巧

### 5.1 检查对象大小

```cpp
#include <iostream>

std::cout << "sizeof(MeasurementResult): "
          << sizeof(MeasurementResult) << '\n';          // 40
std::cout << "alignof(MeasurementResult): "
          << alignof(MeasurementResult) << '\n';          // 8
std::cout << "offset of value_: "
          << offsetof(MeasurementResult, value_) << '\n'; // 0 (private，但 offsetof 可用)
```

注意：`offsetof` 只能用于**标准布局 (standard-layout)** 类型，C++ 中更推荐用 `reinterpret_cast` 方式，但仅用于学习和调试。

### 5.2 减少内存浪费

```cpp
// ❌ 成员按随意顺序声明——padding 多
struct S {
    bool flag;       // 1
    // 7 bytes pad
    double value;    // 8
    int count;       // 4
    // 4 bytes tail pad
}; // sizeof = 24

// ✅ 按 sizeof 降序排列——padding 最少
struct S {
    double value;    // 8
    int count;       // 4
    bool flag;       // 1
    // 3 bytes tail pad
}; // sizeof = 16
```

### 5.3 `final` 与 devirtualization

```cpp
class Calculator final {  // final：禁止继承
public:
    virtual int Compute() { return 42; }
};
// 编译器知道没有子类，可以将虚调用去虚拟化为直接调用
```

另一个性能相关的技巧是用 `= default` 显式要求编译器生成：

```cpp
class Widget {
public:
    Widget() = default;                      // 显式要求默认构造
    Widget(const Widget&) = default;         // 显式要求拷贝
    Widget(Widget&&) = default;              // 显式要求移动
    Widget& operator=(const Widget&) = default;
    Widget& operator=(Widget&&) = default;
    ~Widget() = default;
};
```

## 6. 与 struct 的选择

| 场景 | 推荐 | 原因 |
|---|---|---|
| 有不变量需维护 | `class` | 数据 `private`，通过成员函数保证一致性 |
| 纯数据聚合（无不变式） | `struct` | 数据 `public`，语义上就是"一捆值" |
| 值语义 + 封装 | `class` | C++ 惯用法：`class` 默认 private |
| C 互操作 | `struct` | 兼容 C 的 POD struct |
| 模板元编程辅助 | `struct` | 常见的 type traits / tag types 用 struct |

## 7. 要点

1. **`class` = 默认 `private` 的 `struct`** —— C++ 中两者几近等价，区别仅在默认访问控制。
2. **内存布局：声明顺序决定地址顺序，对齐要求决定填充** —— 按 `sizeof` 降序排列成员可减少 padding。
3. **虚函数 = vptr 开销** —— 每个对象多 8 bytes（64-bit），且虚调用有间接跳转成本。
4. **空基类优化** —— 空类作为基类不占空间，是实现零开销抽象的基石。
5. **Rule of Zero 优先** —— 如果所有成员都是值语义/RAII，编译器生成的拷贝/移动/析构就是最优解。
6. **`std::optional<T>` 内嵌值，非堆分配** —— `sizeof(optional<T>) ≈ sizeof(T) + sizeof(bool) + 对齐填充`。
7. **`offsetof` 仅适用于标准布局类型** —— 调试时可用，生产代码中应依赖类型系统而非手工偏移计算。

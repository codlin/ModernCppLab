# C++ class 三大特性（一）：封装

## 1. 是什么

封装（Encapsulation）是将数据和操作数据的方法捆绑在一起，并**隐藏内部实现细节**，仅通过公开接口与外界交互。它是面向对象设计的基石，目的不是安全，而是**降低耦合**——让调用方不依赖内部实现，你可以随时改内部而不影响外部。

C++ 通过访问控制实现封装：

| 访问级别 | 可访问者 | 用途 |
|---|---|---|
| `private` | 本类 + `friend` | 实现细节、内部状态 |
| `protected` | 本类 + 派生类 + `friend` | 为子类预留的扩展点 |
| `public` | 所有人 | 对外 API |

```cpp
class BankAccount {
public:                                    // 对外接口
    BankAccount(std::string owner, double initialBalance);
    void Deposit(double amount);           // 存钱
    bool Withdraw(double amount);          // 取钱（可能失败）
    double Balance() const;                // 查询余额
    const std::string& Owner() const;

private:                                   // 内部细节
    std::string owner_;
    double balance_;                       // 外部不能直接改！
};
```

`balance_` 是 `private`：
- 外部不能 `account.balance_ = 1000000`——编译器直接拒绝
- 只能通过 `Deposit`/`Withdraw` 修改——你可以在里面加校验、打日志、发事件
- 将来把 `double balance_` 改成 `FixedPointAmount balance_`——调用方一行不用改

## 2. 为什么封装有用：一个实例

```cpp
// ❌ 没有封装：数据暴露
struct Person {
    std::string name;
    int age;  // 外部可以设 age = -5，没有任何保护
};
Person p{"Alice", -5};   // 编译器允许，逻辑上荒谬

// ✅ 有封装：通过接口保护不变式
class Person {
public:
    Person(std::string name, int age)
        : name_(std::move(name)) {
        SetAge(age);                     // 构造也要走校验
    }

    void SetAge(int age) {
        if (age < 0 || age > 150)
            throw std::invalid_argument("Invalid age");
        age_ = age;
    }

    int Age() const { return age_; }

private:
    std::string name_;
    int age_;
};

Person p("Alice", -5);                   // ❌ 抛异常，不会产生无效对象
```

这就是 **不变式 (invariant)**——"`age_` 永远在 0~150 之间"这个约束由类自己保证，外部无需关心。

## 3. 封装的维度

封装不仅是 `private:`，至少有三个维度：

### 3.1 数据隐藏

```cpp
class Timer {
public:
    void Start();
    std::chrono::milliseconds Elapsed() const;

private:
    std::chrono::steady_clock::time_point start_;
    // 调用方不需要知道时间点用什么时钟、什么精度
};
```

### 3.2 实现隐藏（Pimpl 惯用法）

```cpp
// widget.h — 头文件干净，不暴露任何实现细节
#include <memory>

class Widget {
public:
    Widget();
    ~Widget();                          // 必须在 .cpp 中定义（unique_ptr<Impl> 析构需要完整类型）
    Widget(Widget&&) noexcept;          // 移动
    Widget& operator=(Widget&&) noexcept;

    void DoSomething();
    int GetValue() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;       // 指向实现的指针
};

// widget.cpp — 实现细节都在这里
struct Widget::Impl {
    int value;
    std::string name;
    std::vector<double> data;
    // 改这里不影响头文件，不触发大面积重编译
};

Widget::Widget() : pImpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;

void Widget::DoSomething() {
    pImpl_->data.push_back(3.14);
}
```

**收益：** 修改实现细节时，只需重编译 `.cpp`——头文件不变，所有使用者不受影响。

### 3.3 类型级封装——enum class

```cpp
// ❌ 传统 enum 泄漏到作用域
enum Color { Red, Green };              // Red / Green 污染外层
int x = Red;                             // 隐式转 int，类型无保护

// ✅ enum class 封装枚举值
enum class Color { Red, Green };
Color c = Color::Red;                   // 必须带类型名
// int x = Color::Red;                  // ❌ 编译错误
```

## 4. 本项目中封装的实际应用

### 4.1 `MeasurementResult` — 不可变值对象

[`include/MeasurementResult.h`](../../include/MeasurementResult.h)：

```cpp
class MeasurementResult {
public:
    MeasurementResult(double value, std::string unit);

    double Value() const;                // 只读访问
    const std::string& Unit() const;     // 返回 const 引用，不允许外部修改

private:
    double value_;
    std::string unit_;
};
```

封装决策：
- 构造后不可修改——`value_` 和 `unit_` 没有 setter
- 访问器 `const` 限定——编译器强制，即使在 const 上下文中也不能意外修改
- `Unit()` 返回 `const&`——零拷贝读取，但不能通过引用修改内部字符串

### 4.2 `MeasurementTask` — 带控制的变异

[`include/MeasurementTask.h`](../../include/MeasurementTask.h)：

```cpp
class MeasurementTask {
public:
    // 访问器：只读，const 限定
    std::int64_t Id() const;
    const std::string& Name() const;
    MeasurementStatus Status() const;
    std::chrono::system_clock::time_point CreatedTime() const;
    const std::optional<MeasurementResult>& Result() const;

    // 变异操作：显式命名，语义清晰
    void Rename(std::string name);
    void ChangeStatus(MeasurementStatus status);
    void SetResult(MeasurementResult result);
    void ClearResult();

private:
    std::int64_t id_;
    std::string name_;
    MeasurementStatus status_;
    std::chrono::system_clock::time_point createdTime_;
    std::optional<MeasurementResult> result_;
};
```

封装决策：
- 不是 `SetName` / `GetName`，而是 `Rename` / `Name`——动词表达意图
- `SetResult` / `ClearResult` 成对出现——而非暴露 `result_` 让外部操作
- `id_` 和 `createdTime_` 只有 getter 无 setter——外部不能修改身份

### 4.3 本项目封装规范

| 规范 | 说明 |
|---|---|
| 数据成员全部 `private` | 外部不可直接访问任何成员 |
| 访问器无 `get` 前缀 | `Value()` 而非 `GetValue()` |
| 变异器用动词 | `Rename` / `ChangeStatus` / `SetResult` / `ClearResult` |
| 访问器 `const` 限定 | 逻辑上不修改对象，编译器也保证 |
| 成员初始化列表 | 构造函数中直接初始化，而非在函数体内赋值 |
| 后缀 `_` 标记成员 | `value_` / `name_` / `status_` 一眼可辨 |

## 5. 封装的最佳实践

### 5.1 不要为"可能需要的扩展"提前做 getter/setter

```cpp
// ❌ 过度封装：每个字段都 set/get，等于 public
class Point {
    double x_, y_;
public:
    double GetX() const { return x_; }
    void SetX(double v) { x_ = v; }
    double GetY() const { return y_; }
    void SetY(double v) { y_ = v; }
};
// 不如直接用 struct Point { double x, y; };
```

封装要有**理由**——当有不变式要维护、未来可能改实现、或需要控制访问时才封装。

### 5.2 优先传值/移动 而非 const 引用（现代 C++）

```cpp
// ✅ 现代风格：参数按值接收，然后移动到成员
class Widget {
public:
    explicit Widget(std::string name)           // 按值接收
        : name_(std::move(name)) {}             // 移动到成员

    void SetName(std::string name) {            // 同上
        name_ = std::move(name);
    }

private:
    std::string name_;
};
```

调用方传左值时拷贝一次，传右值时移动一次——最优路径。比写两个重载（`const std::string&` + `std::string&&`）干净得多。

### 5.3 `explicit` 构造——防止隐式转换

```cpp
class Duration {
public:
    explicit Duration(int ms) : ms_(ms) {}      // explicit：禁止隐式转换
private:
    int ms_;
};

void Wait(Duration d) { ... }

Wait(100);                                      // ❌ 编译错误
Wait(Duration(100));                            // ✅ 显式构造
```

## 6. 要点

1. **封装不是为了安全，是为了降低耦合**——让调用方不依赖你的内部实现。
2. **数据 private，接口 public**——这是 C++ 默认首选，除非有明确理由暴露。
3. **封装的是不变式，不只是数据**——构造时建立，每个成员函数维护。
4. **访问器命名不带 `get`**——`Value()` 而非 `GetValue()`，这是 C++ 社区的广泛约定。
5. **按值传参 + `std::move`**——现代 C++ 中最简洁高效的 setter 模式。
6. **`explicit` 默认加上**——除非你确实需要隐式转换。
7. **Pimpl 是编译防火墙**——当实现经常变动或依赖很重时，把细节移到 `.cpp` 中。

# C++ class 三大特性（三）：多态

## 1. 是什么

多态（Polymorphism）允许**不同类型的对象通过统一的接口被操作**——调用方不需要知道具体类型，只需知道接口。

C++ 提供四种多态形式：

| 形式 | 绑定时机 | 机制 | 典型用途 |
|---|---|---|---|
| 虚函数多态（子类型多态） | 运行时 | 虚表 (vtable) | 面向对象设计，接口抽象 |
| 函数重载 | 编译期 | 名字修饰 (name mangling) | 相同操作，不同参数类型 |
| 模板多态 | 编译期 | 模板实例化 | 泛型编程，编译期 duck typing |
| `std::variant` + `visit` | 运行时 | union + 访问者 | 封闭的类型集合，替代继承树 |

## 2. 虚函数多态（运行时多态）

### 2.1 机制：虚表 (vtable)

每个有虚函数的类有一张**虚表**——一个函数指针数组。每个对象有一个隐藏的**虚表指针 (vptr)** 指向该表。

```cpp
class Shape {
public:
    virtual ~Shape() = default;
    virtual double Area() const = 0;         // 纯虚函数
    virtual void Draw() const { /* 默认实现 */ }
};

class Circle : public Shape {
private:
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double Area() const override { return 3.14159 * r_ * r_; }
    void Draw() const override { /* 画圆 */ }
};

class Rectangle : public Shape {
private:
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double Area() const override { return w_ * h_; }
    void Draw() const override { /* 画矩形 */ }
};
```

**运行时发生了什么：**

```
Shape* s = new Circle(5.0);
s->Area();   // ① 通过 s 找到对象的 vptr
             // ② 通过 vptr 找到 Circle 的 vtable
             // ③ 从 vtable 取出 Circle::Area 的地址
             // ④ 调用它
```

```
Circle 对象内存布局（64-bit）:
┌──────────────┬─────────┐
│ vptr (8B)    │ r_ (8B) │
│ → Circle虚表 │         │
└──────────────┴─────────┘

Circle 虚表:
┌───────────────────┬──────────────────┐
│ &Circle::~Circle  │ 析构函数         │
│ &Circle::Area     │ 覆盖 Shape::Area │
│ &Shape::Draw      │ 未覆盖，用基类   │
└───────────────────┴──────────────────┘
```

### 2.2 多态用法模板

```cpp
// ① 通过基类引用/指针操作派生类对象
void PrintArea(const Shape& s) {           // 不关心具体类型
    std::cout << s.Area() << '\n';         // 运行时根据实际类型分发
}

Circle c(5.0);
Rectangle r(3.0, 4.0);
PrintArea(c);                              // 78.54
PrintArea(r);                              // 12.0

// ② 异构容器
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(5.0));
shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));

for (const auto& s : shapes) {
    s->Draw();                             // 多态调用：每个对象调自己的 Draw
}

// ③ 工厂模式
std::unique_ptr<Shape> CreateShape(const std::string& type) {
    if (type == "circle")    return std::make_unique<Circle>(1.0);
    if (type == "rectangle") return std::make_unique<Rectangle>(2.0, 3.0);
    return nullptr;
}
```

## 3. 编译期多态

### 3.1 函数重载（静态多态）

```cpp
void Log(int value)    { std::cout << "int: " << value << '\n'; }
void Log(double value) { std::cout << "double: " << value << '\n'; }
void Log(const std::string& value) { std::cout << "string: " << value << '\n'; }

Log(42);                                     // int: 42
Log(3.14);                                   // double: 3.14
Log("hello");                                // string: hello
```

编译器根据**参数类型和数量**在编译期决定调用哪个版本。

### 3.2 模板多态（编译期 duck typing）

```cpp
// 模板：不要求继承，只要 T 有 .Area() 且返回可 < 比较即可
template<typename T>
bool IsLargerThan(const T& a, const T& b) {
    return a.Area() > b.Area();
}

// 任何满足隐式接口的类型都能用
Circle c1(5.0), c2(3.0);
Rectangle r1(10.0, 2.0), r2(3.0, 4.0);

IsLargerThan(c1, c2);                        // ✅
IsLargerThan(r1, r2);                        // ✅

// 完全无关的类型，只要满足接口就能编译通过
struct Triangle {
    double base, height;
    double Area() const { return 0.5 * base * height; }
};
IsLargerThan(Triangle{10, 5}, Triangle{6, 8}); // ✅
```

### 3.3 CRTP（奇异递归模板模式）

一种在编译期模拟虚函数的惯用法：

```cpp
template<typename Derived>
class ShapeBase {
public:
    double Area() const {
        return static_cast<const Derived*>(this)->AreaImpl();  // 编译期分发
    }
};

class Circle : public ShapeBase<Circle> {
    double r_;
public:
    Circle(double r) : r_(r) {}
    double AreaImpl() const { return 3.14159 * r_ * r_; }
};

template<typename T>
void PrintArea(const ShapeBase<T>& s) {
    std::cout << s.Area() << '\n';
}
```

**CRTP vs 虚函数：**

| | 虚函数 | CRTP |
|---|---|---|
| 分发时机 | 运行时 | 编译期 |
| 性能 | vptr 间接跳转 (~数 ns) | 内联展开，零开销 |
| 异构容器 | ✅ `vector<unique_ptr<Base>>` | ❌ 不同类型不能放同一容器 |
| 代码膨胀 | 无 | 每个类型独立实例化 |
| 灵活性 | 高（运行时可替换） | 低（类型必须编译期确定） |

## 4. `std::variant` + `std::visit`（现代替代方案）

当类型集合是**封闭的、已知的**，`std::variant` 是比虚函数继承树更高效的替代：

```cpp
#include <variant>

// 用 variant 替代继承
using Shape = std::variant<Circle, Rectangle, Triangle>;

// 通过 visit 做多态分发
double Area(const Shape& s) {
    return std::visit([](const auto& shape) {
        return shape.Area();
    }, s);
}

// 异构容器
std::vector<Shape> shapes;                   // 值语义！
shapes.push_back(Circle(5.0));
shapes.push_back(Rectangle(3.0, 4.0));
shapes.push_back(Triangle{10.0, 5.0});

for (const auto& s : shapes) {
    std::cout << Area(s) << '\n';            // 运行时多态
}
```

**`variant` vs 虚函数继承：**

| | 虚函数继承 | `std::variant` |
|---|---|---|
| 类型集合 | 开放（新派生类可随时添加） | 封闭（variant 中列出的类型） |
| 内存 | 堆分配（通常用 unique_ptr） | 栈/值语义（variant 内嵌最大的类型） |
| 添加新类型 | 容易：写新派生类 | 需修改 variant 类型列表 + 所有 visit |
| 添加新操作 | 需修改基类 + 所有派生类 | 容易：写新的 visit |
| 性能 | vptr 间接调用 | 通常直接跳转（switch-like），可内联 |

选择原则——**Expression Problem（表达式问题）**：
- 类型经常增加 → 用虚函数继承
- 操作经常增加 → 用 `std::variant`
- 不确定 → 两种各有利弊，按扩展方向选

## 5. 本项目中多态的实际与潜在应用

### 5.1 项目中已有的多态形式

**函数重载**——项目中最简单的多态：

```cpp
// MeasurementStatus.h — ToString 是针对不同枚举值的"多态"（编译期 switch 分发）
std::string ToString(MeasurementStatus status);

// 重载可扩展为其他类型：
std::string ToString(const MeasurementResult& result);
std::string ToString(const MeasurementTask& task);
```

**`std::visit` 潜在用法**——比如状态机：

```cpp
// 任务在不同状态下有不同的可用操作
using TaskState = std::variant<
    PendingState,
    RunningState,
    CompletedState,
    FailedState
>;

std::vector<std::string> AvailableActions(const TaskState& state) {
    return std::visit([](const auto& s) {
        return s.AvailableActions();
    }, state);
}
```

### 5.2 未来可能的虚函数多态场景

```cpp
// 如果将来需要不同的存储后端
class ITaskStorage {                           // 抽象接口
public:
    virtual ~ITaskStorage() = default;
    virtual std::vector<MeasurementTask> Load() = 0;
    virtual void Save(const std::vector<MeasurementTask>& tasks) = 0;
};

class JsonTaskStorage : public ITaskStorage {  // JSON 实现
    // 实现 Load / Save
};

class SqliteTaskStorage : public ITaskStorage { // SQLite 实现
    // 实现 Load / Save
};

// 调用方无需关心具体存储
void Backup(ITaskStorage& storage) {
    auto tasks = storage.Load();
    // ...
}
```

## 6. 运行时多态的性能代价

```cpp
// 基准测试思维
class Base {
public:
    virtual int Compute() const { return 42; }
};

class Derived : public Base {
public:
    int Compute() const override { return 84; }
};

// 虚调用：
//   vptr 加载 + vtable 跳转 = ~2-5 ns（预测命中时）
//   无法内联（除非 devirtualization 优化成功）
//
// 直接调用（final 或非虚）：
//   可内联，指令级并行，~0 cost
```

**结论：** 虚函数开销在绝大多数场景可忽略。只有当它出现在纳秒级热路径的循环中时才需关注——此时考虑 CRTP 或 `variant`。

## 7. 要点

1. **多态 = 统一接口操作不同类型**——C++ 提供四种：虚函数、重载、模板、`variant`。
2. **运行时多态用虚函数**——当类型需要在运行时决定、或类型集合是开放的时候。
3. **编译期多态用模板**——当所有类型在编译期已知，不需要异构容器时，零开销。
4. **`std::variant` 是封闭类型集合的首选**——值语义、无堆分配、无需继承。
5. **`override` 永远加**——编译器替你验证签名匹配。
6. **基类析构必须 `virtual`**——多态体系的基础保障。
7. **虚函数开销极小**——绝大多数场景无需担心，仅纳秒级热路径才考虑优化。

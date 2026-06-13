# C++ class 三大特性（二）：继承

## 1. 是什么

继承（Inheritance）允许一个类从另一个类**复用接口和实现**，建立"is-a"关系。派生类获得基类的成员，并可以添加、改写或隐藏它们。

```cpp
class Animal {
public:
    virtual ~Animal() = default;
    virtual std::string Speak() const { return "?"; }
    std::string Name() const { return name_; }

protected:                              // 派生类可访问
    std::string name_;
};

class Dog : public Animal {             // Dog "is-a" Animal
public:
    Dog() { name_ = "Dog"; }
    std::string Speak() const override { return "Woof!"; }
};
```

**核心语义：** `Dog` 是一个 `Animal`，凡是接受 `Animal&` 的地方都可以传 `Dog`。

## 2. 三种继承方式

| 继承方式 | 基类 public → 派生类 | 基类 protected → 派生类 | 基类 private → 派生类 |
|---|---|---|---|
| `: public` | `public` | `protected` | 不可访问 |
| `: protected` | `protected` | `protected` | 不可访问 |
| `: private` | `private` | `private` | 不可访问 |

**99% 的情况用 `: public`。** `protected`/`private` 继承是"has-a"的实现细节，C++ 中更推荐用组合（composition）替代。

```cpp
// public 继承：接口继承——"is-a"
class Dog : public Animal {};

// private 继承：实现继承——"is-implemented-in-terms-of"
// 等价于组合，仅当需要访问 protected 成员或覆盖虚函数时考虑
class MyStack : private std::vector<int> {   // 不推荐，用组合更好
public:
    void Push(int x) { push_back(x); }
};
```

## 3. 虚函数与覆盖

### 3.1 基本规则

```cpp
class Base {
public:
    virtual ~Base() = default;               // ① 基类析构必须是 virtual
    
    virtual void Draw() const { ... }        // ② 虚函数：可以被覆盖
    void Log() const { ... }                 // ③ 非虚函数：不能被覆盖（隐藏）
};

class Derived : public Base {
public:
    void Draw() const override { ... }       // ④ override：覆盖 Base::Draw
    // void Log() const override { ... }     // ❌ 编译错误！Log 不是虚函数
    
    void Log(int level) const { ... }        // ⑤ 重载：隐藏了 Base::Log()！
};
```

### 3.2 `override` 和 `final`

```cpp
class Base {
public:
    virtual void Draw() const;
    virtual void Update();
};

class Derived : public Base {
public:
    void Draw() const override;              // ✅ 明确覆盖意图，编译器检查签名
    // void Draw() override;                 // ❌ 编译错误：const 不匹配
    void Update() final;                     // ✅ 覆盖，且禁止子类再覆盖
};

class Further : public Derived {
public:
    // void Update() override;               // ❌ 编译错误：Derived::Update 标记了 final
};
```

**永远加 `override`**——它让编译器帮你检查：
- 基类到底有没有这个虚函数
- 签名是否完全匹配（const、参数类型、返回值）
- 避免你以为在覆盖，实际上是声明了新函数

### 3.3 协变返回类型

```cpp
class Base {
public:
    virtual Base* Clone() const;
};

class Derived : public Base {
public:
    Derived* Clone() const override;          // ✅ 协变返回类型
};
```

## 4. 构造函数与析构函数的调用顺序

```
构造：基类 → 成员 → 本类构造函数体
析构：本类析构函数体 → 成员 → 基类  （与构造顺序相反）
```

```cpp
#include <iostream>

struct Base {
    Base()   { std::cout << "Base()\n"; }
    ~Base()  { std::cout << "~Base()\n"; }
};

struct Member {
    Member()  { std::cout << "  Member()\n"; }
    ~Member() { std::cout << "  ~Member()\n"; }
};

class Derived : public Base {
public:
    Derived()  { std::cout << "    Derived()\n"; }
    ~Derived() { std::cout << "    ~Derived()\n"; }
private:
    Member m_;
};

// 输出：
// Base()            ← 基类先构造
//   Member()        ← 成员其次
//     Derived()     ← 本类构造体最后
//     ~Derived()    ← 析构相反
//   ~Member()
// ~Base()
```

**关键规则：** 不要在构造函数/析构函数中调用虚函数——它们调的是当前类的版本，不是最终覆盖的版本。

```cpp
class Base {
public:
    Base() { Init(); }                       // ⚠️ 危险！调的是 Base::Init
    virtual void Init() { ... }
};

class Derived : public Base {
public:
    void Init() override { ... }             // 构造时尚未被调用
};
// Base 构造 → 调用 Base::Init（不是 Derived::Init）→ 在 Derived 成员未初始化时执行
```

## 5. 多重继承与菱形问题

```cpp
class A {
public:
    int value;
};

class B : public A {};
class C : public A {};
class D : public B, public C {};
// D 中有两份 A::value！一份通过 B，一份通过 C
// D d; d.value;  ← ❌ 二义性
```

### 虚继承（virtual inheritance）

```cpp
class B : virtual public A {};               // 虚继承：B 和 C 共享同一个 A
class C : virtual public A {};
class D : public B, public C {};
// D 中只有一份 A，通过虚基类表指针（vbptr）找到
```

**虚继承有运行时代价**（多一次间接访问），只在真正需要共享基类时使用。

### 优先使用组合

```cpp
// ❌ 继承：只是为了复用代码
class UserList : public std::vector<User> {   // UserList "is-a" vector<User>?
    // 暴露了 push_back、operator[] 等全部接口
    // 调用方可以 sort、erase——可能破坏 UserList 的不变式
};

// ✅ 组合：暴露需要的接口
class UserList {
public:
    void Add(User user) { users_.push_back(std::move(user)); }
    const User* FindById(int id) const;
private:
    std::vector<User> users_;                 // UserList "has-a" vector<User>
};
```

**选择原则：** "is-a" 用继承，"has-a" 用组合。不确定时——用组合。

## 6. 纯虚函数与抽象类

```cpp
class Shape {                                  // 抽象类：不能实例化
public:
    virtual ~Shape() = default;
    virtual double Area() const = 0;           // 纯虚函数：= 0
    virtual void Draw() const = 0;
    std::string Name() const { return name_; }
protected:
    std::string name_;
};

class Circle : public Shape {
public:
    Circle(double r) : radius_(r) { name_ = "Circle"; }
    double Area() const override { return 3.14159 * radius_ * radius_; }
    void Draw() const override { /* ... */ }
private:
    double radius_;
};

// Shape s;                                    // ❌ 编译错误：抽象类不能实例化
Circle c(5.0);                                 // ✅ 具体类可以
Shape& ref = c;                                // ✅ 可以通过引用/指针使用抽象类接口
```

抽象类定义**接口契约**——所有具体子类必须实现纯虚函数。

## 7. 构造函数的继承

```cpp
class Base {
public:
    Base(int x) : x_(x) {}
    Base(int x, int y) : x_(x), y_(y) {}
private:
    int x_ = 0, y_ = 0;
};

class Derived : public Base {
public:
    using Base::Base;                          // 继承基类所有构造函数
    // 相当于自动生成：
    // Derived(int x) : Base(x) {}
    // Derived(int x, int y) : Base(x, y) {}
};
```

## 8. 常见问题与陷阱

### 8.1 忘记 virtual 析构函数

```cpp
class Base {
public:
    ~Base() {}                                 // ❌ 非 virtual！
};

class Derived : public Base {
    std::string data_;                         // 不会被正确析构
};

Base* p = new Derived();
delete p;                                      // ❌ 未定义行为！调的是 Base::~Base
// Derived 的 data_ 不会被释放 → 资源泄漏
```

**定律：有虚函数的类，析构函数必须是 `virtual`（或 `protected`）。**

### 8.2 名字隐藏（Name Hiding）

```cpp
class Base {
public:
    void Foo(int x) {}
};

class Derived : public Base {
public:
    void Foo(double x) {}                      // 隐藏了 Base::Foo(int)！
};

Derived d;
d.Foo(42);                                     // 调的是 Derived::Foo(double)，不是 Base::Foo(int)
d.Foo(3.14);                                   // OK
```

在派生类中定义了同名函数，基类所有同名重载都被隐藏。解决方法：

```cpp
class Derived : public Base {
public:
    using Base::Foo;                           // 把 Base 的 Foo 全部引入
    void Foo(double x) {}
};
```

### 8.3 切片（Slicing）

```cpp
void Process(Base b) { ... }                   // ❌ 按值传参！

Derived d;
Process(d);                                    // d 被切成 Base——Derived 的额外成员丢失
// Base b = d;  ← 只拷贝基类部分
```

**解决方案：** 按引用传参——`void Process(const Base& b)`。

## 9. 要点

1. **`public` 继承表示 "is-a"**——派生类对象就是基类对象。
2. **优先用组合，非继承**——"has-a" 比 "is-a" 更常见，也更安全（不切片、不隐藏）。
3. **基类析构函数必须是 `virtual`**——否则 `delete` 派生类对象是未定义行为。
4. **派生类覆盖函数永远加 `override`**——编译器替你检查签名。
5. **构造函数/析构函数中不要调虚函数**——它调的是当前类的版本。
6. **避免多继承的菱形问题**——用虚继承（有代价），或重构为更清晰的设计。
7. **抽象类 = 纯虚函数定义接口**——是 C++ 中最接近 Java/C# interface 的机制。
8. **按引用传参避免切片**——`void f(const Base&)` 而非 `void f(Base)`。
9. **派生类同名函数会隐藏基类所有重载**——需要时用 `using Base::Foo` 导入。

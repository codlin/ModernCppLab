# C++ enum class（强类型枚举）

## 1. 是什么

`enum class`（scoped enumeration，强类型枚举）是 C++11 引入的枚举类型，解决传统 `enum` 的三个核心问题：

| 问题 | 传统 `enum` | `enum class` |
|---|---|---|
| 作用域 | 枚举值泄漏到外层作用域 | 枚举值在 `enum class` 内部，需 `Type::Value` 访问 |
| 隐式转换 | 隐式转 `int`，可与非同类枚举值比较 | 禁止隐式转换，需 `static_cast` |
| 底层类型 | 实现定义，不可控 | 可显式指定：`enum class Foo : std::uint8_t {}` |

## 2. 语法

```cpp
// 基础形式
enum class Color {
    Red,
    Green,
    Blue
};

// 指定底层类型
enum class Status : std::uint8_t {
    Idle     = 0,
    Running  = 1,
    Stopped  = 2
};

// 使用
Color c = Color::Red;              // 必须带作用域
if (c == Color::Green) { ... }     // ✅ 同类型比较 OK
// if (c == 0) { ... }             // ❌ 编译错误：不能隐式转 int
int v = static_cast<int>(c);       // ✅ 显式转换
```

## 3. 对比示例

```cpp
// ❌ 传统 enum 的问题
enum OldStatus { Pending, Running, Completed };
enum OldColor  { Red, Green, Blue };

auto s = Pending;           // 直接访问，污染作用域
auto c = Red;
if (s == Red) { ... }       // ⚠️ 编译通过但语义错误：Status 和 Color 不该比较

// ✅ enum class 解决
enum class Status { Pending, Running, Completed };
enum class Color  { Red, Green, Blue };

auto s = Status::Pending;   // 作用域限定
auto c = Color::Red;
// if (s == Color::Red)     // ❌ 编译错误：不同类型不能比较
if (s == Status::Pending)   // ✅ 正确
```

## 4. 使用场景

### 4.1 状态机

```cpp
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

void Transition(ConnectionState from, ConnectionState to);
// 调用方无法传错类型或 magic number
Transition(ConnectionState::Disconnected, ConnectionState::Connecting); // ✅
// Transition(0, 1);                                                    // ❌ 编译错误
```

### 4.2 命令 / 操作码

```cpp
enum class Command : std::uint16_t {
    Ping    = 0x01,
    Read    = 0x02,
    Write   = 0x03,
    Reset   = 0xFF
};

void SendCommand(Command cmd);
SendCommand(Command::Reset);                              // ✅
// SendCommand(0xFF);                                     // ❌
auto raw = static_cast<std::uint16_t>(Command::Write);    // ✅ 序列化时显式取整
```

### 4.3 错误码 / 结果码

```cpp
enum class ErrorCode {
    None = 0,
    NotFound,
    PermissionDenied,
    Timeout,
    InternalError
};

std::optional<ErrorCode> ParseError(int code);
```

### 4.4 配置选项 / 策略

```cpp
enum class LogLevel { Debug, Info, Warning, Error };
enum class SortOrder { Ascending, Descending };
enum class ThreadPolicy { Single, Pooled, PerTask };
```

### 4.5 结合 `switch` 获取编译器穷举检查

```cpp
enum class Direction { North, South, East, West };

std::string ToString(Direction d) {
    switch (d) {
        case Direction::North: return "North";
        case Direction::South: return "South";
        case Direction::East:  return "East";
        case Direction::West:  return "West";
    }
    // 编译器警告：若漏掉某个枚举值，-Wswitch 会提示
    return "Unknown";
}
```

## 5. 本项目中的实际应用

[`include/MeasurementStatus.h`](../../include/MeasurementStatus.h) 定义了：

```cpp
enum class MeasurementStatus {
    Pending,
    Running,
    Completed,
    Failed
};

std::string ToString(MeasurementStatus status);
std::optional<MeasurementStatus> MeasurementStatusFromString(const std::string& value);
```

这是一个典型的状态机模式 —— `MeasurementTask` 的状态在四种值之间迁移，`enum class` 保证了：

- 状态值被限定在 `MeasurementStatus::` 作用域内，不与项目其他枚举混淆。
- `ToString` / `FromString` 提供序列化桥接（用于 JSON 持久化），通过 `switch` 显式匹配每个值，编译器会检查漏写。
- `FromString` 返回 `std::optional<MeasurementStatus>` — 这是 C++ 中处理"未知枚举值"的惯用模式。

## 6. 与辅助函数的惯用搭配

```cpp
// ToString: enum → string（日志、序列化、UI 显示）
std::string ToString(MyEnum e);

// FromString: string → optional<enum>（反序列化、用户输入解析）
std::optional<MyEnum> MyEnumFromString(const std::string& s);

// 遍历所有枚举值（C++ 没有内建反射，需要手动维护数组）
constexpr MyEnum AllValues[] = {MyEnum::A, MyEnum::B, MyEnum::C};
```

## 7. 何时用 `enum class` vs 其他方案

| 场景 | 推荐 | 原因 |
|---|---|---|
| 固定集合的离散值 | `enum class` | 类型安全 + 作用域 |
| 需要位运算的标志 | `enum class` + 手动重载 `\|` `&` | 仍比传统 `enum` 安全 |
| 运行时可扩展的值集 | `std::string` / `int` + 常量 | 枚举值必须在编译期确定 |
| C 互操作 | 传统 `enum` | ABI 兼容性 |

## 8. 要点

1. **默认首选 `enum class`** —— 除非有明确的兼容性理由，C++11 及以后项目不再使用传统 `enum`。
2. 显式指定底层类型当需要控制内存布局或序列化格式时（`enum class X : std::uint8_t`）。
3. 配合 `switch` 使用以获取编译器对遗漏分支的警告。
4. 总是写 `ToString` / `FromString` 辅助函数——C++ 没有内建枚举反射。

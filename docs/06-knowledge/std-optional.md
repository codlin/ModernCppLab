# C++ std::optional（可选值）

## 1. 是什么

`std::optional<T>` 是 C++17 引入的模板类，用于表达"可能有值，也可能没有"的语义。它包装一个 `T` 类型的值，并在运行时跟踪该值是否存在。

| 问题 | 传统方案 | `std::optional<T>` |
|---|---|---|
| "无值"的表达 | 哨兵值（`-1`、`nullptr`、`""`），语义不清 | `std::nullopt` 明确表达"无值" |
| 类型安全 | `bool` + `T` 分离，可能不同步 | 值和存在性打包在一起，类型系统保证一致性 |
| 意图表达 | 需要文档/注释说明"何时有效" | 类型本身即文档：`std::optional<T>` 一眼可知可能为空 |
| 内存开销 | 需额外 `bool` 变量或哨兵值判断 | `sizeof(T) + sizeof(bool)` + 对齐，通常与哨兵方案相当 |

## 2. 语法

```cpp
#include <optional>

// 构造
std::optional<int> o1;                          // 空，无值
std::optional<int> o2 = std::nullopt;           // 显式空
std::optional<int> o3 = 42;                     // 有值 42
std::optional<int> o4 = std::make_optional(42); // 同上有值
auto o5 = std::make_optional<std::string>("hi");// 就地构造

// 检查是否有值
if (o3.has_value()) { ... }                     // bool 检查
if (o3) { ... }                                 // 隐式转 bool，等价于 has_value()

// 取值（必须先检查）
int v1 = o3.value();                            // 有值时返回值，无值时抛 std::bad_optional_access
int v2 = *o3;                                   // 同 value()，无值时 UB
int v3 = o3.value_or(-1);                       // 有值时返回，无值时返回默认值 -1

// 修改
o1 = 100;                                       // 设置为新值
o1 = std::nullopt;                              // 重置为空
o1.reset();                                     // 同上，重置为空
o1.emplace(42);                                 // 就地构造新值（避免临时对象）

// C++23 单子操作
auto r = o3.and_then([](int x) {                // 有值时调用函数，返回 optional
    return std::optional<int>(x * 2);
});
auto r = o3.transform([](int x) {               // 有值时转换，自动包裹为 optional
    return std::to_string(x);
});
auto r = o3.or_else([] {                        // 无值时调用，返回替代 optional
    return std::make_optional<int>(-1);
});
```

## 3. 对比示例

### 3.1 查找操作

```cpp
// ❌ 传统方案：哨兵值
int FindIndex(const std::vector<int>& v, int target) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return static_cast<int>(i);
    return -1;  // "未找到"用 -1 表达，但 -1 本身也是合法 size_t 转换值吗？语义模糊
}

auto idx = FindIndex(v, 42);
if (idx != -1) { /* 使用 idx */ }  // 调用方必须知道 -1 是"未找到"

// ✅ std::optional 方案
std::optional<size_t> FindIndex(const std::vector<int>& v, int target) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return i;
    return std::nullopt;  // 语义明确：没找到
}

auto idx = FindIndex(v, 42);
if (idx) { /* 使用 *idx */ }      // 类型系统保证你检查过
size_t safe = idx.value_or(0);    // 或直接给默认值
```

### 3.2 解析/反序列化

```cpp
// ❌ 传统方案：bool + 值分离
struct ParseResult {
    bool success;
    int value;  // success==false 时 value 无意义
};

ParseResult ParseInt(const std::string& s);
auto r = ParseInt("abc");
if (r.success) { /* 用 r.value */ }  // 可以不检查直接用 r.value，编译期无保护

// ✅ std::optional 方案
std::optional<int> ParseInt(const std::string& s);

auto r = ParseInt("abc");
if (r) { /* 用 *r */ }              // 必须先检查才能安全取值
int v = ParseInt("abc").value_or(0); // 或链式给默认值
```

### 3.3 延迟初始化

```cpp
// ❌ 传统方案：指针 + new / nullptr
class Widget {
    std::unique_ptr<ExpensiveData> data_;  // 堆分配，缓存不友好
public:
    void LazyInit() {
        if (!data_) data_ = std::make_unique<ExpensiveData>();
    }
};

// ✅ std::optional 方案
class Widget {
    std::optional<ExpensiveData> data_;     // 栈上/对象内分配，缓存友好
public:
    void LazyInit() {
        if (!data_) data_.emplace(/* args */);  // 就地构造，无堆分配
    }
};
```

## 4. 使用场景

### 4.1 返回值——"可能没有结果"

这是 `std::optional` 最主要的使用场景，替代哨兵值和 `bool` + 值分离的方案：

```cpp
// 查找
std::optional<User> FindUserById(int64_t id);

// 解析
std::optional<double> ParseDouble(const std::string& s);

// 取值
std::optional<std::string> GetEnv(const std::string& name);

// 从容器安全取值
template<typename T>
std::optional<T> At(const std::vector<T>& v, size_t index) {
    if (index < v.size()) return v[index];
    return std::nullopt;
}
```

### 4.2 函数参数——可选参数

```cpp
// 有默认行为的可选参数
void Log(const std::string& msg,
         std::optional<LogLevel> level = std::nullopt,
         std::optional<std::string> tag = std::nullopt);

Log("hello");                                   // 使用默认 level 和 tag
Log("error", LogLevel::Error);                   // 指定 level
Log("request", std::nullopt, "http");            // 仅指定 tag
```

### 4.3 数据成员——"可能暂缺"的字段

```cpp
struct UserProfile {
    std::string name;
    std::optional<std::string> nickname;    // 用户可以不填昵称
    std::optional<std::string> avatar_url;  // 用户可以不设头像
    std::optional<int> age;                 // 用户可以不透露年龄
};
```

### 4.4 延迟初始化（lazy initialization）

```cpp
class TextureCache {
    std::optional<Texture> cached_;
public:
    const Texture& Get() {
        if (!cached_) cached_.emplace(LoadFromDisk());  // 首次访问时才初始化
        return *cached_;
    }
};
```

### 4.5 与错误处理的关系

| 场景 | 推荐 | 原因 |
|---|---|---|
| "没找到" / "不存在" | `std::optional<T>` | 这不是错误，是预期内的正常结果 |
| 操作失败需要原因 | `std::expected<T, E>` (C++23) 或异常 | 需要携带错误信息 |
| 多种失败模式 | 异常 / `status_code` | optional 只能表达"有/无" |
| 异步回调 | callback(const T&) / callback(std::nullopt) | 避免空指针 |

## 5. 本项目中的实际应用

### 5.1 `MeasurementStatusFromString` — 解析可能失败

[`include/MeasurementStatus.h:14`](../../include/MeasurementStatus.h#L14)：

```cpp
std::optional<MeasurementStatus> MeasurementStatusFromString(const std::string& value);
```

将字符串反序列化为枚举值——如果字符串不匹配任何已知状态，返回 `std::nullopt`。这是 C++ 中处理"未知枚举值"的惯用模式：调用方必须检查返回值是否有效。

### 5.2 `MeasurementTask::Result()` — 可选的测量结果

[`include/MeasurementTask.h:18`](../../include/MeasurementTask.h#L18)：

```cpp
const std::optional<MeasurementResult>& Result() const;
```

一个任务可能尚未完成（没有结果），也可能已完成（有 `MeasurementResult`）。用 `std::optional` 天然地表达了这种"可能有/可能没有"的语义，无需引入 `bool has_result` + `MeasurementResult result` 的分离字段。

### 5.3 `RegexFirstGroup` — 内部工具函数

[`src/JsonTaskStorage.cpp:37`](../../src/JsonTaskStorage.cpp#L37)：

```cpp
std::optional<std::string> RegexFirstGroup(const std::string& text, const std::regex& pattern);
```

用正则提取 JSON 字段——匹配成功返回捕获的字符串，失败返回 `std::nullopt`。这是 `optional` 作为"可能失败的操作"返回值的经典用例。

### 5.4 本项目中的使用模式总结

```
┌──────────────────────────────────────────────────────┐
│  FromString / Parse / Find → std::optional<T>        │
│  "我尝试了，但不保证成功"                              │
├──────────────────────────────────────────────────────┤
│  数据成员可能暂缺 → std::optional<MemberType>         │
│  "这个字段是可选的，业务上允许为空"                     │
├──────────────────────────────────────────────────────┤
│  调用方                                 →             │
│  if (opt) { use(*opt); } 或 opt.value_or(default)    │
│  必须处理"无值"分支，类型系统强制执行                  │
└──────────────────────────────────────────────────────┘
```

## 6. 注意事项

### 6.1 不要用 `std::optional<T&>`

```cpp
// ❌ 不合法：std::optional<T&> 在标准中不要求支持
std::optional<int&> opt_ref = x;

// ✅ 替代方案：用指针（语义相同，"有值"即非空）
int* ptr = &x;
if (ptr) { use(*ptr); }
```

### 6.2 `value()` vs `operator*`

```cpp
std::optional<int> opt;

int a = opt.value();  // 抛 std::bad_optional_access（定义行为）
int b = *opt;         // 未定义行为！（与 value() 不同）
```

**建议：** 生产代码永远用 `value()` 或先检查 `.has_value()`。`operator*` 仅在已经检查过时使用。

### 6.3 不要过度使用

```cpp
// ❌ 不必要的 optional
std::optional<std::string> GetName() {
    return "default";  // 永远有值
}

// ✅ 当函数正常结果就是"可能有/没有"时才用
std::optional<std::string> GetNickname() {
    if (user.has_nickname) return user.nickname;
    return std::nullopt;
}
```

### 6.4 与 `std::unique_ptr<T>` 的选择

| 考量 | `std::optional<T>` | `std::unique_ptr<T>` |
|---|---|---|
| 内存位置 | 栈上/对象内（值语义，缓存友好） | 堆上（指针语义） |
| 所有权 | 值所有权，复制时拷贝整个 `T` | 独占所有权，仅可移动 |
| 可为空 | ✅ | ✅ |
| 适用 | `T` 有确定大小，期望值语义 | `T` 很大/多态/不可拷贝 |

```cpp
// ✅ optional：值语义，小对象
std::optional<Point> FindNearest(const Point& target);

// ✅ unique_ptr：多态，大对象
std::unique_ptr<EventHandler> CreateHandler(EventType type);
```

## 7. 要点

1. **用 `std::optional<T>` 替代哨兵值** —— 类型系统帮你确保调用方处理了"无值"的情况。
2. **用 `value_or(default)` 提供回退值** —— 当有合理的默认值时，这是最简洁的写法。
3. **用 `.emplace(args...)` 就地构造** —— 避免创建临时对象再拷贝/移动。
4. **永远检查再取值** —— `.has_value()` 或隐式 `bool` 转换，或使用 `.value_or()` / `.and_then()` 等单子操作。
5. **`std::optional<T&>` 不合法** —— 用裸指针替代。
6. **C++23 引入单子操作** `.and_then()` `.transform()` `.or_else()` —— 链式处理 optional，避免嵌套 `if`。

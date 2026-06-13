# C++ `std::string` 深入介绍

## 1. 是什么

`std::string` 是 C++ 标准库提供的字符串类型（`#include <string>`），本质是 `std::basic_string<char>` 的类型别名。它是**可变**、**拥有其内存**、**以 null 结尾**（C++11 起）的动态字符数组。

```cpp
#include <string>

std::string s1;                      // 空字符串
std::string s2 = "hello";            // 从 C 字符串构造
std::string s3(10, 'x');             // "xxxxxxxxxx"
std::string s4{s2};                  // 拷贝
std::string s5 = s2 + " world";      // 拼接
```

## 2. 内部机制

### 2.1 Small String Optimization (SSO)

`std::string` 的经典实现使用 SSO：在对象内部预留一个固定大小的小缓冲区（典型为 15–22 字符），短字符串直接存储在栈上，不分配堆内存。

```cpp
// MSVC 典型布局（简化模型）
class string {
    union {
        char small_buf[16];   // 短字符串直接存在这里，size ≤ 15
        char* heap_ptr;       // 长字符串指向堆内存
    };
    size_t size;              // 字符串长度（不含 '\0'）
    size_t capacity;          // 短模式 = 15（缓冲区大小），长模式 = 堆分配大小

    // 判断是否在 SSO 模式
    bool is_small() const { return capacity <= 15; }
};
```

| 短字符串（size ≤ 15） | 长字符串（size > 15） |
|---|---|
| 字符在对象内部的 `char[16]` | 字符在堆上，对象存指针 |
| 无堆分配，无额外开销 | 一次堆分配 |
| 拷贝=拷贝整个内部数组（快） | 拷贝=新堆分配+memcpy |
| `data()` 返回内部缓冲区地址 | `data()` 返回堆地址 |
| 移动=拷贝，无优化 | 移动=偷指针，O(1) |

### 2.2 容量增长策略

当 `append` / `push_back` / `+=` 导致 `size > capacity` 时：
- MSVC：1.5× 增长
- GCC libstdc++：2× 增长
- Clang libc++：2× 增长

```cpp
std::string s;
s.reserve(1000);          // 预分配，避免反复重分配
// 如果知道目标大小，先 reserve 再 += 比直接 += 高效
```

### 2.3 key 方法速览

| 方法 | 说明 |
|---|---|
| `size()` / `length()` | 返回字符数（不含 null），O(1) |
| `capacity()` | 已分配空间可容纳的字符数（不含 null） |
| `c_str()` | 返回 `const char*`（保证 null 结尾），用于 C API |
| `data()` | C++17: 返回 `char*`（可变，null 结尾）；C++11/14: 返回 `const char*`（可能不 null 结尾） |
| `empty()` | `size() == 0` |
| `clear()` | 清空内容，不释放已分配内存 |
| `reserve(n)` | 保证 capacity ≥ n，不改变 size |
| `shrink_to_fit()` | 请求释放多余内存（是否释放由实现决定） |
| `substr(pos, count)` | 返回子串（会分配新内存并拷贝） |
| `find(str)` | 查找子串位置，未找到返回 `std::string::npos` |
| `starts_with(str)` | C++20 引入的前缀检查 |

### 2.4 C++17 `std::string_view`

```cpp
#include <string_view>

// 只读视图，不拥有数据，不分配内存，适合解析场景
std::string_view sv = "hello world";
sv = sv.substr(0, 5);       // "hello"，无需分配，只移动指针
// sv[0] = 'H';             // ❌ 编译错误：只读

// 从 string 隐式构造
std::string s = "abc";
std::string_view sv2 = s;   // 指向 s 的内部 buffer
s += "def";
// sv2 现在悬空！⚠️ 原始 string 重分配后 view 失效
```

## 3. 使用说明

### 3.1 构造与赋值

```cpp
std::string s("hello");          // 推荐
std::string s = "hello";         // 等价，隐式转换
// std::string s = nullptr;      // ❌ 运行时 crash：从 nullptr 构造未定义行为

s = "world";                     // 赋值，复用已分配内存
s.assign("world");               // 等价
```

### 3.2 拼接

```cpp
std::string s = "a";
s += "b";                           // 推荐：不会创建临时对象
s = s + "c";                        // 创建临时 string 然后赋值，效率较低
s.append("d").append("e");          // 链式拼接

// 多段拼接
s += std::to_string(42);            // 数字转字符串
```

### 3.3 参数传递

```cpp
// 传入只读：用 const std::string& 或 std::string_view（C++17）
void read(const std::string& s);
void read(std::string_view sv);     // 更灵活：可接受 string / const char* 而不构造临时对象

// 需要拷贝副本：用按值传参 + std::move
void store(std::string s) {         // 调用方拷贝/移动传入
    data_ = std::move(s);           // 不额外拷贝
}

// 严格只读 + 不需 null 结尾 + 长度已知：用 std::string_view
```

### 3.4 与 C API 交互

```cpp
std::string s = "hello";

// → C 字符串
const char* p = s.c_str();
fopen(p, "r");

// C 字符串 →
std::string from_c(p);
std::string from_c(p, len);         // 指定长度，p 可以不 null 结尾
```

### 3.5 遍历

```cpp
std::string s = "abc";

// 按字节遍历（ASCII）
for (char c : s) { /* c 是字节 */ }

// 按 UTF-8 遍历 —— std::string 不原生支持，需要第三方库或手动处理
// 注意：std::string 的 s[i] 返回的是字节，不是 Unicode 码位
```

## 4. 与 C 语言的差异

| C `char*` / `char[]` | C++ `std::string` |
|---|---|
| 手动管理内存 (`malloc`/`free`) | RAII：自动管理，离开作用域自动释放 |
| `strlen(p)` 扫描到 `\0`，O(n) | `s.size()` 直接读取成员，O(1) |
| `strcpy`/`strcat` 需手动保证缓冲区够大 | `s1 = s2` / `s += t` 自动扩容 |
| 无容量概念，每次增长都可能重分配 | 有 capacity，可 `reserve` 预分配 |
| 支持 `\0` 内部字符（二进制安全） | 支持 `\0` 内部字符（`std::string` 以 size 而非 null 判断结尾） |
| `strcmp(s, t) == 0` 比较相等 | `s == t` 直接比较 |
| `strstr(p, needle)` 查找子串 | `s.find(needle)` |
| 无标准库支持 Unicode | 同样无标准库支持（字节数组，编码无关） |

### 互操作模板

```cpp
// C API → C++
std::string s(GetCStr(), GetLength());

// C++ → C API
FILE* f = fopen(s.c_str(), "r");
SomeCAPIFunc(s.data(), s.size());
```

## 5. 与 C# 语言的差异

| C# `string` / `System.String` | C++ `std::string` |
|---|---|
| **不可变 (immutable)** — 每次修改都创建新对象 | **可变 (mutable)** — 原地修改 |
| `s = s + "x"` 创建新 string，旧 string 等待 GC | `s += "x"` 在 buffer 上追加，不分配（若 capacity 够） |
| `StringBuilder` 用于高效拼接 | `std::string` 本身就是 "StringBuilder" |
| 引用类型（堆上，`null` 是合法值） | 值类型（栈上 + 堆 buffer），无 `null` 状态 |
| `s.Length` 属性 | `s.size()` / `s.length()` 方法 |
| 默认 Unicode（UTF-16，`char` 是 16 位） | 字节序列，编码无关，`char` 是 8 位 |
| `string.IsNullOrEmpty(s)` 静态方法 | `s.empty()` 成员方法 |
| `s.IndexOf(sub)` — StringComparison 枚举控制 | `s.find(sub)` — 字节级搜索 |
| `$"Hello {name}"` 字符串插值 | C++20: `std::format("Hello {}", name)` (`#include <format>`) |
| `s.Split(',')` 返回 `string[]` | 需手动实现或第三方库，C++20 `std::views::split` |
| `Substring` 创建新 string（分配） | `substr` 创建新 string（分配）；用 `string_view` 避免分配 |

### 典型模式对比

```cpp
// C#: string 不可变，大量拼接必须用 StringBuilder
var sb = new StringBuilder();
for (int i = 0; i < 1000; i++) sb.Append(i);
string result = sb.ToString();

// C++: std::string 本身就是 mutable 的
std::string s;
s.reserve(4000);                     // 可选：一次预分配
for (int i = 0; i < 1000; i++) s += std::to_string(i);
// s 就是最终结果，不需要额外类型

// C++20: 也可以用 std::format
std::string s2 = std::format("Hello {}", name);
```

### 无 null 状态

```csharp
// C#: null 是合法状态
string? nullable;                        // null 合法

// C++: std::string 不能用 nullptr 赋值
std::string s;                           // 空字符串，不是 null
s.empty();                               // true
// std::optional<std::string> 用于表达"可能没有"
std::optional<std::string> maybe = std::nullopt;
```

## 6. 常见陷阱

### 6.1 返回 `c_str()` 的指针给外部持有

```cpp
// ❌ 危险
const char* GetName() {
    std::string name = BuildName();
    return name.c_str();    // name 析构后指针悬空！
}

// ✅ 正确：返回 string 本身，让调用方按需 .c_str()
std::string GetName() {
    return BuildName();
}

// ✅ 如果必须返回 const char*，确保生命周期
const std::string& GetGlobalName() {
    static std::string name = "hello";
    return name;            // 调用方可安全 .c_str()
}
```

### 6.2 `string_view` 悬空

```cpp
// ❌ 危险
std::string_view sv = std::string("temp") + "string";
// 临时 string 已析构，sv 悬空

// ✅ 正确：string_view 只用于传参或从活着的 std::string 获取
void PrintView(std::string_view sv) { /* 函数体内使用 */ }
PrintView(s); // s 在调用方存活，安全
```

### 6.3 效率误区 1：不必要的拷贝

```cpp
// ❌ substr 总是分配新内存
bool startsWith(const std::string& s, const std::string& prefix) {
    return s.substr(0, prefix.size()) == prefix;   // 分配临时 string
}

// ✅ 用 compare（不分配）
bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

// ✅ C++20
bool startsWith(const std::string& s, const std::string& prefix) {
    return s.starts_with(prefix);
}
```

### 6.4 效率误区 2：忽略 SSO

```cpp
// 短字符串拷贝很快（栈上 16 字节被 memcpy），不必过度优化用移动
std::string s = "ok";            // SSO，无堆分配
std::string s2 = std::move(s);   // SSO 下移动 = 拷贝，没有额外收益
```

### 6.5 多字节编码

```cpp
std::string s = "你好";
s.size();           // 6（UTF-8 每个中文字符 3 字节），不是 2
s[0];               // 第一个字节，不是第一个字符
// std::string 是字节序列，不感知 Unicode
```

### 6.6 空字符串 vs `nullptr`

```cpp
std::string s(nullptr);    // ❌ 未定义行为 — 运行时通常 crash
std::string s;             // ✅ 空字符串
std::string s = "";        // ✅ 等价
```

## 7. 在本项目中的使用

本项目中 `std::string` 被广泛使用于：

- **数据成员** — `MeasurementTask::name_`、`MeasurementResult::unit_`
- **序列化** — `JsonTaskStorage::Save/Load` 通过 `std::ostringstream` 构建 JSON 文本
- **用户输入** — `ConsoleApp::ReadLine` 通过 `std::getline(std::cin, ...)` 返回 `std::string`
- **枚举转换** — `ToString(MeasurementStatus)` 返回 `std::string`

其中 `const std::string&` 被用于 getter 返回值（零拷贝访问），`std::move` 用于构造时转移所有权。

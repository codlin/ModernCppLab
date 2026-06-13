# 堆与栈：从硬件到 C++

## 1. 直观类比

想象你在做饭：

| | 栈 (Stack) | 堆 (Heap) |
|---|---|---|
| 类比 | **案板**——放正在处理的食材，用完了立刻清走，空间有限但取放极快 | **冰箱/储藏室**——想存多久存多久，空间很大但取放慢，需要记"什么东西放在哪" |
| 谁来管 | 你自己（编译器），按菜谱步骤自动收放 | 你自己（程序员），需要时去拿，用完放回去 |
| 忘了会怎样 | 不会忘——离开台面自动归位 | 忘了收回 → 冰箱越来越满（内存泄漏） |

## 2. 操作系统视角：一个进程的内存地图

```
高地址
┌──────────────────────┐
│  命令行参数 + 环境变量  │
├──────────────────────┤
│       栈 (Stack)      │  ← 向下增长（高→低地址）
│       │               │     局部变量、返回地址、函数参数
│       ↓               │
│   ⋯  空闲区域  ⋯      │
│       ↑               │
│       │               │
│       堆 (Heap)       │  ← 向上增长（低→高地址）
├──────────────────────┤
│    BSS 段（未初始化全局）│
├──────────────────────┤
│    Data 段（已初始化全局）│
├──────────────────────┤
│    Text 段（机器码）    │
└──────────────────────┘
低地址
```

这并不是某种抽象概念——每个运行中的进程真的在内存里长这样。`/proc/<pid>/maps`（Linux）可以看到：

```
00400000-00401000 r-xp ... /usr/bin/myapp    ← Text
00600000-00601000 r--p ... /usr/bin/myapp    ← Data
00601000-00602000 rw-p ... /usr/bin/myapp    ← BSS
7fff12340000-7fff12360000 rw-p ... [stack]  ← 栈（几 MB）
7fff12400000-7fff12800000 rw-p ... [heap]   ← 堆（动态增长）
```

## 3. 栈的工作原理

### 3.1 硬件层面

栈是 CPU 直接支持的：有一个专用寄存器 **RSP**（x86-64，Stack Pointer）指向栈顶。

```asm
; x86-64 汇编——函数调用时栈的变化
push rbp           ; ① 保存调用者的栈帧基址（RSP 减 8，写入内存）
mov rbp, rsp       ; ② 建立新栈帧
sub rsp, 32        ; ③ 为局部变量预留 32 字节（RSP 减 32）

; ... 函数体使用 [rbp-8], [rbp-16] 等局部变量 ...

mov rsp, rbp       ; ④ 恢复栈指针
pop rbp            ; ⑤ 恢复调用者的栈帧基址
ret                ; ⑥ 弹出返回地址，跳回调用者
```

整个过程只是加减一个寄存器（RSP）——这就是为什么栈分配是**纳秒级**的。

### 3.2 栈帧（Stack Frame）

每次函数调用都会在栈上压入一个"栈帧"：

```
调用者栈帧:
┌──────────────────────┐
│  局部变量             │
│  ...                 │
├──────────────────────┤
│  传给被调函数的参数    │  ← 部分参数可能通过寄存器传递
├──────────────────────┤
│  返回地址             │  ← call 指令自动压入
├──────────────────────┤
被调函数栈帧:
│  保存的 rbp           │  ← push rbp
├──────────────────────┤
│  局部变量             │  ← sub rsp, N
│  ...                 │
└──────────────────────┘  ← RSP 当前指向这里
```

### 3.3 递归与栈溢出

```cpp
int Factorial(int n) {
    if (n <= 1) return 1;
    return n * Factorial(n - 1);     // 每次递归：一个新栈帧（几十字节）
}

Factorial(1000000);                   // ☠️ 栈溢出！每个线程栈 ~1-8 MB
```

每次递归调用都在栈上分配新栈帧，递归太深 → 栈撞上堆或保护区 → **stack overflow**（这就是那个网站名字的由来）。

### 3.4 为什么栈这么快——六个根因

#### 根因一：内存已提前分配

栈空间在**线程创建时**就由操作系统一次性分配好（通常 1-8 MB），后续的"分配"只是移动栈指针——不涉及任何系统调用或内存申请。

```
线程创建时:
mmap(NULL, 8MB, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0)
 → OS 预留 8MB 虚拟地址空间，初始只有少量页面被物理映射

使用时:
sub rsp, 4096     ← 可能触发一次 page fault（首次访问该页）
                   OS 透明映射物理页，程序无感知
```

对比堆：每次 `malloc` 都可能触发 `brk()`/`mmap()` 系统调用——从用户态切换到内核态，代价巨大。

#### 根因二：分配 = 一条 CPU 指令

```asm
; 为 3 个局部变量分配空间（一个 int + 一个 double + 一个指针 = 20 bytes，对齐到 32）
sub rsp, 32        ; 1 条指令，1 个时钟周期，CPU 可以和其他指令并行执行

; 对比：malloc(32) 的指令路径
; → call malloc@plt （跳转）
; → 检查 tcache （多次内存访问）
; → tcache miss → 检查 fastbins （更多内存访问）
; → fastbins miss → 检查 smallbins
; → 可能 arena 锁
; → 可能 brk() 系统调用
; → 数百到数千条指令
```

CPU 的乱序执行引擎可以把 `sub rsp, 32` 和周围的指令并行调度，几乎完全隐藏其延迟。

#### 根因三：LIFO 结构天然零碎片

```
栈操作:
push A  ─┐
push B  ─┤  A 和 B 紧挨着
push C  ─┘
pop  C  ← 后进先出：C 出栈后，空间立刻归还 B
pop  B  ← 空间连续，永远不会出现"洞"
pop  A

堆操作:
malloc(A) → 地址 0x1000
malloc(B) → 地址 0x1100
free(A)   → 0x1000-0x10FF 空闲
malloc(C) → 如果 C 比 A 小，可能用 0x1000
              如果 C 比 A 大，0x1000 暂时无法用，需要从别处分配
              → 碎片逐渐累积
```

栈的 LIFO 属性意味着**释放顺序 = 分配顺序的反向**，永远不会有"中间先释放"留下的空洞。这是栈为什么不需要复杂内存管理器的根本原因。

#### 根因四：无元数据开销

```
栈上的"分配":
  sub rsp, 32
  [rbp-32] ← int a       # 直接使用，无需记录大小
  [rbp-28] ← double b    # 编译器在编译时就知道每个变量的偏移
  [rbp-20] ← char* c

堆上的分配:
  ┌──────────┬──────────────────────┬──────────┐
  │ size(8B) │ 用户数据 (32B)        │ padding  │
  │ ← prev   │                      │          │
  │   chunk  │                      │          │
  │   size   │                      │          │
  └──────────┴──────────────────────┴──────────┘
  每次 malloc 都要额外分配 8-16 字节的 chunk header
  free() 时依赖 header 中的 size 来知道释放多少
```

编译器在编译期就知道每个局部变量的确切偏移和大小，不需要运行时元数据。

#### 根因五：天然线程安全，无需锁

```
每个线程有独立的栈                  堆是所有线程共享的

Thread-1 栈:                       Thread-1 ─┐
┌──────────────┐                            │
│  帧1 帧2 ... │   各自独立，                ├─→ 堆（需要锁/原子操作）
└──────────────┘   互不干扰                  │
                              Thread-2 ─┘
Thread-2 栈:
┌──────────────┐
│  帧1 帧2 ... │
└──────────────┘
```

`malloc` 在多线程程序中需要锁或原子操作来保护内部数据结构（ptmalloc 用 arena 降低竞争，tcmalloc/jemalloc 用 per-thread cache 几乎消除竞争——但仍有开销）。

#### 根因六：缓存永远是最热的

```
栈：连续分配 → 最新帧在 L1 缓存中

  L1 Cache（32KB, 4 cycle 延迟）
  ┌──────────────────────────────────┐
  │ [旧帧]* [当前帧的局部变量全部在这] │ ← 每次函数调用都在操作栈顶
  └──────────────────────────────────┘
  CPU 预取器：看到连续的栈访问模式，提前加载

堆：散列分配 → 每次访问可能到不同缓存行

  L1 Cache
  ┌──────────────────────────────────┐
  │ [objA 碎片] ... [objC 碎片] ...   │ ← 每个对象来自不同的 malloc
  └──────────────────────────────────┘
  访问 objA → L1 miss → L2 miss → L3 miss → 主存（~100 cycle）
```

## 4. 堆的工作原理——深入 malloc 内部

### 4.1 不只是"空闲链表"——真实分配器的结构

文档和教材常用"空闲链表"解释 malloc，但生产级分配器远比这复杂。以 Linux 的 **ptmalloc (glibc malloc)** 为例：

```
ptmalloc 内部分层结构:

┌─────────────────────────────────────────────┐
│                 Thread Cache (tcache)        │  ← 每线程，无锁，极快
│  tcache_bins[64]: 每个 bin 存 7 个同大小块    │
│  大小: 32B ~ 1040B (按 16B 递增)              │
└─────────────────────────────────────────────┘
              ↓ tcache 满了或大小不匹配
┌─────────────────────────────────────────────┐
│                    Arena                      │  ← 每个 arena 一把锁
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐ │
│  │ fastbins │ │ smallbins│ │  largebins   │ │
│  │ 单链表   │ │ 双链表   │ │  双链表+排序  │ │
│  │ 16-88B  │ │ 32-1024B │ │  >1024B      │ │
│  │ LIFO    │ │ FIFO     │ │  best-fit    │ │
│  │ 不合并  │ │ 精确匹配  │ │  大小相近     │ │
│  └──────────┘ └──────────┘ └──────────────┘ │
│  ┌──────────────────────────────────────┐    │
│  │         unsorted bin                 │    │
│  │  free() 后先放这里，下次 malloc 时再分类│    │
│  └──────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
              ↓ 所有 arena 都没合适的内存
┌─────────────────────────────────────────────┐
│             系统调用                          │
│  brk() : 小扩展（移动 program break）         │
│  mmap(): 大块申请（≥128KB 通常）              │
└─────────────────────────────────────────────┘
```

### 4.2 一次 `malloc(32)` 的实际路径

```
malloc(32) → 实际分配 48B（32 + 16B chunk header）

① 检查 tcache[48B bin]
   - 命中 → 从链表头取一个，返回 → 总计 ~10 条指令 ✅
   - 未命中 → 进入 ②

② 检查 fastbins（arena 内，需锁）
   - 命中 → 从链表头取一个，解锁，返回 → ~50 条指令
   - 未命中 → 进入 ③

③ 检查 smallbins（精确 48B 的 bin）
   - 命中 → 从链表取一个，解锁，返回
   - 未命中 → 进入 ④

④ 合并 fastbins 到 unsorted bin，遍历 unsorted bin
   - 找到合适大小 → 切分，返回，剩余放回
   - 找不到 → 进入 ⑤

⑤ 检查 largebins 并尝试 best-fit
   - 找到 → 切分返回
   - 找不到 → 进入 ⑥

⑥ 尝试扩大堆顶 (top chunk)
   - top chunk 够大 → 从 top 切分返回
   - 不够 → 进入 ⑦

⑦ brk() / mmap() 系统调用
   - 用户态 → 内核态 → 页表更新 → 返回用户态
   - 数千条指令 + 可能的 TLB flush
```

**关键事实：** tcache 命中时 malloc 已经很快（~10-20 ns），但一旦 miss，成本指数上升。而栈分配**永远是** 1 条指令。

### 4.3 `free(ptr)` 为什么也慢

```
free(ptr):

① 根据 ptr 找到 chunk header（ptr - 16），读取 size
② 检查 tcache 对应 bin 是否 < 7 个
   - 是 → 放入 tcache → 返回 ✅ 最快路径
   - 否 → 进入 ③

③ 放入 fastbins 或 unsorted bin（需 arena 锁）
④ 检查前后相邻块是否空闲
   - 是 → 合并（unlink + 更新 size + 重新链接）
   - 合并可能触发更大的合并链
⑤ 更新 arena 统计信息
⑥ 如果 top chunk 相邻 → 收缩堆（可能 munmap）
```

对比栈：`mov rsp, rbp` 或 `add rsp, N`——仍然只一条指令，释放 1 个字节和释放 10000 个字节都是 O(1)。

### 4.4 碎片化：为什么堆越来越慢

```
初始状态:
┌──┬──┬──┬──┬──┬──┬──┬──┐
│  空闲区域 (top chunk)    │
└──┴──┴──┴──┴──┴──┴──┴──┘

分配 A(2), B(2), C(2):
┌──┬──┬──┬──┬──┬──┬──┬──┐
│ A│ A│ B│ B│ C│ C│空闲  │
└──┴──┴──┴──┴──┴──┴──┴──┘

free(B):
┌──┬──┬──┬──┬──┬──┬──┬──┐
│ A│ A│空│空│ C│ C│空闲  │  ← 中间出现一个空洞
└──┴──┴──┴──┴──┴──┴──┴──┘

分配 D(3): 需要 3 个单位
┌──┬──┬──┬──┬──┬──┬──┬──┐
│ A│ A│ 空 │ C│ C│ D│ D│ D│  ← B 的空洞装不下 D(3)
└──┴──┴──┴──┴──┴──┴──┴──┘  D 只能放末尾，B 的空洞浪费了

经过数万次循环后：
┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│A│A│空│C│C│空│E│空│D│D│D│F│  ← "瑞士奶酪"
└─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
总空闲: 6 单元
最大连续空闲: 2 单元  → 分配 3 单元会失败，即使总空间足够
```

栈的 LIFO 属性**不可能**产生这种碎片——后分配的永远先释放。

### 4.5 C++ 中的堆

```cpp
// C 风格
int* p = (int*)malloc(sizeof(int));   // 分配原始内存，不调用构造函数
free(p);                                // 释放，不调用析构函数

// C++ 风格
int* p = new int(42);                   // ① malloc + ② 调用构造函数
delete p;                               // ① 调用析构函数 + ② free

// 现代 C++：永远不要手写 new/delete
auto up = std::make_unique<int>(42);    // 智能指针，出作用域自动释放
auto sp = std::make_shared<int>(42);    // 引用计数
auto v = std::vector<int>{1, 2, 3};     // vector 内部的堆缓冲区自动管理
```

## 5. C++ 存储期（Storage Duration）——栈和堆在语言规范中的正式名称

C++ 标准定义了四种**存储期**，这才是语言层面的正式概念：

| 存储期 | 对应位置 | 何时创建 | 何时销毁 | 示例 |
|---|---|---|---|---|
| **自动** (automatic) | 栈 | 进入作用域/语句块 | 离开作用域 | 局部变量 `int x;` |
| **动态** (dynamic) | 堆 | `new` / `malloc` | `delete` / `free` | `new Widget()` |
| **静态** (static) | Data/BSS 段 | 程序启动 | 程序结束 | 全局变量、`static` 局部变量 |
| **线程** (thread) | 线程栈 | 线程启动 | 线程结束 | `thread_local` 变量 |

```cpp
int global = 1;              // 静态存储期（Data 段）

void Foo() {
    static int calls = 0;    // 静态存储期（Data 段），只初始化一次
    calls++;

    int local = 42;          // 自动存储期（栈），离开 Foo 时销毁

    auto* p = new int(100);  // p 本身在栈上（自动存储期）
                              // *p 在堆上（动态存储期）
    delete p;
}
```

## 6. 从 C++ 代码追踪到内存

看一个完整例子：

```cpp
#include <vector>
#include <memory>

class Widget {
    int id_;                              // 4 bytes
    std::string name_;                    // 32 bytes (SSO buffer in MSVC)
};

int main() {
    // ① 栈分配
    Widget w;                             // w 本体（~40B）在栈上
                                           // w.name_ 的 SSO 小缓冲区也在栈上

    // ② vector：控制块在栈上，元素在堆上
    std::vector<Widget> widgets;          // 控制块（3个指针=24B）在栈上
    widgets.emplace_back();               // 元素的 Widget 对象在堆上（vector 内部分配）

    // ③ unique_ptr：指针在栈上，对象在堆上
    auto p = std::make_unique<Widget>();  // p（8B指针）在栈上
                                           // *p 的 Widget 对象在堆上

    // ④ shared_ptr：控制块和对象都在堆上
    auto sp = std::make_shared<Widget>(); // sp（16B：ptr+控制块ptr）在栈上
                                           // 引用计数+Widget 对象在一起（堆上，一次分配）

    // ⑤ optional：值内嵌
    std::optional<Widget> ow;             // ow（~48B：bool+Widget+padding）在栈上
    ow.emplace();                         // Widget 就地构造在 optional 内部，仍在栈上

    return 0;
    // w 自动析构 → w.name_ 的 string 自动析构
    // widgets 自动析构 → vector 释放堆缓冲区 → 各元素的 string 自动析构
    // p 自动析构 → delete Widget → string 析构 → 释放堆内存
    // sp 自动析构 → 引用计数减1 → 归零则 delete Widget → 释放堆内存
    // ow 自动析构 → 如果有值则析构 Widget → 无堆释放
}
// 六个对象，只有一处隐式堆分配（vector 内部）和一处显式堆分配（make_unique）——
// RAII 保证全部自动释放，没有泄漏。
```

## 7. 性能对比——从 perf stat 看根因

前面的 §3.4 和 §4 从机制上解释了为什么栈快堆慢。这一节用**实际测量数据**把它们串起来。

### 7.1 宏观数据：延迟对比

| 操作 | 耗时 | 关键瓶颈 | 对应根因 |
|---|---|---|---|
| 栈"分配" `sub rsp, 4096` | ~0.5 ns | 无（被乱序执行吸收） | 根因二：单指令 |
| 栈首次访问新页 | ~1000 ns | page fault（仅首次） | 根因一：OS 延迟映射 |
| `malloc(32)` tcache 命中 | ~10-20 ns | 几条指针操作 | 根因二的反面：malloc 最快也要 ~10条指令 |
| `malloc(32)` tcache miss | ~50-100 ns | arena 锁 + bin 搜索 | 根因五：锁竞争；根因三反面：bin 遍历 |
| `malloc(1MB)` 大块 | ~1-10 μs | `mmap()` 系统调用 | 系统调用 = 用户↔内核切换 |
| `free(32)` tcache 路径 | ~5-10 ns | 指针操作 | 根因四反面：需更新 chunk header |
| `free(32)` 需合并 | ~50-200 ns | arena 锁 + unlink + coalesce | 根因三反面：碎片需要合并 |
| 栈访问 `[rbp-8]` | ~1-3 ns | L1 命中（几乎必然） | 根因六：缓存热度 |
| 堆访问 `*ptr`（刚分配） | ~1-3 ns | L1 命中（刚用过） | — |
| 堆访问 `*ptr`（很久以前分配） | ~50-100 ns | L3 miss → 主存访问 | 根因六反面：缓存被冲刷 |

### 7.2 perf stat 分解：栈分配 vs 堆分配

用一个微基准对比——分配并初始化 1000 个 `int`，各执行 1M 次：

```
栈版本 perf stat 输出:
  int arr[1000];  arr[0] = 42;

  instructions:        2,000,012  ← 几乎全是循环开销，分配本身只是一条 sub
  branches:            1,000,008
  branch-misses:               4  ← 循环分支预测几乎完美
  cache-misses:              123  ← 栈连续，缓存命中率 > 99.9%
  page-faults:                 0  ← 栈页面已在首次使用时映射好
  cpu-cycles:          3,000,100  ← ~3M cycle / 1M 次 = 每次 3 cycle

堆版本 perf stat 输出:
  int* arr = new int[1000]; arr[0] = 42; delete[] arr;

  instructions:       15,300,420  ← 约 7.5x！malloc + free 内部有大量指令
  branches:            3,200,310  ← 更多的条件判断
  branch-misses:          48,200  ← malloc 内部分支难以预测（bin 状态各异）
  cache-misses:           12,300  ← malloc 内部数据结构分散在不同缓存行
  page-faults:                23  ← 偶尔需要 brk() 或 mmap
  cpu-cycles:         28,500,000  ← ~28.5M cycle / 1M 次 = 每次 28.5 cycle

差距来源:
  extra instructions:  ~13M → 主要是 malloc 内部的 bin 遍历 + free 的合并逻辑
  branch misses:       ~48K → malloc 中的条件分支（bin 是否为空、大小是否匹配等）
  cache misses:        ~12K → malloc 的元数据（chunk headers, bins, tcache）散落各处
```

**本质：** 不是"栈有一条快指令"——而是栈分配根本不需要做 malloc 必须做的那堆事。差距不在指令速度，在指令数量。

### 7.3 缓存局部性的一阶与二阶效应

```
一阶效应：访问模式

  栈（顺序访问）:
    帧地址: 0x7fff1000 → 0x7fff1100 → 0x7fff1200
    CPU 硬件预取器: "我看到你在按步长访问，帮你把后面的也加载好"
    → 在真正访问之前，数据已经在 L1 里了

  堆（随机访问）:
    对象地址: 0x7f001000 → 0x7f00a300 → 0x7f003800
    CPU 预取器: "我看不懂你的模式..."
    → 每次访问都是真正的 cache miss

二阶效应：TLB (Translation Lookaside Buffer)

  栈:
    8MB 连续虚拟地址 = 8MB / 4KB = 2048 页
    如果工作集集中在最近几帧，只需 10-50 个 TLB entry
    → TLB 命中率极高

  堆:
    每次 malloc 可能返回完全不相邻的虚拟地址
    → TLB 条目被多个不相干的区域瓜分
    → 频繁 TLB miss → 需要查页表（多 4 次内存访问）
```

### 7.4 分配/释放完整路径对比

用一个更公平的场景——分配后立即释放，重复 N 次：

```
栈 (N=1M):
  for i in 0..1M:
    int arr[1000];    // ← sub rsp, 4000
    arr[0] = 42;
    // 作用域结束      // ← add rsp, 4000（或 mov rsp, rbp）

  总分配次数: 1M
  释放方式: 隐式，零指令（编译器复用栈帧，循环内根本不变 rsp）
  实际分配/释放操作: 可能只有 1 次（编译器将分配提升到循环外）
  → 编译器优化后，基准测试可能根本测不到分配开销

堆 (N=1M):
  for i in 0..1M:
    int* arr = new int[1000];
    arr[0] = 42;
    delete[] arr;

  总分配次数: 1M
  每次: malloc(4000) + free(ptr) = ~50-500+ ns
  编译器无法优化掉（malloc/free 有全局副作用）
  → 绝对无法消除
```

**这揭示了一个更深层的真相：** 栈分配可以被编译器彻底优化掉（比如将数组提升到循环外或直接复用寄存器），而堆分配由于有全局副作用（修改 allocator 内部状态），编译器必须保留每次调用。

### 7.5 多线程下的放大效应

```
单线程:
  malloc: ~50 ns（tcache 命中）
  栈:    ~0.5 ns
  差距:  100x

4 线程并发分配:
  malloc: ~50-500+ ns（取决于 arena 竞争）
  栈:    ~0.5 ns（每个线程独立栈，无竞争）
  差距:  100-1000x

16 线程高并发:
  malloc: 可能微妙级（arena 锁激烈竞争，或频繁 mmap）
  栈:    仍 ~0.5 ns
  差距:  2000x+
```

这就是为什么现代高性能分配器（tcmalloc, jemalloc）的核心设计目标就是 per-thread cache——尽可能让 `malloc` 在无锁路径上完成。

## 8. 常见陷阱

### 8.1 栈溢出

```cpp
// ❌ 巨大局部变量
void Bad() {
    int huge[10000000];          // ~40 MB！默认栈 ~1-8 MB → 栈溢出
}

// ✅ 大对象放堆上
void Good() {
    auto huge = std::make_unique<int[]>(10000000);  // 堆上，安全
}
```

### 8.2 悬空指针

```cpp
// ❌ 返回局部变量的地址——这是最经典的栈/堆混淆错误
int* DanglingStack() {
    int x = 42;                   // x 在栈上
    return &x;                    // 返回时 x 已经析构了！
}                                 // 调用者在操作已回收的栈内存

// ✅ 返回堆上的对象（通过智能指针）
std::unique_ptr<int> Safe() {
    return std::make_unique<int>(42);
}

// ❌ 另一种悬空：delete 之后仍持有指针
int* p = new int(42);
delete p;
*p = 100;                         // 未定义行为
```

### 8.3 内存泄漏（堆特有）

```cpp
// ❌ 忘记 delete
void Leak() {
    auto* p = new Widget();       // 在堆上
    if (Something()) return;      // 提前返回——泄漏！
    delete p;
}

// ✅ RAII：不可能泄漏
void NoLeak() {
    auto p = std::make_unique<Widget>();
    if (Something()) return;      // p 自动析构——不可能泄漏
}
```

### 8.4 碎片化（堆特有）

```
// 多次小分配 → 碎片化
for (int i = 0; i < 100000; ++i) {
    auto* p = new char[random(10, 100)];  // 大小不一
    // ... 不释放，或乱序释放
}
// 堆变成"瑞士奶酪"：总空闲很多，但没有一块足够大
// → 新的大分配失败，即使总空闲数倍于请求
```

## 9. C++ 中"在栈上却在用堆内存"的特殊情况

有些东西看起来在栈上，内部其实在操作堆：

```cpp
// std::string: SSO 边界
std::string s1 = "hello";         // 短字符串：完全在栈上（SSO，Small String Optimization）
std::string s2(10000, 'x');       // 长字符串：控制块在栈上，字符数据在堆上

// std::vector: 控制块在栈上，元素永远在堆上
std::vector<int> v = {1, 2, 3};   // v 对象（3 个指针）在栈上
                                   // v 的元素 [1,2,3] 在堆上

// std::optional<T>: T 内嵌在 optional 中——如果 optional 在栈上，T 也在栈上
std::optional<std::string> o = "hello";  // sso 范围内：全部在栈上
std::optional<std::array<int, 1000>> big; // optional + array 全部在栈上
```

## 10. C++ 中"看起来在堆上却在用栈"的特殊情况

```cpp
// placement new：在栈内存上构造对象
alignas(Widget) char buffer[sizeof(Widget)];   // 栈上的原始内存
Widget* w = new (buffer) Widget(42);           // 在栈上构造！没有 malloc
w->~Widget();                                  // 手动析构（不会释放内存，因为是栈上的）

// std::pmr + monotonic_buffer_resource
#include <memory_resource>
char stack_buf[4096];                          // 栈上的缓冲区
std::pmr::monotonic_buffer_resource pool(stack_buf, sizeof(stack_buf));
std::pmr::vector<Widget> v(&pool);             // vector 的元素从栈上的缓冲区分配！
v.emplace_back();                              // 没有堆分配
```

## 11. 再回到本项目的内存图景

```
程序启动:
┌── Data 段 ──────────────────────────────┐
│  (静态变量、虚表等——本项目目前没有)       │
└─────────────────────────────────────────┘
┌── 栈 ───────────────────────────────────┐
│  main()                                 │
│    ConsoleApp app("data/tasks.json")     │  ← app 在栈上
│      TaskRepository repo_                │  ← repo_ 在栈上（app 的成员）
│        vector<MeasurementTask> tasks_    │  ← 控制块在栈上（repo_ 内）
│                                          │
│  ConsoleApp::Run()                       │
│    局部变量、菜单选择等                    │  ← 全在栈上
└──────────────────────────────────────────┘
┌── 堆 ───────────────────────────────────┐
│  tasks_ 的元素缓冲区                      │  ← vector 内部分配
│    [MeasurementTask #1]                  │  ← 对象内嵌在缓冲区
│      name_ (如果 > SSO 阈值)              │  ← 长字符串的字符数据
│      result_ (optional 内嵌，无额外分配)    │
│    [MeasurementTask #2]                  │
│    ...                                   │
└──────────────────────────────────────────┘
```

**没有一次 `new`，没有泄漏风险，所有资源由 RAII 自动管理。**

## 12. 要点

1. **栈是硬件支持的连续内存，分配只需移动一个寄存器**——秒杀一切速度，但空间有限（~1-8 MB/线程）。
2. **堆是运行时库管理的散列内存，分配需要搜索和系统调用**——灵活但慢，而且有碎片/泄漏风险。
3. **C++ 的"自动存储期" = 栈，"动态存储期" = 堆**——前者是默认，后者需显式申请。
4. **一个变量的存储位置取决于声明方式，不取决于类型**——`int* p`（栈）和 `*p`（堆）是两个位置。
5. **RAII 是栈语义的延伸**——利用"离开作用域自动析构"来管理堆资源。
6. **现代 C++ 中堆分配是隐式的**——`vector`/`string`/`make_unique` 帮你管理，你不写 `new`。
7. **大对象用堆，小对象用栈**——一个数组超过几十 KB 就值得考虑堆分配。
8. **`std::optional<T>` / `std::variant<T...>` 内嵌值**——如果它们本身在栈上，T 也在栈上，没有隐式堆分配。

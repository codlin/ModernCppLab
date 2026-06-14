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

> **但这只是故事的一半。** 线程栈为什么在多核 CPU 上仍然安全？L1 缓存不一致怎么办？线程在核心之间迁移会出问题吗？M:N 协程模型下栈还安全吗？——详见 §3.5。

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

### 3.5 深入：线程栈、多核缓存一致性与数据同步

前面 §3.4 根因五说了"每个线程有独立栈，所以无需锁"。但这句话背后藏着几个关键问题：

- 线程在 CPU 核心之间迁移时，L1 缓存不一样怎么办？
- 栈上的局部变量被共享给其他线程后还安全吗？
- M:N 协程模型（例如 C++20 无栈协程）下，"线程私有栈"这个前提还成立吗？
- `volatile` 能不能解决多线程同步问题？

这些问题的答案串起了**三层一致性模型**——理解这个模型，才算真正理解栈的线程安全性。

---

#### 3.5.1 第一层：线程栈的"所有权"——为什么默认安全

每个 OS 线程在创建时，操作系统会为它分配一段**独立的栈空间**（通常 1-8 MB）：

```
        Thread A                         Thread B
   ┌──────────────────┐            ┌──────────────────┐
   │  Stack A          │            │  Stack B          │
   │  0x7fff_1000      │            │  0x7ffe_f000      │
   │  ┌──────────────┐ │            │  ┌──────────────┐ │
   │  │ frame: foo() │ │            │  │ frame: bar() │ │
   │  │   int x = 10 │ │            │  │   int y = 20 │ │
   │  │   int z = 30 │ │            │  │   int w = 40 │ │
   │  └──────────────┘ │            │  └──────────────┘ │
   │       RSP ────────┘            │       RSP ────────┘
   └──────────────────┘            └──────────────────┘
        不同的虚拟地址                  不同的虚拟地址
        映射到不同的物理页              映射到不同的物理页
```

关键事实：

1. **Thread A 的栈地址和 Thread B 的栈地址不同**——它们不是同一块内存。
2. **每个线程有自己的栈指针寄存器 RSP**——线程切换时由 OS 保存/恢复。
3. **栈上的局部变量"默认不共享"**——除非你显式把地址/引用传给其他线程。

所以 `void worker() { int x = 10; x += 1; }` 在两个线程中同时执行：`x` 在 Thread A 的栈上和 `x` 在 Thread B 的栈上是**不同的物理内存地址**，不存在竞争。

**判断规则（核心）**：

```
数据位置              默认是否共享    是否需要同步
─────────────────────────────────────────────────
普通函数局部变量          否              否
线程自己的栈帧            否              否
被引用捕获传给其他线程     是              是
全局变量                 是              是
static 局部变量          是              是
堆对象                 取决于用法        共享则需要
thread_local 变量        否（每线程一份）  否
```

**结论：栈/堆/全局不是关键，"是否被多个线程访问"才是关键。**

---

#### 3.5.2 第二层：线程在 CPU 核心间迁移——L1 缓存不一致怎么办？

这是最常见的困惑。假设：

```
t1 时刻: Thread A 在 CPU1 上运行，CPU1 的 L1 缓存了 Stack A 的部分数据
t2 时刻: Thread A 被 OS 调度到 CPU2 上运行
        CPU2 的 L1 中没有 Stack A 的数据
        → "数据丢了？读到旧值？"
```

##### 答案：不会。三个机制保证正确性。

**机制一：栈数据在内存里，不在 L1 里**

L1 cache 只是主内存的**临时副本**，不是数据的"家"。线程 A 的栈真实存在于物理内存中。CPU1 L1 中的缓存行是那片内存的副本。线程迁移时：

- OS 保存 Thread A 的寄存器（包括 RSP、RIP 等）到线程控制块
- OS 恢复 Thread A 的寄存器到 CPU2
- CPU2 拿到同一个 RSP，访问同一虚拟地址
- MMU 将虚拟地址翻译为同一物理地址

**机制二：CPU 缓存一致性协议（Cache Coherence）**

现代多核 CPU 有硬件级缓存一致性协议（MESI / MOESI），保证**同一物理地址**在各核心缓存中的副本不会永久矛盾：

```
CPU1 L1                         CPU2 L1
┌──────────────────┐           ┌──────────────────┐
│ Cache line N     │           │ Cache line N     │
│ 状态: Modified ✗  │←────────→│ 状态: Invalid    │
│ (Thread A 刚写过) │ coherence │ (或 Shared)      │
└──────────────────┘  protocol └──────────────────┘
         │                            │
         └────────────┬───────────────┘
                      ↓
              共享的 L3 Cache
                      ↓
                 主内存 (DRAM)
```

当 CPU2 访问同一物理地址时，硬件自动通过一致性协议拿到最新数据：
- CPU2 发起读请求
- 一致性协议探测到 CPU1 持有 Modified 副本
- CPU1 将数据写回（Writeback）或直接转发（Cache-to-cache transfer）
- CPU2 获得最新数据

**整个过程是硬件自动完成的，程序员不需要干预。**

**机制三：同一线程不会同时在两个 CPU 上执行**

```
Thread A 的执行时间线:

CPU1: [运行] [运行] ──被切走──                           ──[运行]──
                        ↓ 保存 RSP/RIP/寄存器               ↑ 恢复
CPU2:                                ──[运行] [运行] [运行]──┘
                                          ↑
                                   同一个 RSP → 同一栈内存
```

Thread A 在任意时刻只在一个 CPU 上运行。所以不存在"A 同时在 CPU1 和 CPU2 上写自己的栈"这种并发场景。线程自己的栈对线程自己是**串行访问**的。

##### 关键区分：线程迁移 ≠ 多线程共享

```
线程迁移（Thread Migration）:
  同一个线程在不同时间、不同 CPU 上访问自己的栈
  → 硬件缓存一致性 + OS 上下文恢复 = 自动正确
  → 不需要 mutex / atomic

多线程共享（Multi-threaded Sharing）:
  两个不同线程同时访问同一块内存
  → 需要 mutex / atomic / condition_variable
  → 缓存一致性不够，还需要内存顺序保证
```

---

#### 3.5.3 深入内存屏障：从"反直觉的现象"到"硬件根因"再到"C++ 解法"

硬件缓存一致性只保证**同一地址**的缓存副本最终一致，但它**不保证**：

- 操作顺序符合你的直觉
- 复合操作（读-改-写）是原子的
- 线程 B 立刻看到线程 A 的写入
- 编译器不会重排指令
- CPU 不会乱序执行

内存屏障就是用来解决"顺序"和"可见性"的。但不要一上来就钻进 CPU 微架构——先看两个**反直觉的问题**，带着疑问再去挖根因，理解会深得多。

---

##### 3.5.3.1 经典问题一：Store-Load 重排（Dekker 算法）

两个线程各自的 flag，最初都是 `false`。请问两个线程可能**同时进入临界区**吗？

```cpp
bool flag0 = false;  // 线程 0 的 flag
bool flag1 = false;  // 线程 1 的 flag

// 线程 0                              // 线程 1
flag0 = true;                           flag1 = true;
if (!flag1) {                           if (!flag0) {
    // 临界区                               // 临界区
}                                       }
```

直觉告诉你"不可能"——因为线程 0 先把自己的 flag 设为 `true`，再去读对方的 flag，至少有一个线程会看到对方的 `true`。

**但答案是：可能。** 在 Store-Load 重排下：

- 线程 0 的 `flag0 = true` 还在 Store Buffer 里，尚未对其他核心可见
- 线程 0 的 `if (!flag1)` 从自己的 L1 读到了 flag1 的旧值 `false` → 进入临界区
- 线程 1 同理。两个线程同时进入临界区。

等一下——**为什么会这样？** 硬件缓存一致性不是保证同一地址不会矛盾吗？答案在 §3.5.3.3。先记住这个"反直觉"的感觉。

修复方案（§3.5.3.5 详解）：将 flag 改为 `std::atomic<bool>`，用默认的 `seq_cst`，编译器会在 x86 上生成 `LOCK` 前缀、在 ARM 上生成 `DMB SY`，强制排空 Store Buffer。

---

##### 3.5.3.2 经典问题二：Message Passing —— 一条消息的两个字段

线程 A 先写数据再发信号，线程 B 看到信号后读数据。线程 B 一定看到 `data == 42` 吗？

```cpp
int data = 0;          // 普通变量
bool ready = false;    // 普通变量

// 线程 A: 生产者                         // 线程 B: 消费者
data = 42;                                if (ready) {
ready = true;                                 std::cout << data;  // 一定输出 42？
                                          }
```

直觉告诉你是——既然 `data = 42` 写在 `ready = true` 前面，线程 B 看到 `ready == true` 时应该已经能看到 `data == 42`。

**但答案是：不一定。** 原因有两个来源：

1. **编译器重排**：编译器发现 `data` 和 `ready` 没有数据依赖，可能交换两条写入的顺序。
2. **CPU Store-Load 重排**：即使编译器没重排，CPU 的 Store Buffer 也可能让 `ready = true` 先变成对其他核心可见，而 `data = 42` 还在路上。

修复方案（§3.5.3.5 详解）：

```cpp
std::atomic<int> data{0};
std::atomic<bool> ready{false};

// 线程 A
data.store(42, std::memory_order_relaxed);    // ① 写数据
ready.store(true, std::memory_order_release);  // ② 发信号——release 保证 ① 不会跑到 ② 之后

// 线程 B
if (ready.load(std::memory_order_acquire)) {   // ③ acquire——看到 ② 后，保证看到 ①
    std::cout << data.load(std::memory_order_relaxed); // ④ 一定 42
}
```

**为什么不能用 `relaxed` 替代 `release/acquire`？** 因为 `relaxed` 只保证原子性，不保证顺序——CPU 仍然可能重排。

**release/acquire 的"半屏障"特性**：

```
release store 之前: 所有内存操作（读+写）必须完成   ← 单向屏障：只挡前面
release store:      普通 store（x86 上无额外指令）
release store 之后: 无约束——后面的操作可以跑进来

acquire load 之前:  无约束
acquire load:       普通 load（x86 上无额外指令）
acquire load 之后:  所有内存操作（读+写）不能跑到 load 前面  ← 单向屏障：只挡后面
```

这就是 release/acquire 比 seq_cst 便宜的原因——它们只限制一个方向。

---

##### 3.5.3.3 CPU 为什么会重排——Store Buffer 与 Invalidate Queue

上面两个问题（Dekker 同时进临界区、Message Passing 读到旧值）的**共同根因**是 CPU 微架构中的两个异步缓冲区：

```
Core 0                                              Core 1
┌──────────────────────────────┐       ┌──────────────────────────────┐
│ Pipeline                      │       │ Pipeline                      │
│   ↓ store data = 42          │       │   ↓ if (ready) { ... }        │
│   ↓ store ready = true       │       │                                │
│                              │       │                                │
│ ┌──────────────────────────┐ │       │ ┌──────────────────────────┐ │
│ │     Store Buffer (FIFO)  │ │       │ │  Invalidate Queue        │ │
│ │  [data=42] [ready=true]  │ │       │ │  [Inv: data] ← 来自 Core0│ │
│ └──────────┬───────────────┘ │       │ └──────────┬───────────────┘ │
│            │ 异步刷入 L1      │       │            │ 异步处理         │
│            ↓                 │       │            ↓                 │
│ ┌──────────────────────────┐ │       │ ┌──────────────────────────┐ │
│ │ L1 Cache                 │ │       │ │ L1 Cache                 │ │
│ │  data: Modified → Shared │─┼───→──┼→│  data: Invalid → Shared   │ │
│ │  ready: Modified → Shared│ │       │ │  ready: Shared            │ │
│ └──────────────────────────┘ │       │ └──────────────────────────┘ │
└──────────────────────────────┘       └──────────────────────────────┘
```

**Store Buffer**：Core 0 执行 store 时，把值先丢进 Store Buffer，然后**立刻执行下一条指令**——不等 L1 缓存完成。但 Store Buffer 是 FIFO 且异步刷入 L1：`ready` 的 cache line 如果已经在 Core 0 的 L1 中，它的 store 可以立刻完成；而 `data` 的 cache line 如果被 Core 1 持有，则需要先发 Invalidate 到 Core 1、等 Ack 回来。于是 Core 1 看到的顺序可能是 **`ready = true` 先到达，`data = 42` 后到达**。

**Invalidate Queue**：Core 1 收到 Core 0 发来的 Invalidate 消息后，为了不阻塞，先丢进 Invalidate Queue 立刻 Ack。如果 Core 1 在 Invalidate 被处理之前就读了 `data`，读到的就是**自己 L1 中的旧值**。

**这就是两个经典问题的物理根因：不是 L1 坏了，不是缓存一致性协议失效了，是缓冲区让操作的"生效时间"和"执行时间"脱了钩。**

---

##### 3.5.3.4 四种内存屏障——从微架构到指令

要制服 Store Buffer 和 Invalidate Queue，CPU 提供了四种屏障（术语来自 Linux 内核）：

```
               Load (读)           Store (写)
               ──────────          ──────────
Load (读)  │   LoadLoad 屏障     LoadStore 屏障
           │   "读不乱序"         "读不跑到写后面"
           │
Store (写) │   StoreLoad 屏障    StoreStore 屏障
           │   "写不乱序到读后"    "写不乱序"
           │   ← 最贵！需要排空
           │      Store Buffer
```

| 屏障类型 | 保证 | x86-64 指令 | ARM64 指令 | 典型代价 |
|---|---|---|---|---|
| **StoreStore** | Store A 一定在 Store B 之前对其他核心可见 | （x86 天然保证，无需指令） | `DMB ST` | ~1 cycle |
| **LoadLoad** | Load A 一定在 Load B 之前执行 | （x86 天然保证，无需指令） | `DMB LD` | ~1 cycle |
| **LoadStore** | Load A 一定在 Store B 之前完成 | （x86 天然保证，无需指令） | `DMB ISH` | ~1 cycle |
| **StoreLoad** | Store A 的结果一定对 Load B 可见 | `MFENCE` / `LOCK OR [RSP],0` | `DMB SY` / `DMB ISHST` | **~30-100 cycle** |

**x86-64 是强内存模型**：硬件只允许 Store-Load 重排，其他三种被禁止。所以 `volatile` 在 x86 上"碰巧看起来能工作"——这是不可移植的幻觉。

**ARM64 是弱内存模型**：四种重排都可能发生。在 ARM 设备上不加屏障的代码更容易暴露 bug。写可移植代码必须依赖 `std::atomic` 而非架构特性。

---

##### 3.5.3.5 C++ memory_order —— 6 种排序约束

C++ 将上面的硬件概念抽象为可移植接口，根据目标架构自动选择指令：

```
C++ memory_order         保证的屏障          x86-64 实际指令      ARM64 实际指令
──────────────────────────────────────────────────────────────────────────────
memory_order_relaxed     无（仅原子性）      普通 MOV           普通 MOV/LDR+STR
                         不阻止任何重排

memory_order_consume     依赖数据顺序       普通 MOV           普通 MOV
                         （不推荐，编译器      （不可靠）         （不可靠）
                          实现有争议）

memory_order_acquire     后续读/写不          普通 MOV           LDR + DMB LD
   (load 操作)           会跑到此 load 之前

memory_order_release     之前的读/写不         普通 MOV           DMB ST + STR
   (store 操作)          会跑到此 store 之后

memory_order_acq_rel     合并 acquire +       XCHG (隐式          LDR + DMB SY + STR
   (RMW 操作)            release             full barrier)

memory_order_seq_cst     全局单一全序          LOCK XCHG /        LDR + DMB SY + STR
   (默认)                (所有 seq_cst 操作    LOCK CMPXCHG      （同 acq_rel，但需要
                          在所有线程中有一                       额外保证与后续 seq_cst
                          致的总顺序)                             store 的顺序）
```

**关键映射**：
- 在 x86 上，`acquire` 是**免费的**（load 天然有 acquire 语义），`release` 也是**免费的**（store 天然有 release 语义）。只有 `seq_cst` 需要 `MFENCE`/`LOCK` 前缀。
- 在 ARM64 上，`acquire` 和 `release` 都需要插入 `DMB` 指令。

**这就是为什么写可移植的 lock-free 代码必须用 `std::atomic` + explicit memory_order**——编译器会为目标架构选择正确的指令。

现在回头看 §3.5.3.1 的 Dekker 问题：修复它需要 StoreLoad 屏障。在 `std::atomic<bool>` 上做 `store(true)`（默认 `seq_cst`），x86 生成 `LOCK XCHG`，ARM 生成 `DMB SY`——硬件强制排空 Store Buffer 后再读对方的 flag。

---

##### 3.5.3.6 seq_cst：当"全序"是必须的

`memory_order_seq_cst` 是默认值（也是最安全的）。除了 acquire+release 的保证外，还保证**所有 seq_cst 操作在所有线程中有一致的总顺序**：

```cpp
std::atomic<int> x = 0, y = 0;
int r1 = 0, r2 = 0;

// 线程 1                          // 线程 2                          // 线程 3
x.store(1, seq_cst);               y.store(1, seq_cst);               r1 = x.load(seq_cst);
//                                                                            r2 = y.load(seq_cst);

// 线程 4
// r1 = y.load(seq_cst);
// r2 = x.load(seq_cst);
```

在 `seq_cst` 下，所有线程观察到的 `(x=1, y=1)` 的发生顺序是一致的。而在 `acq_rel` 下，可能线程 3 认为"x 先于 y"，线程 4 认为"y 先于 x"。

**什么时候用 seq_cst？**
- 你不确定该用什么的时候（默认，最安全）
- 多个 atomic 变量之间存在依赖顺序时
- Dekker/Peterson 风格的互斥算法

**什么时候降级？**
- 单一生产者-消费者用 release/acquire
- 纯计数器用 relaxed

---

##### 3.5.3.7 `std::atomic_thread_fence` —— 独立屏障

`std::atomic_thread_fence` 和 atomic 操作上的 memory_order 有微妙区别：

```cpp
// 方式一：在 atomic 操作上指定 memory_order（推荐）
data.store(42, std::memory_order_release);

// 方式二：独立 fence（等价但更重）
data.store(42, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);
```

**fence 什么时候需要？**

1. **影响多个非 atomic 变量**（但这是一个灰色地带——最好用 atomic）：

```cpp
int payload[256];  // 非 atomic，但需要和 flag 一起发布
std::atomic<bool> flag = false;

// 线程 A
payload[0] = 42;
payload[1] = 99;
std::atomic_thread_fence(std::memory_order_release);  // 保证 payload 写入可见
flag.store(true, std::memory_order_relaxed);

// 线程 B
if (flag.load(std::memory_order_relaxed)) {
    std::atomic_thread_fence(std::memory_order_acquire);  // 保证看到 payload
    use(payload[0]);
}
```

2. **Dekker 模式必须用 seq_cst fence 而非 acq_rel**：

```cpp
// ❌ 用 acq_rel 不够——两个线程可能同时进入临界区
flag0.store(true, std::memory_order_release);   // store + release 不是 full barrier
if (!flag1.load(std::memory_order_acquire)) { /* 临界区 */ }

// ✅ 用 seq_cst fence（或直接用 seq_cst store/load）
flag0.store(true, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst);  // → x86: MFENCE
if (!flag1.load(std::memory_order_relaxed)) { /* 临界区 */ }
```

---

##### 3.5.3.8 编译器屏障 —— 别忘了编译器也在重排

CPU 不是唯一的重排来源。**编译器也会重排**你的 C++ 代码。`std::atomic` 同时阻止了编译器和 CPU 的重排。

```cpp
// ❌ 编译器可能把 b=1 排到 a=2 前面（因为没有数据依赖）
int a = 2;
int b = 1;

// ✅ atomic 阻止编译器重排
std::atomic<int> a;
std::atomic<int> b;
a.store(2, std::memory_order_relaxed);  // 即使 relaxed，编译器也不会把它们和
b.store(1, std::memory_order_relaxed);  // 其他 atomic 操作互相重排
```

**编译器屏障（非标准，不可移植）**：

```cpp
// GCC/Clang 编译器屏障——阻止编译器重排，但不生成 CPU 屏障指令
asm volatile("" ::: "memory");

// 标准方式：用 atomic_signal_fence（只阻止编译器重排，不阻止 CPU）
std::atomic_signal_fence(std::memory_order_seq_cst);
```

`atomic_signal_fence` 用于**同一线程内的信号处理函数**——只防止编译器重排，不插入 CPU 屏障（因为同一线程不会有 CPU 重排问题）。

---

##### 3.5.3.9 无锁编程中的屏障实战模式

**模式一：Seq-Lock（读写分离，写者不阻塞读者）**

```cpp
std::atomic<uint64_t> seq{0};
Data shared_data;  // 非 atomic 数据

// 写者
void Write(const Data& d) {
    uint64_t s = seq.load(std::memory_order_relaxed);
    seq.store(s + 1, std::memory_order_relaxed);  // 奇数 = 写入中
    std::atomic_thread_fence(std::memory_order_release);  // 保证 shared_data 写入在 seq++ 之前可见
    
    shared_data = d;  // 非原子写入
    
    std::atomic_thread_fence(std::memory_order_release);
    seq.store(s + 2, std::memory_order_release);  // 偶数 = 写入完成
}

// 读者
Data Read() {
    uint64_t s1, s2;
    Data result;
    do {
        s1 = seq.load(std::memory_order_acquire);
        if (s1 & 1) continue;  // 写者在写，重试
        std::atomic_thread_fence(std::memory_order_acquire);
        result = shared_data;
        std::atomic_thread_fence(std::memory_order_acquire);
        s2 = seq.load(std::memory_order_acquire);
    } while (s1 != s2);
    return result;
}
```

这是少数需要 `atomic_thread_fence` 的经典场景——保护**非 atomic 数据**的读写顺序。

**模式二：MPSC 无锁队列（Multi-Producer Single-Consumer）**

```cpp
// 简化版——展示 acquire/release 的实际使用
struct Node { int data; std::atomic<Node*> next{nullptr}; };

std::atomic<Node*> head;   // 消费者从这里读
Node* tail;                // 生产者从这里追加（仅消费者写）

// 生产者（多线程）
void Push(int val) {
    auto* n = new Node{val};
    Node* prev = head.exchange(n, std::memory_order_acq_rel);  // ① acq_rel = 看到最新的 head + 让其他生产者看到我的
    prev->next.store(n, std::memory_order_release);             // ② release = 保证 data 在 next 之前可见
}

// 消费者（单线程）
int Pop() {
    Node* h = head.load(std::memory_order_acquire);
    if (!h) return -1;
    while (h->next.load(std::memory_order_acquire) == nullptr) { /* spin */ }
    head.store(h->next, std::memory_order_release);
    int val = h->data;
    delete h;
    return val;
}
```

**模式三：纯计数器——`relaxed` 就够了**

```cpp
std::atomic<uint64_t> request_count{0};
std::atomic<uint64_t> error_count{0};

// 任意线程
void OnRequest()  { request_count.fetch_add(1, std::memory_order_relaxed); }
void OnError()    { error_count.fetch_add(1, std::memory_order_relaxed); }

// 日志线程（定期打印，不需要精确的 happens-before）
void Report() {
    uint64_t r = request_count.load(std::memory_order_relaxed);
    uint64_t e = error_count.load(std::memory_order_relaxed);
    // r 和 e 可能微微"过时"，但不会撕裂（原子性保证），且没有数据竞争
}
```

---

##### 3.5.3.10 内存屏障速查总表

| 机制 | 解决什么 | 阻止编译器重排 | 阻止 CPU 重排 | x86 代价 | ARM 代价 |
|---|---|---|---|---|---|
| `std::atomic` + `relaxed` | 原子性（无撕裂） | ✅ | 仅保证该变量原子 | ~1 cycle | ~1 cycle |
| `std::atomic` + `acquire` | 读侧的 happens-before | ✅ | 后续不跑到 load 前 | 免费 | `DMB LD` |
| `std::atomic` + `release` | 写侧的 happens-before | ✅ | 之前的不跑到 store 后 | 免费 | `DMB ST` |
| `std::atomic` + `acq_rel` | RMW 的 acquire+release | ✅ | 双向 | `XCHG` (含 LOCK) | `DMB SY` |
| `std::atomic` + `seq_cst` | 全局单一全序 | ✅ | StoreLoad 屏障 | `MFENCE` / `LOCK` | `DMB SY` |
| `std::atomic_thread_fence(acq)` | 独立 acquire 屏障 | ✅ | 同 acquire | 免费 | `DMB LD` |
| `std::atomic_thread_fence(rel)` | 独立 release 屏障 | ✅ | 同 release | 免费 | `DMB ST` |
| `std::atomic_thread_fence(seq_cst)` | 独立 full barrier | ✅ | StoreLoad 屏障 | `MFENCE` | `DMB SY` |
| `std::atomic_signal_fence` | 仅编译器屏障 | ✅ | ❌ | 免费 | 免费 |
| `std::mutex::lock()` | 互斥 + acquire | ✅ | acquire | 免费 | `DMB LD` |
| `std::mutex::unlock()` | 互斥 + release | ✅ | release | 免费 | `DMB ST` |
| CPU 缓存一致性 (MESI) | 同一地址副本一致 | N/A | 硬件自动 | 硬件自动 | 硬件自动 |

关键结论：**缓存一致性是硬件的"地基"，但 C++ 多线程正确性需要在此基础上叠加内存顺序保证。x86 的强内存模型掩盖了很多 bug——在 ARM 上这些 bug 会暴露出来。永远用 `std::atomic` 而不是 `volatile`，用明确的 memory_order 来表达你的意图。**

---

#### 3.5.4 栈上的数据被共享后——从安全到不安全

```cpp
// ✅ 安全：局部变量仅被当前线程访问
void Safe() {
    int x = 0;
    x++;  // 只有当前线程
}

// ❌ 不安全：局部变量被引用捕获，传给其他线程
void Unsafe() {
    int x = 0;  // 在调用者线程的栈上

    std::thread t1([&] { x++; });  // x 被 t1 共享
    std::thread t2([&] { x++; });  // x 被 t2 共享

    t1.join();
    t2.join();
    // 数据竞争！x 仍然是栈变量，但被多个线程并发写入
}
```

修复方式：

```cpp
// 修复一：atomic
void Safe1() {
    std::atomic<int> x = 0;
    std::thread t1([&] { x.fetch_add(1); });
    std::thread t2([&] { x.fetch_add(1); });
    t1.join(); t2.join();
}

// 修复二：mutex
void Safe2() {
    int x = 0;
    std::mutex m;
    std::thread t1([&] { std::lock_guard lk(m); x++; });
    std::thread t2([&] { std::lock_guard lk(m); x++; });
    t1.join(); t2.join();
}

// 修复三：join 本身建立 happens-before
void Safe3() {
    int result = 0;
    std::thread t([&] { result = 42; });
    t.join();
    // join() 保证：子线程的所有写入 happens-before join() 返回
    std::cout << result;  // 安全，一定输出 42
}

// ❌ 但这样不安全——在 join 之前读
void UnsafeAgain() {
    int result = 0;
    std::thread t([&] { result = 42; });
    std::cout << result;  // 数据竞争！和子线程的写入并发
    t.join();
}
```

**`std::thread::join()` 本身就是同步点**——子线程完成前的所有写入对 join 后的主线程可见。

---

#### 3.5.5 M:N 线程模型下的栈——协程 / Fiber / 无栈协程

M:N 模型 = M 个用户态任务映射到 N 个 OS 线程。例如：10000 个协程跑在 8 个内核线程上。

##### 情况一：有栈协程 / Fiber（如 Boost.Fiber）

每个 fiber 有自己的**用户态栈**（几 KB 到几 MB），仍然遵循"每个执行上下文有独立栈"的原则：

```
OS Thread 1                    OS Thread 2
┌─────────────────────┐       ┌─────────────────────┐
│ Fiber A stack        │       │ Fiber D stack        │
│ Fiber B stack        │       │ Fiber E stack        │
│ (当前运行: Fiber B)   │       │ (当前运行: Fiber D)   │
└─────────────────────┘       └─────────────────────┘
     ↑ 调度器切换                 ↑
     │ A ↔ B ↔ C ↔ ...           │ D ↔ E ↔ F ↔ ...
     │                             │
     └── 同一个 fiber 不会同时在 ──┘
         两个 OS 线程上执行
```

关键保证：**同一个 fiber 不会被并发调度到两个 OS 线程上**。所以 fiber 的栈仍然是串行访问——fiber 从线程 1 迁移到线程 2 时，靠 OS 线程迁移同样的机制（缓存一致性 + 上下文保存/恢复）保证正确。

##### 情况二：无栈协程（C++20 Coroutines）

C++20 协程**不在传统调用栈上保存局部变量**。跨 `co_await` 存活的局部变量会被提升到 **coroutine frame**（通常在堆上）：

```cpp
task<int> Foo() {
    int x = 1;              // x 可能提升到 coroutine frame
    co_await Something();   // 挂起点——x 必须存活到 resume 之后
    x++;                    // 对 coroutine frame 中的 x 操作
    co_return x;
}
```

内存布局对比：

```
普通函数调用:                          C++20 协程:

栈上:                                  堆上 (coroutine frame):
┌──────────────────┐                  ┌──────────────────────┐
│ Foo 的栈帧        │                  │ promise_type         │
│  int x = 1       │                  │ 跨 co_await 的局部变量 │
│  (co_await 后     │                  │   int x = 1          │
│   会被销毁——所以    │                  │  (co_await 后仍存活)   │
│   不能直接放栈上)   │                  └──────────────────────┘
└──────────────────┘
                  栈上:
                  ┌──────────────────┐
                  │ coroutine handle │  ← 一个指针，指向堆上的 frame
                  └──────────────────┘
```

无栈协程的安全模型：

- coroutine frame 应该**不被多个线程同时 resume**
- 如果同一个 coroutine frame 被多线程并发 resume → 需要同步
- 如果调度器保证一次只有一个线程执行某个 coroutine → 安全
- **注意：`thread_local` 在此模型下不可靠**——因为一个协程可能在 OS 线程之间迁移

##### 三种模型对比

| 模型 | 局部变量位置 | 默认安全性 | 注意事项 |
|---|---|---|---|
| 普通 OS 线程 | 线程自己的栈 | 安全（私有） | 跨线程共享时需要同步 |
| 有栈协程 (Fiber) | Fiber 自己的用户态栈 | 安全（私有） | 同一 fiber 不会被并发执行 |
| 无栈协程 (C++20) | coroutine frame（通常堆上） | 视调度器而定 | 不能依赖 `thread_local`；同一 frame 不应被并发 resume |

---

#### 3.5.6 `thread_local`：每线程一份独立变量

```cpp
thread_local int counter = 0;

void Worker() {
    counter++;  // 每个线程有自己的 counter
}
```

- Thread A: `counter = 1`, Thread B: `counter = 0`——不是同一个对象
- 适用于：线程本地缓存、日志 buffer、随机数生成器、减少锁竞争
- **局限**：`thread_local` 是 OS 线程本地，不是协程本地。在 M:N 协程调度下，协程跨 OS 线程迁移时 `thread_local` 的值会变化

---

#### 3.5.7 常见误区：`volatile` 不能解决多线程同步

```cpp
// ❌ 错误做法——volatile 不提供任何多线程保证
volatile bool ready = false;

// 线程 A
data = 42;
ready = true;   // volatile 不保证 data=42 对线程 B 可见

// 线程 B
if (ready) {
    // data 可能还是 0！
}
```

| `volatile` 的用途 | `std::atomic` 的用途 |
|---|---|
| 内存映射 I/O (MMIO) | 多线程同步 |
| 防止编译器优化掉某些访问 | 原子性 + happens-before + acquire/release |
| 信号处理函数中的标志位（有限场景） | 跨线程可见性 + 顺序保证 |
| **不**提供原子性 | 提供原子性 |
| **不**建立 happens-before | 建立 happens-before |

**多线程同步永远用 `std::atomic`，不要用 `volatile`。**

---

#### 3.5.8 三层一致性：完整心智模型

```
┌─────────────────────────────────────────────────────────────┐
│ 第三层：程序级数据竞争（C++ 语言层）                          │
│   - 非 atomic 变量被多线程并发访问 + 至少一个写 = 数据竞争 UB  │
│   - 需要: mutex / atomic / condition_variable / join /       │
│           future / latch / barrier                           │
├─────────────────────────────────────────────────────────────┤
│ 第二层：内存可见性与顺序（C++ 内存模型）                        │
│   - happens-before / synchronizes-with                       │
│   - acquire / release / seq_cst / relaxed                   │
│   - 需要: atomic + memory_order / mutex / fence              │
├─────────────────────────────────────────────────────────────┤
│ 第一层：硬件缓存一致性（CPU 硬件）                             │
│   - MESI / MOESI 协议                                        │
│   - 保证同一物理地址的缓存副本最终一致                          │
│   - 自动，程序员不直接控制                                     │
│   - 不保证操作顺序、不保证原子性                               │
└─────────────────────────────────────────────────────────────┘
```

**每一层都依赖下一层，但每一层解决不同的问题。** 硬件缓存一致性是地基，但光有地基不够——还需要 C++ 内存模型的顺序保证，以及同步原语来防止数据竞争。

---

#### 3.5.9 要点

1. **线程栈的"私有性"来自线程有独立栈空间，不是来自 L1 缓存。**
2. **线程在 CPU 核心间迁移时，OS 保存/恢复上下文 + 缓存一致性协议保证数据正确——不需要程序员干预。**
3. **线程迁移 ≠ 多线程共享。同一个线程自己的栈是串行访问的；多线程访问同一数据才需要同步。**
4. **缓存一致性协议只保证"同一地址不会永久矛盾"，不保证操作顺序和可见性。**
5. **内存屏障（acquire/release/fence）解决跨线程的 happens-before 关系。**
6. **栈变量一旦被共享（引用捕获、取地址传给其他线程），就不再因"在栈上"而安全——必须同步。**
7. **M:N 协程模型下，有栈协程仍保持栈私有性；无栈协程的局部变量提升到 coroutine frame，安全性取决于调度器。**
8. **`thread_local` 是 OS 线程本地，不是协程本地。**
9. **`volatile` 用于 MMIO，不用于多线程同步——用 `std::atomic`。**
10. **判断是否需要同步的唯一标准：是否多个线程并发访问 + 至少一个写。**

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

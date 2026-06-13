# 通用项目：GitHub Issues + Projects + docs + GitHub MCP 落地方案

> 推荐保存位置：`docs/github-mcp-workflow.md`
>
> 适用项目：C++、C#/.NET、WPF、CLI、SDK/Library、Web API、后端服务、数据采集系统、AI Agent 工程、内部工具、个人长期项目。
>
> 目标：让 Codex / Claude Code / VS Code Agent 结合仓库代码、GitHub Issues、GitHub Projects、Pull Requests、Actions、现有 `docs/` 文档和 GitHub MCP Server，生成并维护项目所需的模板、文档、标签、任务结构和基础流程。

---

## 1. 总体方案

本项目采用以下组合：

```text
GitHub Issues          管需求 / Bug / 研究 / 实验 / 重构 / 事故 / 验收任务
GitHub Projects        管状态、优先级、当前周计划和恢复现场
docs/*.md              管正式技术文档、研究结论、设计、规范和验收标准
Pull Requests          管代码变更、评审和验证记录
GitHub Actions         管构建、测试、静态检查、打包和发布
GitHub MCP Server      给 AI Agent 读写仓库、Issue、PR、Actions 和项目上下文
AGENTS.md              给 AI Agent 的项目执行规则
```

核心原则：

```text
Issue = 要做什么、为什么做、做到哪了、下一步是什么
docs/*.md = 已确认的长期规则、设计、规范和验收标准
PR = 实际代码变化和验证记录
Project = 当前状态、优先级、本周工作和阻塞项
MCP = 让 AI Agent 串联 Issue、代码、PR、CI、文档
```

---

## 2. 适用场景

适合以下项目：

```text
1. 项目会持续开发，不是一次性脚本。
2. 经常做一半中断，过几天或下周需要恢复现场。
3. 项目包含需求、设计、Bug、重构、测试、文档。
4. 希望 Codex / Claude / VS Code Agent 能按项目规则自动干活。
5. 希望 Issue、PR、docs、CI 之间形成闭环。
6. 不想为个人项目引入 Jira / YouTrack / Linear 等额外系统。
```

特别适合个人开发：

```text
1. 记录想法。
2. 拆分功能。
3. 追踪 Bug。
4. 沉淀设计。
5. 让 AI Agent 接着上次上下文继续。
6. 一周后还能知道从哪里接。
```

---

## 3. 推荐 docs 目录结构

通用项目建议使用以下文档结构。若项目已有自己的 `docs/` 结构，应优先保留现有结构，不要强行迁移。

```text
docs/
├─ 00-discussion/
├─ 01-architecture/
├─ 02-planning/
├─ 03-refactoring/
├─ 04-review/
├─ 05-bugfix/
├─ 06-knowledge/
└─ github-mcp-workflow.md
```

说明：

```text
1. 这是推荐结构，不是强制结构。
2. 如果项目已有编号目录，可按现有目录调整。
3. AI Agent 不应随意创建一套与现有文档并行的新目录。
4. 新文档必须放入语义正确的位置。
5. docs 根目录尽量只放总入口或流程类文档。
```

---

## 4. docs 目录职责

### 4.1 `00-discussion/`

用途：历史讨论、方案推演、需求讨论、AI 生成草稿、会议记录、临时想法。

性质：

```text
参考资料
历史上下文
可用于理解项目演进
不可直接视为最终设计事实
```

使用规则：

```text
1. 可以阅读，用于理解为什么这么设计。
2. 不应直接以 discussion 文档覆盖 architecture 文档。
3. 如果 discussion 与 architecture 冲突，优先采用 architecture。
4. 如果 discussion 中有有价值但未沉淀的内容，应整理到 architecture、planning 或 knowledge。
```

### 4.2 `01-architecture/`

用途：正式架构文档、长期有效设计、模块边界、核心流程、关键技术决策。

性质：

```text
正式设计依据
AI Agent 修改代码前必须优先阅读
Issue / PR / 测试设计应以此目录为主要依据
```

使用规则：

```text
1. 架构、模块边界、接口设计、数据流、关键机制写入此目录。
2. 不要在 docs 根目录随意新建重复的 architecture 文档。
3. 如果新增正式设计文档，应放在 architecture。
4. 如果只是短期计划或阶段任务，不应放在 architecture。
```

### 4.3 `02-planning/`

用途：当前阶段计划、开发推进顺序、里程碑、短中期任务安排。

性质：

```text
当前执行计划
AI Agent 开始任务前应读取
GitHub Project 和 Issue 应与此目录保持一致
```

使用规则：

```text
1. 当前阶段的任务拆分、优先级、下一步计划写入此目录。
2. 每次完成阶段性任务后，Codex 应建议是否更新计划文档。
3. 如果 GitHub Project 中状态变化较大，应同步 planning 文档。
```

### 4.4 `03-refactoring/`

用途：重构计划、模块迁移方案、代码结构调整、依赖整理、技术债治理。

使用规则：

```text
1. 只有明确涉及重构时才写入。
2. 不要把普通 Feature、Bug、Research 塞进此目录。
3. 重构文档应说明：当前问题、目标结构、迁移步骤、风险、回滚方式、验收标准。
```

### 4.5 `04-review/`

用途：阶段验收、代码审查结论、测试结果、发布前检查、质量评估。

性质：

```text
验收标准和阶段检查记录
PR 和 Issue 的 Verification 应引用这里
```

使用规则：

```text
1. 涉及验收、测试、质量评估时，应优先检查此目录。
2. 新的验收文档应放入 review。
3. PR 模板中的 Verification 应要求引用相关 review 文档或测试命令。
```

### 4.6 `05-bugfix/`

用途：缺陷记录、问题分析、临时问题归档、修复说明。

使用规则：

```text
1. 对明确 Bug 或运行异常，优先创建 GitHub Issue。
2. 如果已有问题记录，Codex 应读取后整理成 Issue 或 bugfix 文档。
3. 解决后的问题应在 Issue / PR 中闭环，不应只停留在问题文本里。
```

### 4.7 `06-knowledge/`

用途：长期知识、技术研究、外部资料整理、学习笔记、领域知识。

使用规则：

```text
1. 深度研究、技术背景、外部资料整理可放在此目录。
2. 如果研究结论已经变成正式架构规则，应同步到 architecture。
3. 如果研究结论只是背景，不应直接指导代码变更，需先转成 Issue 或 architecture 决策。
```

### 4.8 `docs/github-mcp-workflow.md`

用途：本文件，定义 GitHub Issues + Projects + docs + GitHub MCP Server 工作流。

使用规则：

```text
1. AI Agent 初始化项目流程前必须读取。
2. AI Agent 更新 Issue 模板、PR 模板、AGENTS.md、Project 规则时必须遵守。
3. 如果流程变化，应优先更新本文档。
```

---

## 5. 推荐仓库结构

通用结构：

```text
<project-root>/
├─ src/
├─ tests/
├─ docs/
│  ├─ 00-discussion/
│  ├─ 01-architecture/
│  ├─ 02-planning/
│  ├─ 03-refactoring/
│  ├─ 04-review/
│  ├─ 05-bugfix/
│  ├─ 06-knowledge/
│  └─ github-mcp-workflow.md
├─ .github/
│  ├─ ISSUE_TEMPLATE/
│  │  ├─ feature.yml
│  │  ├─ bug.yml
│  │  ├─ research.yml
│  │  ├─ experiment.yml
│  │  ├─ incident.yml
│  │  ├─ test.yml
│  │  ├─ docs.yml
│  │  └─ refactoring.yml
│  ├─ pull_request_template.md
│  └─ workflows/
└─ AGENTS.md
```

### 5.1 C++ 项目示例

```text
ModernCppLab/
├─ src/
├─ include/
├─ tests/
├─ examples/
├─ benchmarks/
├─ cmake/
├─ docs/
├─ .github/
├─ CMakeLists.txt
├─ CMakePresets.json
└─ AGENTS.md
```

### 5.2 C# / .NET 项目示例

```text
MyDotNetProject/
├─ src/
├─ tests/
├─ samples/
├─ docs/
├─ .github/
├─ MySolution.sln
├─ Directory.Build.props
└─ AGENTS.md
```

### 5.3 WPF 控件库项目示例

```text
KathMaticUI/
├─ src/
│  ├─ KathMaticUI.Controls/
│  ├─ KathMaticUI.Themes/
│  └─ KathMaticUI.Demo/
├─ tests/
├─ docs/
├─ .github/
└─ AGENTS.md
```

---

## 6. GitHub Labels 设计

### 6.1 类型标签

```text
type: feature
type: bug
type: research
type: experiment
type: incident
type: test
type: docs
type: tech-debt
type: refactoring
```

### 6.2 通用模块标签

```text
area: core
area: api
area: cli
area: ui
area: data
area: storage
area: config
area: logging
area: testing
area: build
area: ci
area: docs
area: performance
area: security
area: packaging
area: release
```

C++ 项目可补充：

```text
area: cmake
area: headers
area: library
area: examples
area: benchmarks
area: memory
area: concurrency
area: templates
area: abi
```

C# / .NET 项目可补充：

```text
area: dotnet
area: nuget
area: dependency-injection
area: hosting
area: serialization
area: async
```

WPF 项目可补充：

```text
area: controls
area: themes
area: resources
area: binding
area: behavior
area: validation
area: dpi
area: demo
```

### 6.3 风险标签

```text
risk: breaking-change
risk: data-loss
risk: performance
risk: security
risk: compatibility
risk: unstable
risk: migration
risk: api-change
```

### 6.4 优先级标签

```text
priority: p0
priority: p1
priority: p2
priority: p3
```

建议定义：

| Label | 含义 |
|---|---|
| `priority: p0` | 阻塞核心功能、构建、测试、发布，或可能造成严重风险 |
| `priority: p1` | 影响稳定性、核心体验或当前阶段目标 |
| `priority: p2` | 重要但不阻塞当前阶段 |
| `priority: p3` | 低优先级、整理、优化或后续增强 |

---

## 7. GitHub Project 设计

建议创建一个 GitHub Project：

```text
<ProjectName>
```

状态列：

```text
Inbox
Researching
Ready
Doing
Verifying
Blocked
Done
Archived
```

自定义字段：

```text
Type
Area
Priority
Risk
Impact
Next Action
Resume Note
Target Week
Doc Impact
Verification
```

最关键字段：

```text
Next Action
Resume Note
Doc Impact
Verification
```

字段说明：

| 字段 | 说明 |
|---|---|
| `Type` | Feature / Bug / Research / Experiment / Incident / Test / Docs / TechDebt / Refactoring |
| `Area` | 模块或关注点 |
| `Priority` | P0 / P1 / P2 / P3 |
| `Risk` | None / Low / Medium / High / Critical |
| `Impact` | None / Low / Medium / High / Critical |
| `Next Action` | 下次继续时第一步做什么 |
| `Resume Note` | 中断后如何恢复上下文 |
| `Target Week` | 计划处理周 |
| `Doc Impact` | None / Discussion / Architecture / Planning / Review / Bugfix / Knowledge |
| `Verification` | Not Started / In Progress / Passed / Failed / Not Required |

---

## 8. Issue 模板

AI Agent 应在 `.github/ISSUE_TEMPLATE/` 下生成以下模板。

### 8.1 `feature.yml`

```yaml
name: Feature
description: 新功能或能力建设
title: "[Feature] "
labels: ["type: feature"]
body:
  - type: textarea
    id: background
    attributes:
      label: 背景
      description: 为什么要做这个功能？
    validations:
      required: true

  - type: textarea
    id: goal
    attributes:
      label: 目标
      description: 完成后应该达到什么效果？
    validations:
      required: true

  - type: textarea
    id: scope
    attributes:
      label: 范围
      description: 包括什么？不包括什么？
      value: |
        包括：
        - 

        不包括：
        - 
    validations:
      required: true

  - type: textarea
    id: design
    attributes:
      label: 设计方案
      description: 准备如何实现？涉及哪些模块？
      value: |
        初步方案：
        - 

        影响模块：
        - 

  - type: textarea
    id: docs
    attributes:
      label: 相关文档
      value: |
        - `docs/01-architecture/...`
        - `docs/02-planning/...`
        - `docs/04-review/...`

  - type: textarea
    id: acceptance
    attributes:
      label: 验收标准
      value: |
        - [ ] 
        - [ ] 
        - [ ] 
    validations:
      required: true

  - type: textarea
    id: risks
    attributes:
      label: 风险
      value: |
        兼容性风险：
        性能风险：
        数据风险：
        安全风险：
        维护风险：

  - type: textarea
    id: files
    attributes:
      label: 关键文件
      value: |
        - `src/...`
        - `tests/...`
        - `docs/...`

  - type: textarea
    id: next-action
    attributes:
      label: 下一步
      description: 下次继续时先做什么？
    validations:
      required: true

  - type: textarea
    id: resume-note
    attributes:
      label: 恢复现场
      description: 下周或中断后回来时，先看哪里？
    validations:
      required: true
```

### 8.2 `bug.yml`

```yaml
name: Bug
description: 缺陷、异常行为、测试失败
title: "[Bug] "
labels: ["type: bug"]
body:
  - type: textarea
    id: symptom
    attributes:
      label: 现象
      description: 看到的异常是什么？
    validations:
      required: true

  - type: textarea
    id: expected
    attributes:
      label: 期望行为
      description: 正确行为应该是什么？
    validations:
      required: true

  - type: textarea
    id: reproduction
    attributes:
      label: 复现步骤
      value: |
        1. 
        2. 
        3. 

  - type: textarea
    id: environment
    attributes:
      label: 环境
      value: |
        OS：
        Compiler / Runtime：
        Build config：
        Version / Commit：

  - type: textarea
    id: impact
    attributes:
      label: 影响范围
      value: |
        影响模块：
        影响功能：
        影响稳定性：

  - type: textarea
    id: logs
    attributes:
      label: 日志 / 样例 / 证据
      description: 贴出相关日志、错误堆栈、测试失败信息或截图说明。

  - type: textarea
    id: docs
    attributes:
      label: 相关文档
      value: |
        - `docs/05-bugfix/...`
        - `docs/04-review/...`
        - `docs/01-architecture/...`

  - type: textarea
    id: suspected-cause
    attributes:
      label: 初步判断
      description: 可能是什么原因？

  - type: textarea
    id: acceptance
    attributes:
      label: 修复验收标准
      value: |
        - [ ] 
        - [ ] 
        - [ ] 

  - type: textarea
    id: next-action
    attributes:
      label: 下一步
    validations:
      required: true

  - type: textarea
    id: resume-note
    attributes:
      label: 恢复现场
    validations:
      required: true
```

### 8.3 其他模板

除 Feature 和 Bug 外，还应创建：

```text
research.yml       技术调研、方案设计、行为验证
experiment.yml     实验验证、PoC、压测、策略对比
incident.yml       运行事故、发布事故、严重异常复盘
test.yml           验收测试、稳定性测试、回归测试
docs.yml           文档新增、整理、同步、迁移
refactoring.yml    重构、模块迁移、结构调整
```

这些模板都必须包含：

```text
背景 / 目标
范围
相关文档
验收标准或输出物
下一步 Next Action
恢复现场 Resume Note
```

---

## 9. Pull Request 模板

AI Agent 应生成 `.github/pull_request_template.md`：

```markdown
## Related Issue

Closes #

## Summary

-

## Changes

-

## Affected Areas

- [ ] Core
- [ ] API
- [ ] CLI
- [ ] UI
- [ ] Data
- [ ] Storage
- [ ] Config
- [ ] Logging
- [ ] Testing
- [ ] Build / CI
- [ ] Docs
- [ ] Packaging / Release

## Documentation Read

- [ ] `docs/github-mcp-workflow.md`
- [ ] Related architecture docs:
      -
- [ ] Related planning docs:
      -
- [ ] Related review docs:
      -
- [ ] Related knowledge/discussion docs:
      -

## Verification

- [ ] Build
- [ ] Unit tests
- [ ] Integration tests
- [ ] Static analysis / lint
- [ ] Benchmark / performance check
- [ ] Manual verification
- [ ] Not required, reason:

## Safety / Compatibility

- [ ] No breaking API change
- [ ] No data migration required
- [ ] No performance regression expected
- [ ] No security-sensitive change
- [ ] Backward compatibility considered
- [ ] Not applicable, reason:

## Documentation Update

- [ ] 不需要更新文档
- [ ] 已更新 `docs/00-discussion/...`
- [ ] 已更新 `docs/01-architecture/...`
- [ ] 已更新 `docs/02-planning/...`
- [ ] 已更新 `docs/03-refactoring/...`
- [ ] 已更新 `docs/04-review/...`
- [ ] 已更新 `docs/05-bugfix/...`
- [ ] 已更新 `docs/06-knowledge/...`

## Resume Note

如果这个 PR 没合并，下次继续时先看：

-
```

---

## 10. AGENTS.md

AI Agent 应生成或补齐项目根目录下的 `AGENTS.md`。

```markdown
# AGENTS.md

## Project

This repository is a software engineering project managed with GitHub Issues, GitHub Projects, docs, Pull Requests, GitHub Actions and GitHub MCP Server.

## Core Goals

- Keep code, tests, docs, Issues and PRs synchronized.
- Prefer small, reviewable changes.
- Preserve existing architecture unless a refactoring Issue explicitly asks for change.
- Add or update tests where possible.
- Keep `Next Action` and `Resume Note` updated so work can resume after interruption.

## Documentation Map

Always start with:

- `docs/github-mcp-workflow.md`

Directory rules:

- `docs/00-discussion/`: historical discussion, design drafts and background context.
- `docs/01-architecture/`: formal architecture and long-lived design decisions.
- `docs/02-planning/`: current plans, staged tasks and execution order.
- `docs/03-refactoring/`: refactoring plans and migration designs.
- `docs/04-review/`: acceptance checks, review notes and validation results.
- `docs/05-bugfix/`: bug analysis and problem records.
- `docs/06-knowledge/`: research reports, technical knowledge and external references.

Use `docs/00-discussion/` as historical context only. If it conflicts with `docs/01-architecture/`, prefer `docs/01-architecture/`.

## Workflow Rules

Before making code changes:

1. Read the related GitHub Issue.
2. Read `docs/github-mcp-workflow.md`.
3. Read relevant architecture, planning, review and knowledge docs.
4. Inspect the current code structure.
5. Identify affected source and test files.
6. Explain the change plan briefly.
7. Update or add tests where possible.

After making code changes:

1. Run relevant build and tests.
2. Update or create documentation if a design rule changed.
3. Update the related Issue with:
   - what changed
   - what remains
   - next action
   - resume note
   - files touched
   - tests run
4. Open or update the related PR.

## Issue Update Format

Append this section to related Issues:

### Progress Update

Done:
- 

Remaining:
- 

Next Action:
- 

Resume Note:
- 

Files touched:
- 

Tests:
- 

Docs updated:
- 

## Safety Rules

- Do not remove error handling, validation, tests, or recovery logic without replacing it with an equivalent or safer mechanism.
- Do not change public APIs without documenting compatibility impact.
- Do not change data schemas or file formats without documenting migration impact.
- Do not silently ignore errors.
- Do not mark work complete unless verification is explicit.
- Prefer deterministic tests and reproducible examples.
- Keep docs synchronized when behavior, architecture, or acceptance criteria change.
```

### 10.1 C++ 项目可追加到 AGENTS.md

```markdown
## C++ Project Rules

- Prefer modern C++ practices consistent with the project's configured standard.
- Do not introduce raw owning pointers unless explicitly justified.
- Prefer RAII for resource management.
- Keep headers minimal and avoid unnecessary includes.
- Preserve ABI/API compatibility if this is a library project.
- Update CMake files when new source, test, or example files are added.
- Run or document relevant build commands, for example:
  - `cmake --preset <preset>`
  - `cmake --build --preset <preset>`
  - `ctest --preset <preset>`
```

### 10.2 C# / .NET 项目可追加到 AGENTS.md

```markdown
## .NET Project Rules

- Preserve solution and project structure.
- Update `.csproj`, solution files or package references only when required.
- Prefer async APIs only when there is real asynchronous work.
- Avoid breaking public contracts without documenting migration impact.
- Run or document relevant commands:
  - `dotnet restore`
  - `dotnet build`
  - `dotnet test`
```

### 10.3 WPF 项目可追加到 AGENTS.md

```markdown
## WPF Project Rules

- Preserve MVVM boundaries.
- Avoid code-behind unless explicitly justified.
- Keep resource dictionaries and theme resources organized.
- Consider DPI, theme, binding and design-time behavior.
- Add demo coverage when creating or changing reusable controls.
```

---

## 11. GitHub MCP Server 工作流

### 11.1 Agent 能力目标

接入 GitHub MCP Server 后，AI Agent 应能够执行：

```text
读取当前 Issue
读取相关 docs/*.md
读取相关源码和测试
分析当前进度
生成或修改代码
生成或修改测试
创建 PR
读取 Actions 结果
更新 Issue 进展
补充 Resume Note
```

### 11.2 推荐 Agent 工作流

每次让 AI Agent 执行任务时，使用以下流程：

```text
1. 指定一个 GitHub Issue
2. 要求 AI Agent 读取 Issue 内容
3. 要求 AI Agent 读取 docs/github-mcp-workflow.md 和相关 docs/*.md
4. 要求 AI Agent 检查现有代码结构
5. 要求 AI Agent 输出简短实施计划
6. 要求 AI Agent 修改代码和测试
7. 要求 AI Agent 运行测试
8. 要求 AI Agent 创建或更新 PR
9. 要求 AI Agent 更新 Issue 的 Progress Update
10. 要求 AI Agent 写清楚 Resume Note
```

### 11.3 给 AI Agent 的标准任务提示词

```text
请基于当前仓库、GitHub Issue 和 docs/github-mcp-workflow.md 打通本项目的 GitHub Issues + Projects + docs + GitHub MCP 工作流。

执行要求：

1. 阅读 docs/github-mcp-workflow.md。
2. 检查当前仓库结构。
3. 保留现有 docs 目录结构。
4. 不要生成一套新的、与现有 docs 并行的编号文档体系。
5. 如果缺少以下内容，请创建：
   - .github/ISSUE_TEMPLATE/*.yml
   - .github/pull_request_template.md
   - AGENTS.md
6. 如果已有同名文件，请在不破坏现有内容的前提下合并。
7. 根据本项目实际语言、框架和目录，调整模板里的 src/tests 路径。
8. 不要生成与项目无关的重型流程。
9. 保留重点字段：
   - Next Action
   - Resume Note
   - Safety / Compatibility
   - Verification
   - Documentation Update
10. 输出变更摘要。
11. 如果可以，创建一个 PR。
12. PR 描述中说明：
    - 新增了哪些流程文件
    - 这些文件如何配合 GitHub MCP Server 使用
    - 下一步需要人工在 GitHub Project 中配置哪些字段
```

---

## 12. 第一批 Issue 建议

可以先创建这些 Issue：

```text
[Docs] 初始化 GitHub Issues + Projects + docs + MCP 工作流
[Docs] 校准当前计划与 GitHub Project
[Docs] 补齐 AGENTS.md
[Docs] 建立项目文档目录职责说明
[Feature] 建立最小可运行项目骨架
[Test] 建立基础构建和测试流程
[Test] 配置 CI 构建与测试
[Research] 梳理当前项目架构与关键技术选择
[Refactoring] 整理当前模块边界与目录结构
[Bug] 整理现有问题记录并拆分为 GitHub Issues
```

---

## 13. GitHub CLI 辅助命令

执行前确认当前目录已绑定正确 GitHub 仓库。

```bash
gh label create "type: feature" --description "New feature or capability" || true
gh label create "type: bug" --description "Bug or incorrect behavior" || true
gh label create "type: research" --description "Research or design validation" || true
gh label create "type: experiment" --description "Experiment, PoC or benchmark" || true
gh label create "type: incident" --description "Runtime incident or postmortem" || true
gh label create "type: test" --description "Acceptance, regression or stability test" || true
gh label create "type: docs" --description "Documentation task" || true
gh label create "type: tech-debt" --description "Technical debt" || true
gh label create "type: refactoring" --description "Refactoring or structural change" || true

gh label create "area: core" --description "Core functionality" || true
gh label create "area: api" --description "Public or internal API" || true
gh label create "area: cli" --description "Command line interface" || true
gh label create "area: ui" --description "User interface" || true
gh label create "area: data" --description "Data model or processing" || true
gh label create "area: storage" --description "Storage layer" || true
gh label create "area: config" --description "Configuration" || true
gh label create "area: logging" --description "Logging" || true
gh label create "area: testing" --description "Testing" || true
gh label create "area: build" --description "Build system" || true
gh label create "area: ci" --description "Continuous integration" || true
gh label create "area: docs" --description "Project documentation" || true
gh label create "area: performance" --description "Performance" || true
gh label create "area: security" --description "Security" || true
gh label create "area: packaging" --description "Packaging" || true
gh label create "area: release" --description "Release" || true

gh label create "risk: breaking-change" --description "Breaking change risk" || true
gh label create "risk: data-loss" --description "Potential data loss risk" || true
gh label create "risk: performance" --description "Performance risk" || true
gh label create "risk: security" --description "Security risk" || true
gh label create "risk: compatibility" --description "Compatibility risk" || true
gh label create "risk: unstable" --description "Stability risk" || true
gh label create "risk: migration" --description "Migration risk" || true
gh label create "risk: api-change" --description "API change risk" || true

gh label create "priority: p0" --description "Critical priority" || true
gh label create "priority: p1" --description "High priority" || true
gh label create "priority: p2" --description "Medium priority" || true
gh label create "priority: p3" --description "Low priority" || true
```

---

## 14. 人工仍需配置的内容

AI Agent 可以生成仓库文件，但以下内容通常需要人工在 GitHub UI 中配置：

```text
GitHub Project
Project 字段
Project 视图
MCP 客户端授权
GitHub MCP Server 权限
GitHub Actions secrets
Repository branch protection
Required checks
```

建议人工配置：

```text
Project name: <ProjectName>

Views:
  Board by Status
  Table by Priority
  Table by Area
  Current Week
  Blocked Items
  Verification

Fields:
  Status
  Type
  Area
  Priority
  Risk
  Impact
  Next Action
  Resume Note
  Target Week
  Doc Impact
  Verification
```

权限建议：

```text
GitHub MCP Server 先只授权当前 repo。
不要一开始给组织级管理权限。
不要给不必要的 secret/package/delete 权限。
```

---

## 15. 完成标准

本流程打通后，应满足：

```text
1. 新任务可以通过 Issue 创建。
2. 每个 Issue 都包含 Next Action 和 Resume Note。
3. GitHub Project 可以看到所有任务状态。
4. docs 目录结构清晰，没有生成重复的并行文档体系。
5. PR 模板要求说明 Documentation Read、Verification、Safety / Compatibility。
6. AGENTS.md 规定了 AI Agent 的执行流程。
7. AI Agent 可以根据 Issue + docs + 代码执行任务。
8. AI Agent 修改后可以更新 PR 和 Issue。
9. 中断一周后，可以通过 Issue 的 Resume Note 恢复现场。
```

---

## 16. 建议的第一步执行指令

把本文件放入仓库后，向 AI Agent 发送：

```text
请阅读 docs/github-mcp-workflow.md，并结合当前仓库代码，完成 GitHub Issues + Projects + docs + GitHub MCP 工作流的初始化。

要求：

1. 创建或补齐 .github/ISSUE_TEMPLATE 下的模板。
2. 创建或补齐 .github/pull_request_template.md。
3. 创建或补齐 AGENTS.md。
4. 保留当前 docs 目录结构。
5. 不要生成一套新的、与现有 docs 并行的编号文档体系。
6. 根据当前项目实际目录调整模板中的 src/tests 路径。
7. 如果是 C++ 项目，检查 CMake、include、src、tests、examples、benchmarks 等目录是否需要在 AGENTS.md 中说明。
8. 如果是 .NET 项目，检查 solution、src、tests、samples、NuGet 相关规则是否需要在 AGENTS.md 中说明。
9. 不要删除现有文档和配置；如有冲突，以合并为主。
10. 完成后输出变更清单。
11. 给出需要我手动在 GitHub Project UI 配置的字段和视图。
```

---

## 17. 后续使用规则

每次开发新能力时：

```text
1. 先建 Issue
2. 写清楚目标、验收标准、Next Action、Resume Note
3. 放入 GitHub Project
4. 让 AI Agent 读取 Issue + docs + 代码
5. 由 AI Agent 修改代码和测试
6. 通过 PR 合并
7. 更新 docs 和 Issue
```

每周结束时：

```text
1. 检查 Doing / Blocked / Verifying
2. 补全每个未完成 Issue 的 Resume Note
3. 把下周要做的 Issue 移到 Ready 或 Doing
4. 更新 planning 文档
5. 关闭已经完成且文档同步的 Issue
```

每次出现异常时：

```text
1. 创建 Bug 或 Incident Issue
2. 记录时间范围、影响范围、风险
3. 判断是否需要回滚、迁移或补救
4. 生成后续改进任务
5. 更新 bugfix 或 review 文档
6. 如果形成长期规则，同步更新 architecture 文档
```

每次产生研究结论时：

```text
1. 先记录到 Research Issue
2. 如是背景材料，整理到 knowledge 文档
3. 如是正式设计规则，整理到 architecture 文档
4. 如影响当前执行计划，更新 planning 文档
5. 如影响验收标准，更新 review 文档
```

---

## 18. 项目类型适配建议

### 18.1 C++ 项目

重点关注：

```text
CMake
头文件组织
库边界
ABI / API 兼容
RAII
内存管理
异常安全
并发
测试
benchmark
安装和打包
```

建议 CI 至少包含：

```text
cmake configure
cmake build
ctest
clang-format / clang-tidy
```

### 18.2 C# / .NET 项目

重点关注：

```text
solution 结构
项目引用
NuGet 包
依赖注入
配置
日志
异步
测试
发布
```

建议 CI 至少包含：

```text
dotnet restore
dotnet build
dotnet test
```

### 18.3 WPF 项目

重点关注：

```text
控件边界
主题资源
明暗主题
DPI
绑定
行为
校验
Demo App
视觉一致性
```

Issue 和 PR 必须说明：

```text
是否影响主题
是否影响 DPI
是否影响绑定
是否需要 Demo
是否需要截图或人工验收
```

### 18.4 CLI 工具

重点关注：

```text
命令设计
参数兼容
退出码
配置文件
日志输出
错误提示
脚本集成
发布包
```

### 18.5 Web API / 后端服务

重点关注：

```text
接口契约
认证授权
数据库迁移
缓存
幂等性
日志
监控
部署
兼容性
```

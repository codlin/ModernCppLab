# AGENTS.md

## Project

This repository is a C++17 software engineering project managed with GitHub Issues, GitHub Projects, docs, Pull Requests, GitHub Actions and GitHub MCP Server.

Build system: CMake 3.20+, Ninja generator, CMakePresets.json.
See also `CLAUDE.md` for build commands and architecture overview.

## Core Goals

- Keep code, tests, docs, Issues and PRs synchronized.
- Prefer small, reviewable changes.
- Preserve existing architecture unless a refactoring Issue explicitly asks for change.
- Add or update tests where possible.
- Keep `Next Action` and `Resume Note` updated so work can resume after interruption.

## Documentation Map

Always start with:

- `docs/github-mcp-workflow.md`
- `CLAUDE.md`

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
5. Identify affected source (`src/`, `include/`) and test files (`tests/`).
6. Explain the change plan briefly.
7. Update or add tests where possible.

After making code changes:

1. Run build: `cmake --preset default && cmake --build cmake-build-default`
2. Run tests if available: `ctest --test-dir cmake-build-default`
3. Update or create documentation if a design rule changed.
4. Update the related Issue with:
   - what changed
   - what remains
   - next action
   - resume note
   - files touched
   - tests run
5. Open or update the related PR.

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
- Do not change public APIs (public headers in `include/`) without documenting compatibility impact.
- Do not change data schemas or file formats (`data/tasks.json`) without documenting migration impact.
- Do not silently ignore errors.
- Do not mark work complete unless verification is explicit.
- Prefer deterministic tests and reproducible examples.
- Keep docs synchronized when behavior, architecture, or acceptance criteria change.

## C++ Project Rules

- C++17 standard, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Header/source layout: public headers in `include/`, implementation in `src/`. One public class per header/source pair.
- Prefer RAII for resource management. Do not introduce raw owning pointers unless explicitly justified.
- Keep headers minimal: `#pragma once`, avoid unnecessary includes, forward-declare where possible.
- Const-qualified accessors, no `get` prefix on getters.
- When adding or removing source files, update `CMakeLists.txt` (`add_executable`).
- Build commands:
  - Configure: `cmake --preset default`
  - Build: `cmake --build cmake-build-default`
  - Run: `./cmake-build-default/ModernCppLab`
- The project currently has no test framework. If tests are added, wire them in CMake with `enable_testing()` and `add_test()`, then run via `ctest --test-dir cmake-build-default`.
- JSON persistence is hand-rolled in `JsonTaskStorage` (no third-party JSON library). Any change to the serialization format must maintain backward compatibility or include a migration path.
- Platform code: `localtime_s` on Windows, `localtime_r` on POSIX (currently duplicated in `ConsoleApp.cpp` and `JsonTaskStorage.cpp` — extract to a shared utility if adding a third usage).

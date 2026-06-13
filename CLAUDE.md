# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (Ninja generator, Debug)
cmake --preset default

# Build
cmake --build cmake-build-default

# Run
./cmake-build-default/ModernCppLab
```

There are no tests or linting configured yet.

## Architecture

This is a C++17 console app for managing measurement tasks — a learning project practicing modern C++ features.

**Domain layer** (`include/` + `src/`, one class per file):
- `MeasurementStatus` — `enum class` with `ToString()`/`FromString()` helpers using `std::optional`
- `MeasurementResult` — immutable value object holding a `double` value and `std::string` unit
- `MeasurementTask` — aggregate with `int64_t` id, name, status, `std::chrono::system_clock::time_point` createdTime, and `std::optional<MeasurementResult>`; all fields are private with const accessors and mutation methods

**Repository** (`TaskRepository`): In-memory `std::vector<MeasurementTask>` with auto-incrementing IDs (max existing id + 1). Provides `Add`, `Remove`, `FindById`, `GetAll`, `ReplaceAll`, `Clear`.

**Persistence** (`JsonTaskStorage`): Hand-rolled JSON serialization — no third-party JSON library. Writes a JSON array to file and reads it back using `std::regex` to extract fields from each object. Handles JSON string escaping (`\\`, `\"`, `\n`, `\r`, `\t`). Time values are formatted/parsed as `"%Y-%m-%d %H:%M:%S"` in local time. The `localtime` call uses `localtime_s` on Windows and `localtime_r` on POSIX (duplicated in both `ConsoleApp.cpp` and `JsonTaskStorage.cpp` as anonymous-namespace helpers).

**UI** (`ConsoleApp`): Command-line menu loop that wires `TaskRepository` and `JsonTaskStorage` together. Loads tasks from disk on startup and at the user's request; saves on exit and on demand. Default storage path is `data/tasks.json`.

**Entry point** (`src/main.cpp`): Creates `ConsoleApp("data/tasks.json")` and calls `Run()`, catching exceptions for a fatal-error message.

## Key Conventions

- C++17, no extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- One public class per header/source pair
- Headers use `#pragma once`
- Private data members with `const`-qualified accessors (no getter prefix)
- Mutation methods use explicit names (`Rename`, `ChangeStatus`, `SetResult`, `ClearResult`)
- No third-party dependencies — standard library only

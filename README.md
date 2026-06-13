# ModernCppLab

A small C++17 console project for practicing modern C++ basics.

## Features

- Add measurement task
- List tasks
- Change task status
- Set measurement result
- Remove task
- Save/load tasks from JSON-like file

## Practice Points

- `std::vector`
- `std::string`
- `std::optional`
- `enum class`
- `std::filesystem`
- `std::chrono`
- RAII-oriented class design
- One public class per header/source file

## Open in CLion

1. Open the `ModernCppLab` folder in CLion.
2. Let CLion load `CMakeLists.txt`.
3. Select the `ModernCppLab` target.
4. Build and run.

## Command line build

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
./cmake-build-debug/ModernCppLab
```

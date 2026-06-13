cmake_minimum_required(VERSION 3.20)
project(ModernCppLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ModernCppLab
    src/main.cpp
    src/MeasurementStatus.cpp
    src/MeasurementResult.cpp
    src/MeasurementTask.cpp
    src/TaskRepository.cpp
    src/JsonTaskStorage.cpp
    src/ConsoleApp.cpp
)

target_include_directories(ModernCppLab PRIVATE include)

#pragma once

#include "JsonTaskStorage.h"
#include "TaskRepository.h"

#include <filesystem>
#include <string>

class ConsoleApp {
public:
    explicit ConsoleApp(std::filesystem::path storageFilePath);

    void Run();

private:
    void PrintMenu() const;
    void AddTask();
    void ListTasks() const;
    void ChangeTaskStatus();
    void SetTaskResult();
    void RemoveTask();
    void SaveTasks() const;
    void LoadTasks();

    static std::string ReadLine(const std::string& prompt);
    static std::int64_t ReadInt64(const std::string& prompt);
    static double ReadDouble(const std::string& prompt);
    static MeasurementStatus ReadStatus();
    static std::string FormatTaskTime(std::chrono::system_clock::time_point timePoint);

    std::filesystem::path storageFilePath_;
    TaskRepository repository_;
    JsonTaskStorage storage_;
};

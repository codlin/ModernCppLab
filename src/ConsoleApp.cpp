#include "ConsoleApp.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
#ifdef _WIN32
std::tm LocalTime(std::time_t value) {
    std::tm result{};
    localtime_s(&result, &value);
    return result;
}
#else
std::tm LocalTime(std::time_t value) {
    std::tm result{};
    localtime_r(&value, &result);
    return result;
}
#endif
}

ConsoleApp::ConsoleApp(std::filesystem::path storageFilePath)
    : storageFilePath_(std::move(storageFilePath)) {
}

void ConsoleApp::Run() {
    LoadTasks();

    while (true) {
        PrintMenu();

        auto command = ReadLine("Select: ");

        try {
            if (command == "1") {
                AddTask();
            } else if (command == "2") {
                ListTasks();
            } else if (command == "3") {
                ChangeTaskStatus();
            } else if (command == "4") {
                SetTaskResult();
            } else if (command == "5") {
                RemoveTask();
            } else if (command == "6") {
                SaveTasks();
            } else if (command == "7") {
                LoadTasks();
            } else if (command == "0") {
                SaveTasks();
                break;
            } else {
                std::cout << "Unknown command.\n";
            }
        } catch (const std::exception& ex) {
            std::cout << "Error: " << ex.what() << "\n";
        }
    }
}

void ConsoleApp::PrintMenu() const {
    std::cout << "\n";
    std::cout << "==== ModernCppLab ====" << "\n";
    std::cout << "1. Add task" << "\n";
    std::cout << "2. List tasks" << "\n";
    std::cout << "3. Change task status" << "\n";
    std::cout << "4. Set task result" << "\n";
    std::cout << "5. Remove task" << "\n";
    std::cout << "6. Save tasks" << "\n";
    std::cout << "7. Load tasks" << "\n";
    std::cout << "0. Save and exit" << "\n";
}

void ConsoleApp::AddTask() {
    auto name = ReadLine("Task name: ");
    auto& task = repository_.Add(std::move(name));

    std::cout << "Added task. id=" << task.Id() << "\n";
}

void ConsoleApp::ListTasks() const {
    const auto& tasks = repository_.GetAll();

    if (tasks.empty()) {
        std::cout << "No tasks.\n";
        return;
    }

    std::cout << "\n";
    std::cout << "ID | Status | Created Time        | Name | Result\n";
    std::cout << "---+--------+---------------------+------+--------\n";

    for (const auto& task : tasks) {
        std::cout << task.Id() << " | ";
        std::cout << ToString(task.Status()) << " | ";
        std::cout << FormatTaskTime(task.CreatedTime()) << " | ";
        std::cout << task.Name() << " | ";

        if (task.Result().has_value()) {
            std::cout << task.Result()->Value() << " " << task.Result()->Unit();
        } else {
            std::cout << "-";
        }

        std::cout << "\n";
    }
}

void ConsoleApp::ChangeTaskStatus() {
    auto id = ReadInt64("Task id: ");
    auto* task = repository_.FindById(id);

    if (task == nullptr) {
        std::cout << "Task not found.\n";
        return;
    }

    auto status = ReadStatus();
    task->ChangeStatus(status);

    std::cout << "Status changed.\n";
}

void ConsoleApp::SetTaskResult() {
    auto id = ReadInt64("Task id: ");
    auto* task = repository_.FindById(id);

    if (task == nullptr) {
        std::cout << "Task not found.\n";
        return;
    }

    auto value = ReadDouble("Result value: ");
    auto unit = ReadLine("Result unit: ");

    task->SetResult(MeasurementResult(value, std::move(unit)));
    task->ChangeStatus(MeasurementStatus::Completed);

    std::cout << "Result set.\n";
}

void ConsoleApp::RemoveTask() {
    auto id = ReadInt64("Task id: ");

    if (repository_.Remove(id)) {
        std::cout << "Task removed.\n";
    } else {
        std::cout << "Task not found.\n";
    }
}

void ConsoleApp::SaveTasks() const {
    storage_.Save(storageFilePath_, repository_.GetAll());
    std::cout << "Saved to " << storageFilePath_ << "\n";
}

void ConsoleApp::LoadTasks() {
    auto tasks = storage_.Load(storageFilePath_);
    repository_.ReplaceAll(std::move(tasks));
    std::cout << "Loaded " << repository_.GetAll().size() << " task(s).\n";
}

std::string ConsoleApp::ReadLine(const std::string& prompt) {
    std::cout << prompt;

    std::string value;
    std::getline(std::cin, value);
    return value;
}

std::int64_t ConsoleApp::ReadInt64(const std::string& prompt) {
    while (true) {
        auto text = ReadLine(prompt);

        try {
            return std::stoll(text);
        } catch (...) {
            std::cout << "Invalid number.\n";
        }
    }
}

double ConsoleApp::ReadDouble(const std::string& prompt) {
    while (true) {
        auto text = ReadLine(prompt);

        try {
            return std::stod(text);
        } catch (...) {
            std::cout << "Invalid number.\n";
        }
    }
}

MeasurementStatus ConsoleApp::ReadStatus() {
    std::cout << "Status options:\n";
    std::cout << "1. Pending\n";
    std::cout << "2. Running\n";
    std::cout << "3. Completed\n";
    std::cout << "4. Failed\n";

    while (true) {
        auto value = ReadLine("Status: ");

        if (value == "1") {
            return MeasurementStatus::Pending;
        }
        if (value == "2") {
            return MeasurementStatus::Running;
        }
        if (value == "3") {
            return MeasurementStatus::Completed;
        }
        if (value == "4") {
            return MeasurementStatus::Failed;
        }

        std::cout << "Invalid status.\n";
    }
}

std::string ConsoleApp::FormatTaskTime(std::chrono::system_clock::time_point timePoint) {
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    auto localTime = LocalTime(time);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

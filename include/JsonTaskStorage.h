#pragma once

#include "MeasurementTask.h"

#include <filesystem>
#include <vector>

class JsonTaskStorage {
public:
    void Save(const std::filesystem::path& filePath, const std::vector<MeasurementTask>& tasks) const;
    std::vector<MeasurementTask> Load(const std::filesystem::path& filePath) const;

private:
    static std::string EscapeJsonString(const std::string& value);
    static std::string UnescapeJsonString(const std::string& value);
    static std::string FormatTime(std::chrono::system_clock::time_point timePoint);
    static std::chrono::system_clock::time_point ParseTime(const std::string& value);
};

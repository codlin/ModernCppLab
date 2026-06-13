#include "JsonTaskStorage.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

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

std::string ReadWholeFile(const std::filesystem::path& filePath) {
    std::ifstream input(filePath);
    if (!input) {
        throw std::runtime_error("Failed to open file for reading: " + filePath.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::string> RegexFirstGroup(const std::string& text, const std::regex& pattern) {
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return std::nullopt;
    }

    if (match.size() < 2) {
        return std::nullopt;
    }

    return match[1].str();
}
}

void JsonTaskStorage::Save(const std::filesystem::path& filePath, const std::vector<MeasurementTask>& tasks) const {
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream output(filePath);
    if (!output) {
        throw std::runtime_error("Failed to open file for writing: " + filePath.string());
    }

    output << "[\n";

    for (std::size_t index = 0; index < tasks.size(); ++index) {
        const auto& task = tasks[index];

        output << "  {";
        output << "\"id\":" << task.Id() << ",";
        output << "\"name\":\"" << EscapeJsonString(task.Name()) << "\",";
        output << "\"status\":\"" << ToString(task.Status()) << "\",";
        output << "\"createdTime\":\"" << FormatTime(task.CreatedTime()) << "\",";

        if (task.Result().has_value()) {
            output << "\"result\":{";
            output << "\"value\":" << task.Result()->Value() << ",";
            output << "\"unit\":\"" << EscapeJsonString(task.Result()->Unit()) << "\"";
            output << "}";
        } else {
            output << "\"result\":null";
        }

        output << "}";

        if (index + 1 < tasks.size()) {
            output << ",";
        }

        output << "\n";
    }

    output << "]\n";
}

std::vector<MeasurementTask> JsonTaskStorage::Load(const std::filesystem::path& filePath) const {
    if (!std::filesystem::exists(filePath)) {
        return {};
    }

    auto text = ReadWholeFile(filePath);
    std::vector<MeasurementTask> tasks;

    std::regex objectPattern(R"(\{\"id\":\d+.*?\})");
    auto begin = std::sregex_iterator(text.begin(), text.end(), objectPattern);
    auto end = std::sregex_iterator();

    for (auto iterator = begin; iterator != end; ++iterator) {
        auto objectText = iterator->str();

        auto idText = RegexFirstGroup(objectText, std::regex(R"(\"id\":(\d+))"));
        auto nameText = RegexFirstGroup(objectText, std::regex(R"(\"name\":\"((?:\\.|[^\"])*)\")"));
        auto statusText = RegexFirstGroup(objectText, std::regex(R"(\"status\":\"([^\"]+)\")"));
        auto createdTimeText = RegexFirstGroup(objectText, std::regex(R"(\"createdTime\":\"([^\"]+)\")"));

        if (!idText || !nameText || !statusText || !createdTimeText) {
            continue;
        }

        auto status = MeasurementStatusFromString(*statusText);
        if (!status) {
            continue;
        }

        std::optional<MeasurementResult> result = std::nullopt;

        auto valueText = RegexFirstGroup(objectText, std::regex(R"(\"value\":(-?\d+(?:\.\d+)?))"));
        auto unitText = RegexFirstGroup(objectText, std::regex(R"(\"unit\":\"((?:\\.|[^\"])*)\")"));

        if (valueText && unitText) {
            result = MeasurementResult(std::stod(*valueText), UnescapeJsonString(*unitText));
        }

        tasks.emplace_back(
            std::stoll(*idText),
            UnescapeJsonString(*nameText),
            *status,
            ParseTime(*createdTimeText),
            std::move(result));
    }

    return tasks;
}

std::string JsonTaskStorage::EscapeJsonString(const std::string& value) {
    std::string result;

    for (char ch : value) {
        switch (ch) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += ch;
                break;
        }
    }

    return result;
}

std::string JsonTaskStorage::UnescapeJsonString(const std::string& value) {
    std::string result;
    bool escaping = false;

    for (char ch : value) {
        if (!escaping) {
            if (ch == '\\') {
                escaping = true;
            } else {
                result += ch;
            }
            continue;
        }

        switch (ch) {
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            default:
                result += ch;
                break;
        }

        escaping = false;
    }

    return result;
}

std::string JsonTaskStorage::FormatTime(std::chrono::system_clock::time_point timePoint) {
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    auto localTime = LocalTime(time);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::chrono::system_clock::time_point JsonTaskStorage::ParseTime(const std::string& value) {
    std::tm time{};
    std::istringstream input(value);
    input >> std::get_time(&time, "%Y-%m-%d %H:%M:%S");

    if (input.fail()) {
        return std::chrono::system_clock::now();
    }

    auto timeValue = std::mktime(&time);
    return std::chrono::system_clock::from_time_t(timeValue);
}

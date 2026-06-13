#pragma once

#include <string>
#include <optional>

enum class MeasurementStatus {
    Pending,
    Running,
    Completed,
    Failed
};

std::string ToString(MeasurementStatus status);
std::optional<MeasurementStatus> MeasurementStatusFromString(const std::string& value);

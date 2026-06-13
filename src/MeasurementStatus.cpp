#include "MeasurementStatus.h"

std::string ToString(MeasurementStatus status) {
    switch (status) {
        case MeasurementStatus::Pending:
            return "Pending";
        case MeasurementStatus::Running:
            return "Running";
        case MeasurementStatus::Completed:
            return "Completed";
        case MeasurementStatus::Failed:
            return "Failed";
    }

    return "Unknown";
}

std::optional<MeasurementStatus> MeasurementStatusFromString(const std::string& value) {
    if (value == "Pending") {
        return MeasurementStatus::Pending;
    }
    if (value == "Running") {
        return MeasurementStatus::Running;
    }
    if (value == "Completed") {
        return MeasurementStatus::Completed;
    }
    if (value == "Failed") {
        return MeasurementStatus::Failed;
    }

    return std::nullopt;
}

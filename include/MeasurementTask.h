#pragma once

#include "MeasurementResult.h"
#include "MeasurementStatus.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

class MeasurementTask {
public:
    MeasurementTask(
        std::int64_t id,
        std::string name,
        MeasurementStatus status,
        std::chrono::system_clock::time_point createdTime,
        std::optional<MeasurementResult> result);

    static MeasurementTask CreateNew(std::int64_t id, std::string name);

    std::int64_t Id() const;
    const std::string& Name() const;
    MeasurementStatus Status() const;
    std::chrono::system_clock::time_point CreatedTime() const;
    const std::optional<MeasurementResult>& Result() const;

    void Rename(std::string name);
    void ChangeStatus(MeasurementStatus status);
    void SetResult(MeasurementResult result);
    void ClearResult();

private:
    std::int64_t id_;
    std::string name_;
    MeasurementStatus status_;
    std::chrono::system_clock::time_point createdTime_;
    std::optional<MeasurementResult> result_;
};

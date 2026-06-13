#include "MeasurementTask.h"

#include <utility>

MeasurementTask::MeasurementTask(
    std::int64_t id,
    std::string name,
    MeasurementStatus status,
    std::chrono::system_clock::time_point createdTime,
    std::optional<MeasurementResult> result)
    : id_(id),
      name_(std::move(name)),
      status_(status),
      createdTime_(createdTime),
      result_(std::move(result)) {
}

MeasurementTask MeasurementTask::CreateNew(std::int64_t id, std::string name) {
    return MeasurementTask(
        id,
        std::move(name),
        MeasurementStatus::Pending,
        std::chrono::system_clock::now(),
        std::nullopt);
}

std::int64_t MeasurementTask::Id() const {
    return id_;
}

const std::string& MeasurementTask::Name() const {
    return name_;
}

MeasurementStatus MeasurementTask::Status() const {
    return status_;
}

std::chrono::system_clock::time_point MeasurementTask::CreatedTime() const {
    return createdTime_;
}

const std::optional<MeasurementResult>& MeasurementTask::Result() const {
    return result_;
}

void MeasurementTask::Rename(std::string name) {
    name_ = std::move(name);
}

void MeasurementTask::ChangeStatus(MeasurementStatus status) {
    status_ = status;
}

void MeasurementTask::SetResult(MeasurementResult result) {
    result_ = std::move(result);
}

void MeasurementTask::ClearResult() {
    result_.reset();
}

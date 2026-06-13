#pragma once

#include "MeasurementTask.h"

#include <cstdint>
#include <optional>
#include <vector>

class TaskRepository {
public:
    MeasurementTask& Add(std::string name);

    bool Remove(std::int64_t id);
    MeasurementTask* FindById(std::int64_t id);
    const MeasurementTask* FindById(std::int64_t id) const;

    const std::vector<MeasurementTask>& GetAll() const;
    void ReplaceAll(std::vector<MeasurementTask> tasks);
    void Clear();

private:
    std::int64_t NextId() const;

    std::vector<MeasurementTask> tasks_;
};

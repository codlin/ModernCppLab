#include "TaskRepository.h"

#include <algorithm>
#include <utility>

MeasurementTask& TaskRepository::Add(std::string name) {
    auto id = NextId();
    tasks_.push_back(MeasurementTask::CreateNew(id, std::move(name)));
    return tasks_.back();
}

bool TaskRepository::Remove(std::int64_t id) {
    auto oldSize = tasks_.size();

    tasks_.erase(
        std::remove_if(
            tasks_.begin(),
            tasks_.end(),
            [id](const MeasurementTask& task) {
                return task.Id() == id;
            }),
        tasks_.end());

    return tasks_.size() != oldSize;
}

MeasurementTask* TaskRepository::FindById(std::int64_t id) {
    auto iterator = std::find_if(
        tasks_.begin(),
        tasks_.end(),
        [id](const MeasurementTask& task) {
            return task.Id() == id;
        });

    if (iterator == tasks_.end()) {
        return nullptr;
    }

    return &(*iterator);
}

const MeasurementTask* TaskRepository::FindById(std::int64_t id) const {
    auto iterator = std::find_if(
        tasks_.begin(),
        tasks_.end(),
        [id](const MeasurementTask& task) {
            return task.Id() == id;
        });

    if (iterator == tasks_.end()) {
        return nullptr;
    }

    return &(*iterator);
}

const std::vector<MeasurementTask>& TaskRepository::GetAll() const {
    return tasks_;
}

void TaskRepository::ReplaceAll(std::vector<MeasurementTask> tasks) {
    tasks_ = std::move(tasks);
}

void TaskRepository::Clear() {
    tasks_.clear();
}

std::int64_t TaskRepository::NextId() const {
    std::int64_t maxId = 0;

    for (const auto& task : tasks_) {
        if (task.Id() > maxId) {
            maxId = task.Id();
        }
    }

    return maxId + 1;
}

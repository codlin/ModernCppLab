#include "MeasurementResult.h"

#include <utility>

MeasurementResult::MeasurementResult(double value, std::string unit)
    : value_(value), unit_(std::move(unit)) {
}

double MeasurementResult::Value() const {
    return value_;
}

const std::string& MeasurementResult::Unit() const {
    return unit_;
}

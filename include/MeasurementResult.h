#pragma once

#include <string>

class MeasurementResult {
public:
    MeasurementResult(double value, std::string unit);

    double Value() const;
    const std::string& Unit() const;

private:
    double value_;
    std::string unit_;
};

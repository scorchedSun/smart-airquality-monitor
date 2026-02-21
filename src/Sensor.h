#pragma once

#include <memory>

#include "Measurement.h"

class SensorDriver {
   
public:
    virtual ~SensorDriver() = default;
    virtual bool begin() = 0;
    virtual bool provideMeasurements(std::vector<std::unique_ptr<Measurement>>& measurements) = 0;
};

#pragma once

#include <memory>

#include "Measurement.h"

class Sensor {
   
public:
    virtual bool begin() = 0;
    virtual bool provide_measurements(std::vector<std::unique_ptr<Measurement>>& measurements) = 0;
};

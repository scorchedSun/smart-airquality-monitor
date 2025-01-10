#pragma once

#include "ArduinoJson.h"
#include "Measurement.h"

class ValueReporter {
    
    public:
        virtual void report(const std::vector<std::unique_ptr<Measurement>>& measurements) = 0;

    protected:
        ValueReporter() = default;
};
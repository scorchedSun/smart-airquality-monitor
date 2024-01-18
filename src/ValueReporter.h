#pragma once

#include "ArduinoJson.h"

class ValueReporter {
    
    public:
        virtual void report(const JsonDocument& data) const = 0;

    protected:
        ValueReporter() = default;
};
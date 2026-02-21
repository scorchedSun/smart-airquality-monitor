#pragma once

#include <string>
#include <format>

#include "ArduinoJson.h"

enum class MeasurementType { Temperature, Humidity, PM1, PM25, PM10, CO2 };
enum class MeasurementUnit { DegreesCelsius, Percent, PPM, MicroGramPerCubicMeter };

class MeasurementDetails {
    private:
        const MeasurementType type_;     
        const MeasurementUnit unit_;
    
    public:
        MeasurementDetails(MeasurementType type, MeasurementUnit unit) 
            : type_(type)
            , unit_(unit) {
        }

        MeasurementType getType() const {
            return type_;
        }

        MeasurementUnit getUnit() const {
            return unit_;
        }
};

class Measurement {
    private:
        const MeasurementDetails details_;
        const std::string formatted_value_;

    protected:
        Measurement(MeasurementDetails details, std::string formatted_value) 
            : details_(details)
            , formatted_value_(std::move(formatted_value)) {
        }

    public:
        virtual ~Measurement() = default;

        MeasurementDetails getDetails() const {
            return details_;
        }

        const std::string& valueToString() const {
            return formatted_value_;
        }
};

class DecimalMeasurement : public Measurement 
{
    private:
        const double value_;

    public:
        DecimalMeasurement(MeasurementDetails details, double value)
            : Measurement(details, std::format("{:.2f}", value))
            , value_(value) 
        {                
        }

        double getValue() const {
            return value_;
        }
};

class RoundNumberMeasurement : public Measurement
{
    private:
        const uint32_t value_;

    public:
        RoundNumberMeasurement(MeasurementDetails details, uint32_t value) 
            : Measurement(details, std::to_string(value))
            , value_(value)
        {            
        }

        uint32_t getValue() const {
            return value_;
        }
};
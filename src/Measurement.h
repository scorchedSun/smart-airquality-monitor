#pragma once

#include <string>
#include <format>

#include "ArduinoJson.h"

enum class MeasurementType { Temperature, Humidity, PM1, PM25, PM10, CO2 };
enum class MeasurementUnit { DegreesCelcius, Percent, PPM, MicroGramPerCubicMeter };

class MeasurementDetails {
    private:
        const MeasurementType type_;     
        const MeasurementUnit unit_;
    
    public:
        MeasurementDetails(const MeasurementType type, const MeasurementUnit unit) 
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

    protected:
        Measurement(const MeasurementDetails details) 
            : details_(details) {
        }

    private:
        mutable std::string cached_value_;

    protected:
        virtual std::string formatValue() const = 0;

    public:
        MeasurementDetails getDetails() const {
            return details_;
        }

        const std::string& valueToString() const {
            if (cached_value_.empty()) {
                cached_value_ = formatValue();
            }
            return cached_value_;
        }
};

class DecimalMeasurement : public Measurement 
{
    private:
        const double value_;

    public:
        DecimalMeasurement(const MeasurementDetails details, const double value)
            : Measurement(details)
            , value_(value) 
        {                
        }

        double getValue() const {
            return value_;
        }

        std::string formatValue() const override {
            return std::format("{:.2f}", value_);
        }
};

class RoundNumberMeasurement : public Measurement
{
    private:
        const uint32_t value_;

    public:
        RoundNumberMeasurement(const MeasurementDetails details, const uint32_t value) 
            : Measurement(details)
            , value_(value)
        {            
        }

        uint32_t getValue() const {
            return value_;
        }

        std::string formatValue() const override {
            return std::to_string(value_);
        }
};
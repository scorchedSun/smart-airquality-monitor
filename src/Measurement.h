#pragma once

#include <string>
#include <format>

#include "ArduinoJson.h"

enum class MeasurementType { Temperature, Humidity, PM1, PM25, PM10, CO2 };
enum class MeasurementUnit { DegreesCelcius, Percent, PPM, MicroGramPerCubicMeter };

class MeasurementDetails {
    private:
        const MeasurementType type;     
        const MeasurementUnit unit;
    
    public:
        MeasurementDetails(const MeasurementType type, const MeasurementUnit unit) 
            : type(type)
            , unit(unit) {
        }

        MeasurementType get_type() const {
            return type;
        }

        MeasurementUnit get_unit() const {
            return unit;
        }
};

class Measurement {
    private:
        const MeasurementDetails details;

    protected:
        Measurement(const MeasurementDetails details) 
            : details(details) {
        }

    private:
        mutable std::string cached_value;

    protected:
        virtual std::string format_value() const = 0;

    public:
        MeasurementDetails get_details() const {
            return details;
        }

        const std::string& value_to_string() const {
            if (cached_value.empty()) {
                cached_value = format_value();
            }
            return cached_value;
        }
};

class DecimalMeasurement : public Measurement 
{
    private:
        const double value;

    public:
        DecimalMeasurement(const MeasurementDetails details, const double value)
            : Measurement(details)
            , value(value) 
        {                
        }

        double get_value() const {
            return value;
        }

        std::string format_value() const override {
            return std::format("{:.2f}", value);
        }
};

class RoundNumberMeasurement : public Measurement
{
    private:
        const uint32_t value;

    public:
        RoundNumberMeasurement(const MeasurementDetails details, const uint32_t value) 
            : Measurement(details)
            , value(value)
        {            
        }

        uint32_t get_value() const {
            return value;
        }

        std::string format_value() const override {
            return std::to_string(value);
        }
};
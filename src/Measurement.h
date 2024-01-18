#pragma once

#include <string>
#include <sstream>
#include <iomanip>

#include "ArduinoJson.h"

static const std::string names[6] = {"Temperature", "Humidity", "PM1", "PM2.5", "PM10", "CO2"};

class Measurement 
{
    public:
        enum class Type { Temperature, Humidity, PM1, PM25, PM10, CO2 };

    private:
        const std::string unit;
        const Measurement::Type type;     

    protected:
        Measurement(Measurement::Type type, const std::string& unit) 
            : type(type)
            , unit(unit)
        {
        }

    public:
        class HumanReadableFormatter {

            private:
                

            public:
                HumanReadableFormatter() = delete;

                static std::string format(const Measurement::Type& type) {
                    return names[(uint16_t)type];
                }

                static std::string format(const std::unique_ptr<Measurement>& measurement) {
                    std::string result(format(measurement->get_type()));
                    return result.append(": ").append(measurement->value_as_string()).append(" ").append(measurement->get_unit()); 
                }

        };

        virtual std::string value_as_string() const = 0;

        Measurement::Type get_type() const {
            return type;
        }

        std::string get_unit() const {
            return unit;
        }

        std::string in_json_format() const {
            std::string result("{");
            result
                .append("\"unit\": \"").append(unit).append("\",")
                .append("\"value\": ").append(value_as_string())
                .append("}");
            return result;
        }
};

class DecimalMeasurement : public Measurement 
{
    private:
        const double value;

    public:
        DecimalMeasurement(Measurement::Type type, const double value, const std::string& unit)
            : Measurement(type, unit)
            , value(value) 
        {                
        }

        std::string value_as_string() const override {
            std::ostringstream result;
            result.precision(4);
            result << std::ceil(value * 100.0) / 100.0;
            return result.str();
        }
};

class RoundNumberMeasurement : public Measurement
{
    private:
        const uint32_t value;

    public:
        RoundNumberMeasurement(Measurement::Type type, const uint32_t value, const std::string& unit) 
            : Measurement(type, unit)
            , value(value)
        {            
        }

        std::string value_as_string() const override {
            return std::to_string(value);
        }
};
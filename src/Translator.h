#pragma once

#include <string_view>

template <typename T>
class Translator {
public:
    virtual std::string_view translate(const T& to_translate) const = 0;
};

class DisplayUnitTranslator : public Translator<MeasurementUnit> {
public:
    std::string_view translate(const MeasurementUnit& unit) const override {
        switch (unit) {
            case MeasurementUnit::DegreesCelsius: return "\xF7" "C";
            case MeasurementUnit::Percent: return "%";
            case MeasurementUnit::PPM: return "ppm";
            case MeasurementUnit::MicroGramPerCubicMeter: return "ug/m3";
            default: return "";
        }
    }
};

class FriendlyNameTypeTranslator : public Translator<MeasurementType> {
public:
    std::string_view translate(const MeasurementType& type) const override {
        switch (type) {
            case MeasurementType::Temperature: return "Temperature";
            case MeasurementType::Humidity: return "Humidity";
            case MeasurementType::PM1: return "PM1";
            case MeasurementType::PM25: return "PM2.5";
            case MeasurementType::PM10: return "PM10";
            case MeasurementType::CO2: return "CO2";
            default: return "";
        }
    }
};
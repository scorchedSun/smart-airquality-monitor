#pragma once

#include <unordered_map>

#include "Measurement.h"

template <typename T>
class Translator {
public:
    virtual std::string translate(const T& to_translate) const = 0;
};

class DisplayUnitTranslator : public Translator<MeasurementUnit> {
    const std::unordered_map<MeasurementUnit, std::string> unit_names = {
        {MeasurementUnit::DegreesCelcius, "C"},
        {MeasurementUnit::Percent, "%"},
        {MeasurementUnit::PPM, "ppm"},
        {MeasurementUnit::MicroGramPerCubicMeter, "ug/m3"}
    };

public:
    std::string translate(const MeasurementUnit& unit) const override {
        return unit_names.find(unit)->second;
    }
};

class FriendlyNameTypeTranslator : public Translator<MeasurementType> {
    const std::unordered_map<MeasurementType, std::string> type_names = {
        {MeasurementType::Temperature, "Temperature"},
        {MeasurementType::Humidity, "Humidity"},
        {MeasurementType::PM1, "PM1"},
        {MeasurementType::PM25, "PM2.5"},
        {MeasurementType::PM10, "PM10"},
        {MeasurementType::CO2, "CO2"}
    };

public:
    std::string translate(const MeasurementType& type) const override {
        return type_names.find(type)->second;
    }
};
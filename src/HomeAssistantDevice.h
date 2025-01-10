#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "ArduinoJson.h"

#include "Sensor.h"
#include "ReconnectingPubSubClient.h"
#include "Translator.h"

class HomeAssistantDevice {

private:    
    const std::unordered_map<MeasurementType, std::string> device_classes = {
            {MeasurementType::Temperature, "temperature"},
            {MeasurementType::Humidity, "humidity"},
            {MeasurementType::PM1, "pm1"},
            {MeasurementType::PM25, "pm25"},
            {MeasurementType::PM10, "pm10"},
            {MeasurementType::CO2, "carbon_dioxide"}
        };
    const std::unordered_map<MeasurementType, std::string> abbreviated_names = {
            {MeasurementType::Temperature, "temp"},
            {MeasurementType::Humidity, "hum"},
            {MeasurementType::PM1, "pm1"},
            {MeasurementType::PM25, "pm25"},
            {MeasurementType::PM10, "pm10"},
            {MeasurementType::CO2, "co2"}
        };
    const std::unordered_map<MeasurementUnit, std::string> unit_map = {
        {MeasurementUnit::DegreesCelcius, "°C"},
        {MeasurementUnit::Percent, "%"},
        {MeasurementUnit::PPM, "ppm"},
        {MeasurementUnit::MicroGramPerCubicMeter, "µg/m³"}
    };
    const FriendlyNameTypeTranslator friendly_name_type_translator;

    const std::string discovery_prefix;
    const std::string device_prefix;
    const std::string mac_id;
    const std::string device_name;
    const std::string device_id;
    const std::string topic_base;
    const std::string sensor_state_topic;
    DynamicJsonDocument device_json;

    std::unordered_map<MeasurementType, bool> discovery_message_sent;
public:

    HomeAssistantDevice() = delete;

    HomeAssistantDevice(const std::string device_prefix,
                        const std::string mac_id,
                        const std::string device_name,
                        const std::string software_version,
                        const std::string& discovery_prefix = "homeassistant")
        : device_prefix(device_prefix)
        , mac_id(mac_id)
        , device_name(device_name)
        , discovery_prefix(discovery_prefix)
        , device_id(std::string(device_prefix).append(mac_id))
        , topic_base(std::string(discovery_prefix).append("/sensor/"))
        , sensor_state_topic(std::string(topic_base).append(device_id).append("/state"))
        , device_json(512)
    {
        JsonArray identifiers(device_json.createNestedArray("ids"));
        identifiers.add(device_id);
        device_json["name"] = device_name;
        device_json["sw"] = software_version;
    }

    std::string get_unique_id_for(const MeasurementDetails& details) const {
        return std::string(abbreviated_names.find(details.get_type())->second).append("_").append(mac_id);
    }

    std::string get_device_class_for(const MeasurementDetails& details) const {
        return device_classes.find(details.get_type())->second;
    }

    DynamicJsonDocument get_discovery_message_for(const MeasurementDetails& details) const {
        const std::string& device_class = get_device_class_for(details);

        DynamicJsonDocument discovery_json(1024);
        discovery_json["dev"] = device_json;
        discovery_json["name"] = friendly_name_type_translator.translate(details.get_type());
        discovery_json["uniq_id"] = get_unique_id_for(details);
        discovery_json["dev_cla"] = device_class;
        discovery_json["val_tpl"] = std::string("{{ value_json.").append(device_class).append(" }}");
        discovery_json["unit_of_meas"] = unit_map.find(details.get_unit())->second;
        discovery_json["stat_t"] = sensor_state_topic;

        return discovery_json;
    }

    std::string get_discovery_topic_for(const MeasurementDetails& details) const {
        return std::string(topic_base).append(device_id).append("/").append(abbreviated_names.find(details.get_type())->second).append("/config");
    }

    std::string get_device_id() const {
        return device_id;
    }

    std::string get_sensor_state_topic() const {
        return sensor_state_topic;
    }

    bool has_discovery_message_been_sent_for(const MeasurementDetails& details) const {
        const auto& iterator = discovery_message_sent.find(details.get_type());

        if (iterator != discovery_message_sent.cend()) {
            return iterator->second;
        }
        return false;
    }

    void set_discovery_message_sent_for(const MeasurementDetails& details) {
        discovery_message_sent[details.get_type()] = true;
    }
};
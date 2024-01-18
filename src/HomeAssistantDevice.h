#pragma once

#include <string>
#include <memory>
#include <vector>

#include "ArduinoJson.h"

#include "Measurement.h"
#include "ReconnectingPubSubClient.h"

static const char default_discovery_prefix[] = "homeassistant";
static const std::string abbrevations_of_measurement_types[6] = {"temp", "hum", "pm1", "pm25", "pm10", "co2"};
static const std::string device_classes_of_measurement_types[6] = {"temperature", "humidity", "pm1", "pm25", "pm10", "carbon_dioxide"};
static const std::string units_of_measurements[6] = {"°C", "%", "µg/m³", "µg/m³", "µg/m³", "ppm"};

class HomeAssistantDevice {

private:    
    const std::string discovery_prefix;
    const std::string device_prefix;
    const std::string mac_id;
    const std::string device_name;
    const std::string device_id;
    const std::string topic_base;
    const std::string sensor_state_topic;
    DynamicJsonDocument device_json;

public:

    HomeAssistantDevice() = delete;

    HomeAssistantDevice(const std::string device_prefix,
                        const std::string mac_id,
                        const std::string device_name,
                        const std::string software_version,
                        const std::string& discovery_prefix = default_discovery_prefix)
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

    DynamicJsonDocument get_discovery_message_for_sensor(const Measurement::Type& measurement_type) const {

        const uint16_t type_index((uint16_t)measurement_type);
        const std::string device_class(device_classes_of_measurement_types[type_index]);

        DynamicJsonDocument discovery_json(1024);
        discovery_json["dev"] = device_json;
        discovery_json["name"] = Measurement::HumanReadableFormatter::format(measurement_type);
        discovery_json["uniq_id"] = std::string(abbrevations_of_measurement_types[type_index]).append("_").append(mac_id);
        discovery_json["dev_cla"] = device_class,
        discovery_json["val_tpl"] = std::string("{{ value_json.").append(device_class).append(" }}");
        discovery_json["unit_of_meas"] = units_of_measurements[type_index];
        discovery_json["stat_t"] = sensor_state_topic;

        return discovery_json;
    }

    std::string get_discovery_topic_for(const Measurement::Type& type) const {
        const uint16_t type_index((uint16_t)type);
        return std::string(topic_base).append(device_id).append("/").append(abbrevations_of_measurement_types[type_index]).append("/config");
    }

    std::string get_device_id() const {
        return device_id;
    }

    std::string get_sensor_state_topic() const {
        return sensor_state_topic;
    }

    std::string get_key_for(const Measurement::Type& type) const {
        return device_classes_of_measurement_types[(uint16_t)type];
    }
};
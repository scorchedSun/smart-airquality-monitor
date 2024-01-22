#pragma once

#include <string>
#include <memory>
#include <vector>

#include "ArduinoJson.h"

#include "Measurement.h"
#include "ReconnectingPubSubClient.h"

static const char default_discovery_prefix[] = "homeassistant";

class HomeAssistantSensorDefinition {

private:
    const std::string _name;
    const std::string _abbreviated_name;
    const std::string _device_class;
    const std::string _unit_of_measurement;

public:
    HomeAssistantSensorDefinition(const std::string& name,
                                  const std::string& abbreviated_name,
                                  const std::string& device_class,
                                  const std::string& unit_of_measurement) 
        : _name(name)
        , _abbreviated_name(abbreviated_name)
        , _device_class(device_class)
        , _unit_of_measurement(unit_of_measurement) {
    }

    const std::string& name() const { return _name; }
    const std::string& abbreviated_name() const { return _abbreviated_name; }
    const std::string& device_class() const { return _device_class; }
    const std::string& unit_of_measurement() const { return _unit_of_measurement; }
};

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
    std::vector<std::unique_ptr<HomeAssistantSensorDefinition>> _sensor_definitions;

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

    void register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>&& sensor_definition) {
        _sensor_definitions.push_back(std::move(sensor_definition));
    }

    DynamicJsonDocument get_discovery_message_for_sensor(const std::unique_ptr<HomeAssistantSensorDefinition>& sensor_definition) const {
        const std::string& device_class = sensor_definition->device_class();

        DynamicJsonDocument discovery_json(1024);
        discovery_json["dev"] = device_json;
        discovery_json["name"] = sensor_definition->name();
        discovery_json["uniq_id"] = std::string(sensor_definition->abbreviated_name()).append("_").append(mac_id);
        discovery_json["dev_cla"] = device_class;
        discovery_json["val_tpl"] = std::string("{{ value_json.").append(device_class).append(" }}");
        discovery_json["unit_of_meas"] = sensor_definition->unit_of_measurement();
        discovery_json["stat_t"] = sensor_state_topic;

        return discovery_json;
    }

    const std::vector<std::unique_ptr<HomeAssistantSensorDefinition>>& sensor_definitions() const { return _sensor_definitions; }

    std::string get_discovery_topic_for(const std::unique_ptr<HomeAssistantSensorDefinition>& sensor_definition) const {
        return std::string(topic_base).append(device_id).append("/").append(sensor_definition->abbreviated_name()).append("/config");
    }

    std::string get_device_id() const {
        return device_id;
    }

    std::string get_sensor_state_topic() const {
        return sensor_state_topic;
    }
};
#pragma once

#include <string>
#include <ArduinoJson.h>
#include <vector>
#include "HADevice.h"

namespace ha {

class Component {
protected:
    const std::string component_type;
    const std::string object_id;
    const std::string friendly_name;
    const std::string unique_id;
    const std::string discovery_prefix;
    const std::string device_id;
    const std::string base_topic;

public:
    Component(const Device& device,
                const std::string& component_type,
                const std::string& object_id,
                const std::string& friendly_name,
                const std::string& discovery_prefix = "homeassistant")
        : component_type(component_type)
        , object_id(object_id)
        , friendly_name(friendly_name)
        , unique_id(object_id + "_" + device.get_mac_id())
        , discovery_prefix(discovery_prefix)
        , device_id(device.get_device_id())
        , base_topic(discovery_prefix + "/" + component_type + "/" + device_id + "/" + object_id)
    {
    }

    virtual ~Component() = default;

    virtual std::string get_discovery_topic() const {
        return base_topic + "/config";
    }

    virtual std::string get_state_topic() const {
        return discovery_prefix + "/device/" + device_id + "/state";
    }

    virtual std::string get_command_topic() const { return ""; }
    virtual std::vector<std::string> get_command_topics() const { return {}; }
    virtual std::string get_state_payload() const { return ""; }

    virtual DynamicJsonDocument get_discovery_payload(const Device& device) const {
        DynamicJsonDocument doc(1024);
        doc["dev"] = device.get_device_info_json();
        doc["name"] = friendly_name;
        doc["uniq_id"] = unique_id;
        doc["stat_t"] = get_state_topic();
        return doc;
    }

    std::string get_object_id() const { return object_id; }
    std::string get_component_type() const { return component_type; }

    virtual void handle_command(const std::string& topic, const std::string& payload) {}

    virtual void populate_state(JsonObject& doc) const {
        std::string state = get_state_payload();
        if (!state.empty()) {
            doc[object_id] = state;
        }
    }
};

} // namespace ha

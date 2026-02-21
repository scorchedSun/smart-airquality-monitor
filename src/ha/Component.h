#pragma once

#include <string>
#include <string_view>
#include <ArduinoJson.h>
#include <vector>
#include "Device.h"

namespace ha {

class Component {
protected:
    const std::string component_type_;
    const std::string object_id_;
    const std::string friendly_name_;
    const std::string unique_id_;
    const std::string discovery_prefix_;
    const std::string device_id_;
    const std::string base_topic_;

public:
    Component(const Device& device,
                std::string_view component_type,
                std::string_view object_id,
                std::string_view friendly_name,
                std::string_view discovery_prefix = "homeassistant")
        : component_type_{component_type}
        , object_id_{object_id}
        , friendly_name_{friendly_name}
        , unique_id_(std::string{object_id} + "_" + std::string{device.getMacId()})
        , discovery_prefix_{discovery_prefix}
        , device_id_{device.getDeviceId()}
        , base_topic_(std::string{discovery_prefix} + "/" + std::string{component_type} + "/" + device_id_ + "/" + std::string{object_id})
    {
    }

    virtual ~Component() = default;

    virtual std::string getDiscoveryTopic() const {
        return base_topic_ + "/config";
    }

    virtual std::string getStateTopic() const {
        return discovery_prefix_ + "/device/" + device_id_ + "/state";
    }

    virtual std::string getCommandTopic() const { return ""; }
    virtual std::vector<std::string> getCommandTopics() const { return {}; }
    virtual std::string getStatePayload() const { return ""; }

    virtual StaticJsonDocument<1024> getDiscoveryPayload(const Device& device) const {
        StaticJsonDocument<1024> doc;
        doc["dev"] = device.getDeviceInfoJson();
        doc["name"] = friendly_name_;
        doc["uniq_id"] = unique_id_;
        doc["stat_t"] = getStateTopic();
        doc["avty_t"] = std::string{device.getAvailabilityTopic()};
        doc["pl_avail"] = std::string{device.getAvailabilityPayloadOnline()};
        doc["pl_not_avail"] = std::string{device.getAvailabilityPayloadOffline()};
        return doc;
    }

    std::string_view getObjectId() const { return object_id_; }
    std::string_view getComponentType() const { return component_type_; }

    virtual void handleCommand(const std::string& topic, const std::string& payload) {}

    virtual void populateState(JsonObject& doc) const {
        std::string state = getStatePayload();
        if (!state.empty()) {
            doc[object_id_] = state;
        }
    }
};

} // namespace ha

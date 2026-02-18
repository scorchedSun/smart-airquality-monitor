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
        : component_type_(std::string(component_type.data(), component_type.length()))
        , object_id_(std::string(object_id.data(), object_id.length()))
        , friendly_name_(std::string(friendly_name.data(), friendly_name.length()))
        , unique_id_(std::string(object_id.data(), object_id.length()) + "_" + std::string(device.getMacId().data(), device.getMacId().length()))
        , discovery_prefix_(std::string(discovery_prefix.data(), discovery_prefix.length()))
        , device_id_(std::string(device.getDeviceId().data(), device.getDeviceId().length()))
        , base_topic_(std::string(discovery_prefix.data(), discovery_prefix.length()) + "/" + std::string(component_type.data(), component_type.length()) + "/" + device_id_ + "/" + std::string(object_id.data(), object_id.length()))
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

    virtual DynamicJsonDocument getDiscoveryPayload(const Device& device) const {
        DynamicJsonDocument doc(1024);
        doc["dev"] = device.getDeviceInfoJson();
        doc["name"] = friendly_name_;
        doc["uniq_id"] = unique_id_;
        doc["stat_t"] = getStateTopic();
        doc["avty_t"] = std::string(device.getAvailabilityTopic().data(), device.getAvailabilityTopic().length());
        std::string_view on = device.getAvailabilityPayloadOnline();
        doc["pl_avail"] = std::string(on.data(), on.length());
        std::string_view off = device.getAvailabilityPayloadOffline();
        doc["pl_not_avail"] = std::string(off.data(), off.length());
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

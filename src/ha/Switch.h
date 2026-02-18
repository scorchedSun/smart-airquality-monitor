#pragma once

#include "Component.h"
#include <functional>
#include "../Logger.h"

namespace ha {

class Switch : public Component {
private:
    const std::string command_topic_;
    std::function<void(bool)> callback_;
    bool current_state_ = false;

public:
    Switch(const Device& device,
             std::string_view object_id,
             std::string_view friendly_name,
             std::function<void(bool)> on_toggle_callback)
        : Component(device, "switch", object_id, friendly_name)
        , command_topic_(base_topic_ + "/set")
        , callback_(on_toggle_callback)
    {
    }

    DynamicJsonDocument getDiscoveryPayload(const Device& device) const override {
        DynamicJsonDocument doc = Component::getDiscoveryPayload(device);
        doc["cmd_t"] = command_topic_;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        doc["val_tpl"] = std::string("{{ value_json.").append(object_id_).append(" }}");
        return doc;
    }

    std::string getCommandTopic() const override {
        return command_topic_;
    }

    std::vector<std::string> getCommandTopics() const override {
        return { command_topic_ };
    }

    std::string getStatePayload() const override {
        return current_state_ ? "ON" : "OFF";
    }

    void updateState(bool state) {
        current_state_ = state;
    }

    void handleCommand(const std::string& topic, const std::string& payload) override {
        bool new_state = (payload == "ON");
        updateState(new_state);
        if (callback_) {
            callback_(new_state);
        }
    }
};

} // namespace ha

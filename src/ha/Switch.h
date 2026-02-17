#pragma once

#include "HAComponent.h"
#include <functional>
#include "Logger.h"

namespace ha {

class Switch : public Component {
private:
    const std::string command_topic;
    std::function<void(bool)> callback;
    bool current_state = false;

public:
    Switch(const Device& device,
             const std::string& object_id,
             const std::string& friendly_name,
             std::function<void(bool)> on_toggle_callback)
        : Component(device, "switch", object_id, friendly_name)
        , command_topic(base_topic + "/set")
        , callback(on_toggle_callback)
    {
    }

    DynamicJsonDocument get_discovery_payload(const Device& device) const override {
        DynamicJsonDocument doc = Component::get_discovery_payload(device);
        doc["cmd_t"] = command_topic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        doc["val_tpl"] = std::string("{{ value_json.").append(object_id).append(" }}");
        return doc;
    }

    std::string get_command_topic() const override {
        return command_topic;
    }

    std::vector<std::string> get_command_topics() const override {
        return { command_topic };
    }

    std::string get_state_payload() const override {
        return current_state ? "ON" : "OFF";
    }

    void update_state(bool state) {
        current_state = state;
    }

    void handle_command(const std::string& topic, const std::string& payload) override {
        bool new_state = (payload == "ON");
        update_state(new_state);
        if (callback) {
            callback(new_state);
        }
    }
};

} // namespace ha

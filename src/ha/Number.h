#pragma once

#include "HAComponent.h"
#include <functional>

namespace ha {

class Number : public Component {
private:
    const std::string command_topic;
    const float min_val;
    const float max_val;
    const float step_val;
    std::function<void(float)> callback;
    float current_value = 0;

public:
    Number(const Device& device,
             const std::string& object_id,
             const std::string& friendly_name,
             float min, float max, float step,
             std::function<void(float)> on_change_callback)
        : Component(device, "number", object_id, friendly_name)
        , command_topic(base_topic + "/set")
        , min_val(min)
        , max_val(max)
        , step_val(step)
        , callback(on_change_callback)
    {
    }

    DynamicJsonDocument get_discovery_payload(const Device& device) const override {
        DynamicJsonDocument doc = Component::get_discovery_payload(device);
        doc["cmd_t"] = command_topic;
        doc["min"] = min_val;
        doc["max"] = max_val;
        doc["step"] = step_val;
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
        return std::to_string(current_value);
    }

    void update_value(float value) {
        current_value = value;
    }

    void handle_command(const std::string& topic, const std::string& payload) override {
        if (payload.empty()) return;
        float new_val = atof(payload.c_str());
        current_value = new_val;
        if (callback) {
            callback(new_val);
        }
    }
};

} // namespace ha

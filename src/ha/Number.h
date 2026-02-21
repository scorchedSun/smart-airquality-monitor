#pragma once

#include "Component.h"
#include <functional>

namespace ha {

class Number : public Component {
private:
    const std::string command_topic_;
    const float min_val_;
    const float max_val_;
    const float step_val_;
    std::function<void(float)> callback_;
    float current_value_ = 0;

public:
    Number(const Device& device,
             std::string_view object_id,
             std::string_view friendly_name,
             float min, float max, float step,
             std::function<void(float)> on_change_callback)
        : Component(device, "number", object_id, friendly_name)
        , command_topic_(base_topic_ + "/set")
        , min_val_(min)
        , max_val_(max)
        , step_val_(step)
        , callback_(on_change_callback)
    {
    }

    StaticJsonDocument<1024> getDiscoveryPayload(const Device& device) const override {
        StaticJsonDocument<1024> doc = Component::getDiscoveryPayload(device);
        doc["cmd_t"] = command_topic_;
        doc["min"] = min_val_;
        doc["max"] = max_val_;
        doc["step"] = step_val_;
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
        return std::to_string(current_value_);
    }

    void updateValue(float value) {
        current_value_ = value;
    }

    void handleCommand(const std::string& topic, const std::string& payload) override {
        if (payload.empty()) return;
        char* end = nullptr;
        float new_val = std::strtof(payload.c_str(), &end);
        if (end == payload.c_str()) return; // Parse failed
        current_value_ = new_val;
        if (callback_) {
            callback_(new_val);
        }
    }
};

} // namespace ha

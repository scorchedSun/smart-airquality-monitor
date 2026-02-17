#pragma once

#include "Component.h"
#include <functional>

namespace ha {

class Fan : public Component {
private:
    const std::string command_topic_;
    const std::string percentage_command_topic_;
    std::function<void(bool)> on_off_callback_;
    std::function<void(uint8_t)> speed_callback_;
    bool current_state_ = false;
    uint8_t current_speed_ = 0;

public:
    Fan(const Device& device,
             const std::string& object_id,
             const std::string& friendly_name,
             std::function<void(bool)> on_off_cb,
             std::function<void(uint8_t)> speed_cb)
        : Component(device, "fan", object_id, friendly_name)
        , command_topic_(base_topic_ + "/set")
        , percentage_command_topic_(base_topic_ + "/speed/set")
        , on_off_callback_(on_off_cb)
        , speed_callback_(speed_cb)
    {
    }

    DynamicJsonDocument getDiscoveryPayload(const Device& device) const override {
        DynamicJsonDocument doc = Component::getDiscoveryPayload(device);
        doc["cmd_t"] = command_topic_;
        doc["pct_cmd_t"] = percentage_command_topic_;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["pct_stat_t"] = getStateTopic();
        doc["stat_val_tpl"] = std::string("{{ value_json.").append(object_id_).append("_state }}");
        doc["pct_val_tpl"] = std::string("{{ value_json.").append(object_id_).append("_speed }}");
        doc["spd_rng_min"] = 1;
        doc["spd_rng_max"] = 100;
        return doc;
    }

    std::string getCommandTopic() const override {
        return command_topic_; 
    }

    std::vector<std::string> getCommandTopics() const override {
        std::vector<std::string> topics = { command_topic_ };
        if (!percentage_command_topic_.empty()) {
            topics.push_back(percentage_command_topic_);
        }
        return topics;
    }
    
    std::string getPercentageCommandTopic() const {
        return percentage_command_topic_;
    }

    void handleCommand(const std::string& topic, const std::string& payload) override {
        if (topic == command_topic_) {
            bool new_state = (payload == "ON");
            updateState(new_state);
            if (on_off_callback_) on_off_callback_(new_state);
        } else if (topic == percentage_command_topic_) {
            if (payload.empty()) return;
            int speed = atoi(payload.c_str());
            if (speed < 0) speed = 0;
            if (speed > 100) speed = 100;
            updateSpeed((uint8_t)speed);
            if (speed_callback_) speed_callback_((uint8_t)speed);
        }
    }

    void updateState(bool state) {
        current_state_ = state;
    }

    void updateSpeed(uint8_t speed) {
        current_speed_ = speed;
        if (speed > 0 && !current_state_) {
            current_state_ = true;
        }
    }

    void populateState(JsonObject& doc) const override {
        doc[object_id_ + "_state"] = current_state_ ? "ON" : "OFF";
        doc[object_id_ + "_speed"] = current_speed_;
    }
};

} // namespace ha

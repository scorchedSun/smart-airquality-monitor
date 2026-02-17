#pragma once

#include "HAComponent.h"
#include <functional>

namespace ha {

class Fan : public Component {
private:
    const std::string command_topic;
    const std::string percentage_command_topic;
    std::function<void(bool)> on_off_callback;
    std::function<void(uint8_t)> speed_callback;
    bool current_state = false;
    uint8_t current_speed = 0;

public:
    Fan(const Device& device,
             const std::string& object_id,
             const std::string& friendly_name,
             std::function<void(bool)> on_off_cb,
             std::function<void(uint8_t)> speed_cb)
        : Component(device, "fan", object_id, friendly_name)
        , command_topic(base_topic + "/set")
        , percentage_command_topic(base_topic + "/speed/set")
        , on_off_callback(on_off_cb)
        , speed_callback(speed_cb)
    {
    }

    DynamicJsonDocument get_discovery_payload(const Device& device) const override {
        DynamicJsonDocument doc = Component::get_discovery_payload(device);
        doc["cmd_t"] = command_topic;
        doc["pct_cmd_t"] = percentage_command_topic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["pct_stat_t"] = get_state_topic();
        doc["stat_val_tpl"] = std::string("{{ value_json.").append(object_id).append("_state }}");
        doc["pct_val_tpl"] = std::string("{{ value_json.").append(object_id).append("_speed }}");
        doc["spd_rng_min"] = 1;
        doc["spd_rng_max"] = 100;
        return doc;
    }

    std::string get_command_topic() const override {
        return command_topic; 
    }

    std::vector<std::string> get_command_topics() const override {
        std::vector<std::string> topics = { command_topic };
        if (!percentage_command_topic.empty()) {
            topics.push_back(percentage_command_topic);
        }
        return topics;
    }
    
    std::string get_percentage_command_topic() const {
        return percentage_command_topic;
    }

    void handle_command(const std::string& topic, const std::string& payload) override {
        if (topic == command_topic) {
            bool new_state = (payload == "ON");
            update_state(new_state);
            if (on_off_callback) on_off_callback(new_state);
        } else if (topic == percentage_command_topic) {
            if (payload.empty()) return;
            int speed = atoi(payload.c_str());
            if (speed < 0) speed = 0;
            if (speed > 100) speed = 100;
            update_speed((uint8_t)speed);
            if (speed_callback) speed_callback((uint8_t)speed);
        }
    }

    void update_state(bool state) {
        current_state = state;
    }

    void update_speed(uint8_t speed) {
        current_speed = speed;
        if (speed > 0 && !current_state) {
            current_state = true;
        }
    }

    void populate_state(JsonObject& doc) const override {
        doc[object_id + "_state"] = current_state ? "ON" : "OFF";
        doc[object_id + "_speed"] = current_speed;
    }
};

} // namespace ha

#pragma once

#include <vector>
#include <map>
#include <memory>
#include "HAMqtt.h"
#include "HADevice.h"
#include "HAComponent.h"
#include "HASwitch.h"
#include "HAFan.h"

namespace ha {

class Manager {
private:
    const std::shared_ptr<Device> device;
    const std::shared_ptr<MqttClient> mqtt_client;
    std::vector<std::shared_ptr<Component>> components;
    std::map<std::string, std::shared_ptr<Component>> command_topics;
    bool discovery_published = false;
    uint32_t last_report_time = 0;
    const uint32_t report_interval = 30000; // 30 seconds

public:
    Manager(const std::shared_ptr<Device> device,
                         const std::shared_ptr<MqttClient> mqtt_client)
        : device(device)
        , mqtt_client(mqtt_client)
    {
        mqtt_client->set_callback([this](const char* topic, const uint8_t* payload, size_t length) {
            if (!topic) return;
            std::string t(topic);
            std::string p = (payload && length > 0) ? std::string((const char*)payload, length) : "";
            
            auto it = command_topics.find(t);
            if (it != command_topics.end()) {
                it->second->handle_command(t, p);
            }
        });
    }

    void add_component(std::shared_ptr<Component> component) {
        components.push_back(component);
        
        for (const auto& topic : component->get_command_topics()) {
            if (!topic.empty()) {
                command_topics[topic] = component;
                mqtt_client->subscribe(topic);
            }
        }
    }

    void publish_discovery() {
        if (discovery_published) return;
        
        bool all_published = true;
        for (const auto& comp : components) {
            auto doc = comp->get_discovery_payload(*device);
            std::string payload;
            serializeJson(doc, payload);
            if (!mqtt_client->publish(comp->get_discovery_topic(), payload, true)) {
                all_published = false;
            }
        }
        discovery_published = all_published;
    }

    void report_state(bool force = false) {
        bool was_published = discovery_published;
        if (!discovery_published) publish_discovery();
        if (!discovery_published) return;

        if (!was_published) force = true;

        uint32_t now = millis();
        if (!force && (now - last_report_time < report_interval)) return;
        last_report_time = now;

        StaticJsonDocument<768> state_json;
        JsonObject root = state_json.to<JsonObject>();
        for (const auto& comp : components) {
            comp->populate_state(root);
        }
        
        if (state_json.size() > 0 && !components.empty()) {
            std::string payload;
            serializeJson(state_json, payload);
            mqtt_client->publish(components.front()->get_state_topic(), payload);
        }
    }
};

} // namespace ha

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "MqttClient.h"
#include "Device.h"
#include "Component.h"
#include "Switch.h"
#include "Fan.h"

namespace ha {

class Manager {
private:
    std::shared_ptr<Device> device_;
    std::shared_ptr<MqttClient> mqtt_client_;
    std::vector<std::shared_ptr<Component>> components_;
    
    bool discovery_published_ = false;
    uint32_t last_report_time_ = 0;
    const uint32_t report_interval_ = 30000; // 30 seconds
    
    mutable std::recursive_mutex manager_mutex_;

public:
    Manager(const std::shared_ptr<Device> device,
            const std::shared_ptr<MqttClient> mqtt_client)
        : device_(device)
        , mqtt_client_(mqtt_client) {
        
        mqtt_client_->setCallback([this](const char* topic, const uint8_t* payload, size_t length) {
            if (!topic) return;
            std::string payload_str = (payload && length > 0) ? std::string((const char*)payload, length) : "";
            
            std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
            for (auto& comp : components_) {
                for (const auto& cmd_topic : comp->getCommandTopics()) {
                    if (cmd_topic == topic) {
                        comp->handleCommand(topic, payload_str);
                    }
                }
            }
        });
    }

    void addComponent(std::shared_ptr<Component> component) {
        std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
        components_.push_back(component);
        for (const auto& topic : component->getCommandTopics()) {
            if (!topic.empty()) {
                mqtt_client_->subscribe(topic);
            }
        }
    }

    void publishDiscovery(bool force = false) {
        std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
        if (discovery_published_ && !force) return;
        
        bool all_published = true;
        for (const auto& comp : components_) {
            auto doc = comp->getDiscoveryPayload(*device_);
            std::string payload;
            serializeJson(doc, payload);
            if (!mqtt_client_->publish(comp->getDiscoveryTopic(), payload, true)) {
                all_published = false;
            }
        }
        discovery_published_ = all_published;
    }

    void reportState(bool force = false) {
        std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
        if (!mqtt_client_->isConnected()) {
            discovery_published_ = false; // Reset discovery on disconnect
            return;
        }

        bool was_published = discovery_published_;
        if (!discovery_published_) publishDiscovery();
        if (!discovery_published_) return;

        if (!was_published) force = true;

        uint32_t now = millis();
        if (!force && (now - last_report_time_ < report_interval_)) return;
        last_report_time_ = now;

        StaticJsonDocument<1024> state_json; // Slightly larger for safety
        JsonObject root = state_json.to<JsonObject>();
        for (const auto& comp : components_) {
            comp->populateState(root);
        }
        
        if (state_json.size() > 0 && !components_.empty()) {
            std::string payload;
            serializeJson(state_json, payload);
            mqtt_client_->publish(components_.front()->getStateTopic(), payload);
        }
    }
};

} // namespace ha

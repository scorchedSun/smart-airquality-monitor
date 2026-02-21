#pragma once

#include <memory>
#include <mutex>
#include "MqttClient.h"
#include "Manager.h"
#include "Device.h"
#include "../Logger.h"

namespace ha {

class StateReporter {
public:
    StateReporter(std::shared_ptr<ha::Device> device,
                  std::shared_ptr<ha::MqttClient> mqtt_client,
                  std::shared_ptr<ha::Manager> manager)
        : device_(device)
        , mqtt_client_(mqtt_client)
        , manager_(manager)
    {}

    using ReconnectedCallback = std::function<void()>;

    void setReconnectedCallback(ReconnectedCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        reconnected_cb_ = cb;
    }

    void loop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (mqtt_client_ && mqtt_client_->isConnected()) {
            if (!last_connected_state_) {
                last_connected_state_ = true;
                Logger::getInstance().log(Logger::Level::Info, "HAIntegration: MQTT Reconnected");
                if (reconnected_cb_) {
                    lock.unlock();
                    reconnected_cb_();
                    lock.lock();
                }
                // Publish online status
                if (device_) {
                    mqtt_client_->publish(device_->getAvailabilityTopic(), device_->getAvailabilityPayloadOnline(), true);
                }
                // Force immediate state report so HA gets current values
                if (manager_) manager_->reportState(true);
            }
            
            if (manager_) manager_->reportState(); 
            
            if (needs_report_) {
                if (manager_) manager_->reportState(true);
                needs_report_ = false;
            }
        } else {
            last_connected_state_ = false;
        }
    }

    void requestReport() {
        needs_report_ = true;
    }

    void forceReport() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (manager_) manager_->reportState(true);
    }

private:
    std::shared_ptr<ha::Device> device_;
    std::shared_ptr<ha::MqttClient> mqtt_client_;
    std::shared_ptr<ha::Manager> manager_;
    
    bool needs_report_ = false;
    bool last_connected_state_ = false;
    ReconnectedCallback reconnected_cb_;
    
    mutable std::mutex mutex_;
};

} // namespace ha

#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>

#include <map>
#include <mutex>
#include "MqttClient.h"
#include "Manager.h"
#include "Fan.h"
#include "Switch.h"
#include "Number.h"
#include "Sensor.h"
#include "../Measurement.h"
#include "../Logger.h"
#include "../ConfigKeys.h"

namespace ha {

class Integration {
public:
    using FanCallback = std::function<void(bool state, uint8_t speed)>;
    using DisplayCallback = std::function<void(bool state)>;
    using ConfigSaveCallback = std::function<void(const std::string& key, int value)>;
    using ReconnectedCallback = std::function<void()>;

    Integration(std::shared_ptr<ha::Device> device,
                  std::shared_ptr<ha::MqttClient> mqtt_client)
        : mqtt_client_(mqtt_client), device_(device) {
        manager_ = std::make_shared<ha::Manager>(device_, mqtt_client);
    }

    void begin() {
        setupControls();
    }

    void setFanCallback(FanCallback cb) {
        fan_cb_ = cb;
    }

    void setDisplayCallback(DisplayCallback cb) {
        display_cb_ = cb;
    }

    void setConfigSaveCallback(ConfigSaveCallback cb) {
        config_save_cb_ = cb;
    }

    void setReconnectedCallback(ReconnectedCallback cb) {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        reconnected_cb_ = cb;
    }

    void addSensor(MeasurementType type, const std::string& object_id, const std::string& name, 
                   const std::string& device_class, const std::string& unit) {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        if (!manager_) return;
        
        auto sensor = std::make_shared<ha::Sensor>(*device_, object_id, name, device_class, unit);
        manager_->addComponent(sensor);
        sensors_[type] = sensor;
    }
    
    void report(const std::vector<std::unique_ptr<Measurement>>& measurements) {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        bool updated = false;
        for (const auto& measurement : measurements) {
            MeasurementType type = measurement->getDetails().getType();
            
            auto it = sensors_.find(type);
            if (it == sensors_.end()) continue;

            const auto& sensor = it->second;
            sensor->updateState(measurement->valueToString());
            updated = true;
        }

        if (updated) {
            needs_report_ = true;
        }
    }

    void loop() {
        std::unique_lock<std::mutex> lock(integration_mutex_);
        
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

    void syncState(bool display_enabled, uint32_t display_interval_ms, 
                   uint32_t report_interval_s, uint8_t fan_speed, bool fan_on) {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        if (display_switch_) display_switch_->updateState(display_enabled);
        if (display_interval_) display_interval_->updateValue(display_interval_ms / 1000.0f);
        if (report_interval_) report_interval_->updateValue(report_interval_s / 60.0f);
        
        if (fan_) {
            fan_->updateState(fan_on);
            fan_->updateSpeed(fan_speed);
        }
        
        if (manager_) manager_->reportState(true);
    }
    
    void updateIpAddress(const std::string& ip) {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        if (ip_sensor_) {
            ip_sensor_->updateState(ip.c_str());
            if (manager_) manager_->reportState(true);
        }
    }

    std::string getDeviceId() const {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        return device_ ? device_->getDeviceId() : "";
    }

    std::shared_ptr<ha::Device> getDevice() const {
        std::lock_guard<std::mutex> lock(integration_mutex_);
        return device_;
    }

private:
    std::shared_ptr<ha::MqttClient> mqtt_client_;
    std::shared_ptr<ha::Device> device_;
    std::shared_ptr<ha::Manager> manager_;
    
    std::shared_ptr<ha::Fan> fan_;
    std::shared_ptr<ha::Switch> display_switch_;
    std::shared_ptr<ha::Number> display_interval_;
    std::shared_ptr<ha::Number> report_interval_;
    std::shared_ptr<ha::Sensor> ip_sensor_;

    std::map<MeasurementType, std::shared_ptr<ha::Sensor>> sensors_;
    bool needs_report_ = false;
    bool last_connected_state_ = false;

    FanCallback fan_cb_;
    DisplayCallback display_cb_;
    ConfigSaveCallback config_save_cb_;
    ReconnectedCallback reconnected_cb_;

    std::vector<std::pair<MeasurementType, std::shared_ptr<ha::Sensor>>> pending_sensors_;
    
    mutable std::mutex integration_mutex_;

    void setupControls() {
        if (!manager_) return;
        
        // Display Switch
        display_switch_ = std::make_shared<ha::Switch>(*device_, "display_toggle", "Display Enabled",
            [this](bool state) {
                if (display_cb_) display_cb_(state);
                if (config_save_cb_) config_save_cb_(cfg::keys::enable_display, state);
                
                Logger::getInstance().log(Logger::Level::Info, "Display %s via MQTT", state ? "enabled" : "disabled");
                
                if (display_switch_) display_switch_->updateState(state);
                if (manager_) manager_->reportState(true);
            });
        manager_->addComponent(display_switch_);

        // Display Interval
        display_interval_ = std::make_shared<ha::Number>(*device_, "display_interval", "Display Interval (s)",
            5, 15, 5, [this](float val) {
                if (config_save_cb_) config_save_cb_(cfg::keys::display_interval, (int)val);
                
                Logger::getInstance().log(Logger::Level::Info, "Display interval: %.1fs", val);
                if (manager_) manager_->reportState(true);
            });
        manager_->addComponent(display_interval_);

        // Report Interval
        report_interval_ = std::make_shared<ha::Number>(*device_, "report_interval", "Report Interval (m)",
            1, 15, 1, [this](float val) {
                if (config_save_cb_) config_save_cb_(cfg::keys::report_interval, (int)val);
                
                Logger::getInstance().log(Logger::Level::Info, "Report interval: %.1fm", val);
                if (manager_) manager_->reportState(true);
            });
        manager_->addComponent(report_interval_);

        // Fan
        fan_ = std::make_shared<ha::Fan>(*device_, "fan", "Fan",
            [this](bool state) {
                if (fan_cb_) fan_cb_(state, 0); // 0 = special value "keep current speed"
            },
            [this](uint8_t speed) {
                if (fan_cb_) fan_cb_(true, speed);
            });
        manager_->addComponent(fan_);

        // IP Sensor
        ip_sensor_ = std::make_shared<ha::Sensor>(*device_, "ip_address", "IP Address",
            "", "", "homeassistant", "diagnostic", "mdi:ip-network-outline");
        manager_->addComponent(ip_sensor_);
    }
};

} // namespace ha

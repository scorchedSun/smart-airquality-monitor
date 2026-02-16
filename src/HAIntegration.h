#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>

#include <map>
#include "HAMqtt.h"
#include "HomeAssistantManager.h"
#include "HAFan.h"
#include "HASwitch.h"
#include "HANumber.h"
#include "HASensor.h"
#include "Measurement.h" 

class HAIntegration {
public:
    using FanCallback = std::function<void(bool state, uint8_t speed)>;
    using DisplayCallback = std::function<void(bool state)>;
    using ConfigSaveCallback = std::function<void(const std::string& key, int value)>;

    HAIntegration(const std::string& device_prefix, const std::string& mac_id, 
                  const std::string& friendly_name, const std::string& app_version);

    void begin(std::shared_ptr<ha::MqttClient> mqtt_client);

    void setFanCallback(FanCallback cb);
    void setDisplayCallback(DisplayCallback cb);
    void setConfigSaveCallback(ConfigSaveCallback cb);

    void addSensor(MeasurementType type, std::shared_ptr<ha::Sensor> sensor);
    
    void report(const std::vector<std::unique_ptr<Measurement>>& measurements);

    void loop();

    void syncState(bool display_enabled, uint32_t display_interval_ms, 
                   uint32_t report_interval_s, uint8_t fan_speed, bool fan_on);
    
    void updateIpAddress(const std::string& ip);

    std::string getDeviceId() const;
    std::shared_ptr<ha::Device> getDevice() const;

private:
    std::shared_ptr<ha::Device> device;
    std::shared_ptr<ha::Manager> manager;
    
    std::shared_ptr<ha::Fan> fan;
    std::shared_ptr<ha::Switch> display_switch;
    std::shared_ptr<ha::Number> display_interval;
    std::shared_ptr<ha::Number> report_interval;
    std::shared_ptr<ha::Sensor> ip_sensor;

    std::map<MeasurementType, std::shared_ptr<ha::Sensor>> sensors;
    bool needs_report = false;

    FanCallback fan_cb;
    DisplayCallback display_cb;
    ConfigSaveCallback config_save_cb;

    std::vector<std::pair<MeasurementType, std::shared_ptr<ha::Sensor>>> pending_sensors;

    void setupControls();
};

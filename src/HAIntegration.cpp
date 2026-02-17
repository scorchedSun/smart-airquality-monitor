#include "HAIntegration.h"
#include "Logger.h"
#include "config_keys.h"

HAIntegration::HAIntegration(const std::string& device_prefix, const std::string& mac_id, 
                             const std::string& friendly_name, const std::string& app_version,
                             std::shared_ptr<ha::MqttClient> mqtt_client)
    : mqtt_client(mqtt_client) {
    device = std::make_shared<ha::Device>(device_prefix, mac_id, friendly_name, app_version);
    manager = std::make_shared<ha::Manager>(device, mqtt_client);
}

void HAIntegration::begin() {
    setupControls();
}

void HAIntegration::setFanCallback(FanCallback cb) {
    fan_cb = cb;
}

void HAIntegration::setDisplayCallback(DisplayCallback cb) {
    display_cb = cb;
}

void HAIntegration::setConfigSaveCallback(ConfigSaveCallback cb) {
    config_save_cb = cb;
}

void HAIntegration::setReconnectedCallback(ReconnectedCallback cb) {
    std::lock_guard<std::mutex> lock(integration_mutex);
    reconnected_cb = cb;
}

void HAIntegration::setupControls() {
    if (!manager) return;
    
    // Display Switch
    display_switch = std::make_shared<ha::Switch>(*device, "display_toggle", "Display Enabled",
        [this](bool state) {
            if (display_cb) display_cb(state);
            if (config_save_cb) config_save_cb(cfg::keys::enable_display, state);
            
            Logger::getInstance().log(Logger::Level::Info, "Display %s via MQTT", state ? "enabled" : "disabled");
            
            if (display_switch) display_switch->update_state(state);
            if (manager) manager->report_state(true);
        });
    manager->add_component(display_switch);

    // Display Interval
    display_interval = std::make_shared<ha::Number>(*device, "display_interval", "Display Interval (s)",
        5, 15, 5, [this](float val) {
            if (config_save_cb) config_save_cb(cfg::keys::display_interval, (int)val);
            
            Logger::getInstance().log(Logger::Level::Info, "Display interval: %.1fs", val);
            if (manager) manager->report_state(true);
        });
    manager->add_component(display_interval);

    // Report Interval
    report_interval = std::make_shared<ha::Number>(*device, "report_interval", "Report Interval (m)",
        1, 15, 1, [this](float val) {
            if (config_save_cb) config_save_cb(cfg::keys::report_interval, (int)val);
            
            Logger::getInstance().log(Logger::Level::Info, "Report interval: %.1fm", val);
            if (manager) manager->report_state(true);
        });
    manager->add_component(report_interval);

    // Fan
    fan = std::make_shared<ha::Fan>(*device, "fan", "Fan",
        [this](bool state) {
            if (fan_cb) fan_cb(state, 0); // 0 = special value "keep current speed"
        },
        [this](uint8_t speed) {
            if (fan_cb) fan_cb(true, speed);
        });
    manager->add_component(fan);

    // IP Sensor
    ip_sensor = std::make_shared<ha::Sensor>(*device, "ip_address", "IP Address",
        "", "", "homeassistant", "diagnostic", "mdi:ip-network-outline");
    manager->add_component(ip_sensor);
}

void HAIntegration::addSensor(MeasurementType type, const std::string& object_id, 
                               const std::string& name, const std::string& device_class, 
                               const std::string& unit) {
    std::lock_guard<std::mutex> lock(integration_mutex);
    if (!manager) return;
    
    auto sensor = std::make_shared<ha::Sensor>(*device, object_id, name, device_class, unit);
    manager->add_component(sensor);
    sensors[type] = sensor;
}

void HAIntegration::report(const std::vector<std::unique_ptr<Measurement>>& measurements) {
    std::lock_guard<std::mutex> lock(integration_mutex);
    bool updated = false;
    for (const auto& measurement : measurements) {
        MeasurementType type = measurement->get_details().get_type();
        
        auto it = sensors.find(type);
        if (it == sensors.end()) continue;

        const auto& sensor = it->second;
        sensor->update_state(measurement->value_to_string());
        updated = true;
    }

    if (updated) {
        needs_report = true;
    }
}

void HAIntegration::syncState(bool display_enabled, uint32_t display_interval_ms, 
                               uint32_t report_interval_s, uint8_t fan_speed, bool fan_on) {
    std::lock_guard<std::mutex> lock(integration_mutex);
    if (display_switch) display_switch->update_state(display_enabled);
    if (display_interval) display_interval->update_value(display_interval_ms / 1000.0f);
    if (report_interval) report_interval->update_value(report_interval_s / 60.0f);
    
    if (fan) {
        fan->update_state(fan_on);
        fan->update_speed(fan_speed);
    }
    
    if (manager) manager->report_state(true);
}

void HAIntegration::updateIpAddress(const std::string& ip) {
    std::lock_guard<std::mutex> lock(integration_mutex);
    if (ip_sensor) {
        ip_sensor->update_state(ip.c_str());
        if (manager) manager->report_state(true);
    }
}

std::string HAIntegration::getDeviceId() const {
    std::lock_guard<std::mutex> lock(integration_mutex);
    return device ? device->get_device_id() : "";
}

std::shared_ptr<ha::Device> HAIntegration::getDevice() const {
    std::lock_guard<std::mutex> lock(integration_mutex);
    return device;
}

void HAIntegration::loop() {
    std::unique_lock<std::mutex> lock(integration_mutex);
    
    if (mqtt_client && mqtt_client->isConnected()) {
        if (!last_connected_state) {
            last_connected_state = true;
            Logger::getInstance().log(Logger::Level::Info, "HAIntegration: MQTT Reconnected");
            if (reconnected_cb) {
                // Unlock before callback to avoid deadlocks if callback calls back into HAIntegration
                lock.unlock();
                reconnected_cb();
                lock.lock();
            }
        }
        
        if (manager) manager->report_state(); // Periodic report
        
        if (needs_report) {
            if (manager) manager->report_state(true);
            needs_report = false;
        }
    } else {
        last_connected_state = false;
    }
}



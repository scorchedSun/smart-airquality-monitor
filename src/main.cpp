#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <esp_task_wdt.h>

#include <Arduino.h>
#include "ArduinoJson.h"
#include "PubSubClient.h"
#include "SoftwareSerial.h"
#include "WiFi.h"
#include "driver/ledc.h"

#include "AppState.h"
#include "ConfigKeys.h"
#include "ConfigManager.h"
#include "WebConfig.h"

#include "BootAnimation.h"
#include "DHT20Wrapper.h"
#include "Display.h"
#include "ha/Integration.h"
#include "Logger.h"
#include "MHZ19Wrapper.h"
#include "Measurement.h"
#include "OtaManager.h"
#include "PMWrapper.h"
#include "PWMFan.h"
#include "ReconnectingPubSubClient.h"
#include "Translator.h"
#include "WifiManager.h"
#include <Update.h>

// ── Constants ──────────────────────────────────────────────────
static constexpr uint32_t mhz19_baud_rate = 9600;
static constexpr uint32_t fan_frequency_hz = 25000;
static constexpr std::string_view app_version = "1.1.0";
static constexpr std::string_view device_prefix = "smaq_";

// ── Application State ──────────────────────────────────────────
AppState app;

// ── Hardware ───────────────────────────────────────────────────
Display display(128, 64, OLED_DC, OLED_RESET, OLED_CS, app.i2c_mutex);
std::unique_ptr<PWMFan> fan = std::make_unique<PWMFan>(FAN_PIN, fan_frequency_hz);

// ── Networking ─────────────────────────────────────────────────
WifiManager wifi_manager;
WebConfig web_config;

// ── HA Integration ─────────────────────────────────────────────
std::shared_ptr<ReconnectingPubSubClient> reconnecting_mqtt_client;
std::unique_ptr<ha::Integration> ha_integration;

// ── Sensors ────────────────────────────────────────────────────
std::vector<std::unique_ptr<SensorDriver>> sensors;

// ── Managers ───────────────────────────────────────────────────
std::unique_ptr<OtaManager> ota_manager;
BootAnimation boot_animation(display);

// ── Forward Declarations ───────────────────────────────────────
void applyConfig();
void onWebConfigChanged();
void syncHaFromConfig();

// ═══════════════════════════════════════════════════════════════
//  Config
// ═══════════════════════════════════════════════════════════════

void applyConfig() {
    auto& cm = ConfigManager::getInstance();
    app.display_enabled.store(cm.getBool(cfg::keys::enable_display, cfg::defaults::enable_display));
    display.setEnabled(app.display_enabled.load());
    app.report_interval_in_seconds.store(cm.getInt(cfg::keys::report_interval, cfg::defaults::report_interval) * 60);
    app.display_each_measurement_for_in_millis.store(cm.getInt(cfg::keys::display_interval, cfg::defaults::display_interval) * 1000);
    app.fan_speed_percent.store(cm.getInt(cfg::keys::fan_speed, cfg::defaults::fan_speed));
    if (fan) fan->turnToPercent(app.fan_speed_percent.load());
}

void onWebConfigChanged() {
    applyConfig();    
    syncHaFromConfig();
}

void syncHaFromConfig() {
    if (ha_integration) {
        ha_integration->syncState(
            app.display_enabled.load(),
            app.display_each_measurement_for_in_millis.load(),
            app.report_interval_in_seconds.load(),
            app.fan_speed_percent.load(),
            app.fan_speed_percent.load() > 0
        );
    }
}

// ═══════════════════════════════════════════════════════════════
//  Home Assistant
// ═══════════════════════════════════════════════════════════════
void setupHa() {
    if (ha_integration) {
        ha_integration->setFanCallback([](bool state, uint8_t speed) {
            if (state) {
                if (speed == 0) speed = 20;
                if (fan) fan->turnToPercent(speed);
            } else {
                if (fan) fan->turnOff();
            }
            app.fan_speed_percent.store(speed);
        });

        ha_integration->setDisplayCallback([](bool state) {
            display.setEnabled(state);
            app.display_enabled.store(state);
        });

        ha_integration->setConfigSaveCallback([](const std::string& key, int value) {
            if (key == cfg::keys::enable_display) ConfigManager::getInstance().putBool(key.c_str(), (bool)value);
            else ConfigManager::getInstance().putInt(key.c_str(), value);
            
            if (key == cfg::keys::display_interval) app.display_each_measurement_for_in_millis.store(value * 1000);
            if (key == cfg::keys::report_interval) app.report_interval_in_seconds.store(value * 60);
        });

        ha_integration->begin();

        ha_integration->addSensor(MeasurementType::Temperature, "temp", "Temperature", "temperature", "°C");
        ha_integration->addSensor(MeasurementType::Humidity, "hum", "Humidity", "humidity", "%");
        ha_integration->addSensor(MeasurementType::CO2, "co2", "CO2", "carbon_dioxide", "ppm");
        ha_integration->addSensor(MeasurementType::PM1, "pm1", "PM1", "pm1", "µg/m³");
        ha_integration->addSensor(MeasurementType::PM25, "pm25", "PM2.5", "pm25", "µg/m³");
        ha_integration->addSensor(MeasurementType::PM10, "pm10", "PM10", "pm10", "µg/m³");

        ha_integration->setReconnectedCallback([]() {
            logger.log(Logger::Level::Info, "MQTT connection established, syncing HA state");
            
            if (ha_integration) {
                ha_integration->syncState(
                    app.display_enabled.load(),
                    app.display_each_measurement_for_in_millis.load(),
                    app.report_interval_in_seconds.load(),
                    app.fan_speed_percent.load(),
                    app.fan_speed_percent.load() > 0
                );
            }
        });
    }
}

// ═══════════════════════════════════════════════════════════════
//  MQTT
// ═══════════════════════════════════════════════════════════════

void setupMqtt(std::string_view mqtt_device_id, std::string_view lwt_topic, std::string_view lwt_payload) {
    auto& cm = ConfigManager::getInstance();
    std::string broker = cm.getString(cfg::keys::mqtt_broker, cfg::defaults::mqtt_broker);
    uint16_t port = cm.getInt(cfg::keys::mqtt_port, cfg::defaults::mqtt_port);
    std::string user = cm.getString(cfg::keys::mqtt_user, cfg::defaults::mqtt_user);
    std::string password = cm.getString(cfg::keys::mqtt_pass, cfg::defaults::mqtt_pass);

    app.mqtt_configured = !broker.empty() && port > 0;
    if (!app.mqtt_configured || !WiFi.isConnected()) {
        return;
    }

    reconnecting_mqtt_client = std::make_shared<ReconnectingPubSubClient>(
        broker.c_str(), port, user.c_str(), password.c_str(), mqtt_device_id,
        lwt_topic, lwt_payload, true, 0);
}

// ═══════════════════════════════════════════════════════════════
//  Sensors
// ═══════════════════════════════════════════════════════════════

// ── Sensor Names (for health reporting) ────────────────────────
static const char* sensor_names[] = { "DHT20", "MHZ19", "PMS5003" };
std::vector<bool> sensor_health;

void initializeSensors() {
    sensors.push_back(std::make_unique<DHT20Wrapper>(app.i2c_mutex));
    sensors.push_back(std::make_unique<MHZ19Wrapper>(MHZ19_RX, MHZ19_TX, mhz19_baud_rate));
    sensors.push_back(std::make_unique<PMWrapper>(PMS5003, PMS_TX, PMS_RX));

    sensor_health.resize(sensors.size(), false);

    for (size_t i = 0; i < sensors.size(); i++) {
        if (!sensors[i]->begin()) {
            logger.log(Logger::Level::Error, "Failed to initialize sensor: %s", sensor_names[i]);
            display.show("Sensor Error!");
            delay(2000);
        } else {
            sensor_health[i] = true;
        }
    }
}

void updateSensorHealthStatus() {
    if (!ha_integration) return;
    std::string health;
    for (size_t i = 0; i < sensors.size() && i < 3; i++) {
        if (i > 0) health += ", ";
        health += sensor_names[i];
        health += ": ";
        health += sensor_health[i] ? "OK" : "Error";
    }
    ha_integration->updateSensorHealth(health);
}

void sensorTask(void* parameter) {
    uint32_t last_sensor_read_millis = 0;

    for (;;) {
        esp_task_wdt_reset();

        if (app.ota_in_progress.load()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (app.is_setup) {
            uint32_t now = millis();
            uint32_t interval = app.report_interval_in_seconds.load() * 1000;

            if (now - last_sensor_read_millis >= interval || last_sensor_read_millis == 0) {
                std::vector<std::unique_ptr<Measurement>> new_measurements;
                for (size_t i = 0; i < sensors.size(); i++) {
                    bool ok = sensors[i]->provideMeasurements(new_measurements);
                    sensor_health[i] = ok;
                }

                {
                    std::lock_guard<std::mutex> lock(app.measurements_mutex);
                    app.measurements.clear();
                    for (auto& m : new_measurements) {
                        app.measurements.push_back(std::move(m));
                    }
                    app.current_display_index = 0;
                }

                updateSensorHealthStatus();
                last_sensor_read_millis = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ═══════════════════════════════════════════════════════════════
//  Setup & Loop
// ═══════════════════════════════════════════════════════════════

void setup() {
    Wire.begin();
    Serial.begin(115200);

    auto& cm = ConfigManager::getInstance();
    cm.begin();

    WiFi.mode(WIFI_STA);
    cm.buildMacId();

    if (cm.getBool(cfg::keys::enable_display, cfg::defaults::enable_display)) {
        display.setup();
        display.showBootStep("Initializing...", 0);
    }

    boot_animation.start();

    applyConfig();
    fan->begin(cm.getInt(cfg::keys::fan_speed, cfg::defaults::fan_speed));

    boot_animation.setMessage("Connecting WiFi...");
    bool wifi_connected = wifi_manager.connect();

    ota_manager = std::make_unique<OtaManager>(display, web_config, fan, app);

    web_config.setOnOtaStart([&]() {
        ota_manager->startSafeMode(false);
    });

    web_config.setOnOtaEnd([&]() {
        ota_manager->stopSafeMode(false);
        display.show("Update Done!");
    });

    web_config.begin([]() {
        logger.log(Logger::Level::Info, "Config changed, rebooting...");
        delay(500);
        ESP.restart();
    });

    if (!wifi_connected) {
        std::string hostName = cm.getHostName();
        if (display.getEnabled()) display.showBootStep("AP Mode", 1);
        wifi_manager.setupCaptivePortal(hostName);
        app.ip_address = wifi_manager.softAPIP();

        boot_animation.stop();

        std::string message = "WiFi failed. AP: " + hostName;
        display.show(message.c_str());
        return;
    }

    app.ip_address = wifi_manager.localIP();
    app.mac_id = cm.getMacId();
    display.setIpAddress(app.ip_address.toString().c_str());

    ota_manager->setup();

    logger.setupSerial(Logger::Level::Info);

    std::string syslog_ip = cm.getString(cfg::keys::syslog_server_ip, cfg::defaults::syslog_server_ip);
    if (!syslog_ip.empty()) {
        IPAddress syslog_addr;
        syslog_addr.fromString(syslog_ip.c_str());
        uint16_t syslog_port = cm.getInt(cfg::keys::syslog_server_port, cfg::defaults::syslog_server_port);
        logger.setupSyslog(syslog_addr, syslog_port, app.mac_id.c_str(), Logger::Level::Info);
    }

    std::string friendly_name = cm.getString(cfg::keys::friendly_name, cfg::defaults::friendly_name);
    std::string discovery_prefix = cm.getString(cfg::keys::ha_discovery_prefix, cfg::defaults::ha_discovery_prefix);

    auto device = std::make_shared<ha::Device>(device_prefix, app.mac_id.c_str(), friendly_name.c_str(), app_version);

    setupMqtt(app.mac_id.c_str(), device->getAvailabilityTopic(), device->getAvailabilityPayloadOffline()); 
    
    ha_integration = std::make_unique<ha::Integration>(device, reconnecting_mqtt_client, discovery_prefix);
    
    setupHa();

    boot_animation.setMessage("Sensors...");
    initializeSensors();

    boot_animation.stop();

    app.is_setup = true;

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

    xTaskCreatePinnedToCore(sensorTask, "SensorTask", 16384, NULL, 1, &app.sensor_task_handle, 0);

    logger.log(Logger::Level::Info, "Setup complete. IP: %s", app.ip_address.toString().c_str());
}

void loop() {
    static uint32_t last_stack_check = 0;
    if (millis() - last_stack_check > 1000) {
        last_stack_check = millis();
    }

    esp_task_wdt_reset();
    if (ota_manager) ota_manager->handle();

    if (app.ota_in_progress.load() || Update.isRunning()) {
        return;
    }

    if (!app.is_setup) return;

    uint32_t now = millis();

    if (reconnecting_mqtt_client) {
         reconnecting_mqtt_client->loop();
    }
    if (ha_integration && reconnecting_mqtt_client && reconnecting_mqtt_client->isConnected()) {
        ha_integration->loop();

        static IPAddress last_known_ip;
        if (last_known_ip != WiFi.localIP()) {
            last_known_ip = WiFi.localIP();
            app.ip_address = last_known_ip;
            display.setIpAddress(app.ip_address.toString().c_str());
            if (ha_integration) {
                ha_integration->updateIpAddress(app.ip_address.toString().c_str());
            }
        }
    }

    web_config.loop();

    display.setConnectivity(WiFi.isConnected(), reconnecting_mqtt_client ? reconnecting_mqtt_client->isConnected() : false);
    uint32_t disp_interval = app.display_each_measurement_for_in_millis.load();
    if (now - app.last_display_update_millis >= disp_interval || app.last_display_update_millis == 0) {
        std::lock_guard<std::mutex> lock(app.measurements_mutex);
        if (!app.measurements.empty()) {
            if (ha_integration) {
                ha_integration->report(app.measurements);
            }
            display.show(app.measurements[app.current_display_index]);
            app.current_display_index = (app.current_display_index + 1) % app.measurements.size();
            app.last_display_update_millis = now;
        }
    }

    delay(10);
}

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
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

#include "config_keys.h"
#include "ConfigManager.h"
#include "WebConfig.h"

#include "DHT20Wrapper.h"
#include "Display.h"
#include "HAIntegration.h"
#include "Logger.h"
#include "MHZ19Wrapper.h"
#include "Measurement.h"
#include "PMWrapper.h"
#include "PWMFan.h"
#include "ReconnectingPubSubClient.h"
#include "Translator.h"
#include "HAIntegration.h"
#include <ArduinoOTA.h>
#include <Update.h>

// ── Constants ──────────────────────────────────────────────────
static constexpr uint32_t mhz19_baud_rate = 9600;
static constexpr uint32_t fan_frequency_hz = 25000;
static const std::string app_version = "1.1.0";
static const std::string device_prefix = "smaq_";

// ── Shared State (atomic for cross-core access) ────────────────
std::atomic<uint32_t> report_interval_in_seconds{300};
std::atomic<uint32_t> display_each_measurement_for_in_millis{10000};
std::atomic<uint8_t>  fan_speed_percent{20};
std::atomic<bool>     display_enabled{true};
std::atomic<bool>     ota_in_progress{false};

// ── Hardware ───────────────────────────────────────────────────
std::mutex i2c_mutex;
Display display(128, 64, OLED_DC, OLED_RESET, OLED_CS, i2c_mutex);
std::unique_ptr<PWMFan> fan = std::make_unique<PWMFan>(FAN_PIN, fan_frequency_hz);

// ── Networking ─────────────────────────────────────────────────
WebConfig web_config;

// ── HA Integration ─────────────────────────────────────────────
std::shared_ptr<ReconnectingPubSubClient> reconnecting_mqtt_client;
std::unique_ptr<HAIntegration> ha_integration;

// ── Sensors / Measurements ─────────────────────────────────────
std::vector<std::unique_ptr<Sensor>> sensors;
std::vector<std::unique_ptr<Measurement>> measurements;
std::mutex measurements_mutex;

// ── Task Handles ───────────────────────────────────────────────
TaskHandle_t sensor_task_handle = nullptr;

// ── Timing ─────────────────────────────────────────────────────
uint32_t last_display_update_millis = 0;
size_t current_display_index = 0;

// ── Flags ──────────────────────────────────────────────────────
bool is_setup = false;
bool mqtt_configured = false;

IPAddress ip_address;
std::string mac_id;

// ── Forward Declarations ───────────────────────────────────────
void applyConfig();
void onWebConfigChanged();
void syncHAFromConfig();

// ═══════════════════════════════════════════════════════════════
//  Config
// ═══════════════════════════════════════════════════════════════

void applyConfig() {
    auto& cm = ConfigManager::getInstance();
    display_enabled.store(cm.getBool(cfg::keys::enable_display, cfg::defaults::enable_display));
    display.set_enabled(display_enabled.load());
    report_interval_in_seconds.store(cm.getInt(cfg::keys::report_interval, cfg::defaults::report_interval) * 60);
    display_each_measurement_for_in_millis.store(cm.getInt(cfg::keys::display_interval, cfg::defaults::display_interval) * 1000);
    fan_speed_percent.store(cm.getInt(cfg::keys::fan_speed, cfg::defaults::fan_speed));
    if (fan) fan->turn_to_percent(fan_speed_percent.load());
}

void onWebConfigChanged() {
    applyConfig();    
    syncHAFromConfig();
}

void syncHAFromConfig() {
    if (ha_integration) {
        ha_integration->syncState(
            display_enabled.load(),
            display_each_measurement_for_in_millis.load(),
            report_interval_in_seconds.load(),
            fan_speed_percent.load(),
            fan_speed_percent.load() > 0 // fan_on
        );
    }
}

// ═══════════════════════════════════════════════════════════════
//  WiFi
// ═══════════════════════════════════════════════════════════════

bool connectWifi() {
    auto& cm = ConfigManager::getInstance();
    std::string ssid = cm.getString(cfg::keys::wifi_ssid, cfg::defaults::wifi_ssid);
    std::string pass = cm.getString(cfg::keys::wifi_pass, cfg::defaults::wifi_pass);

    if (ssid.empty()) return false;

    WiFi.setHostname(cm.getHostName().c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
    }

    return WiFi.status() == WL_CONNECTED;
}

// ═══════════════════════════════════════════════════════════════
//  Home Assistant
// ═══════════════════════════════════════════════════════════════
void setup_ha() {
    if (ha_integration) {
        ha_integration->setFanCallback([](bool state, uint8_t speed) {
            if (state) {
                if (speed == 0) speed = 20; // Default if not specified
                if (fan) fan->turn_to_percent(speed);
            } else {
                if (fan) fan->turn_off();
            }
            fan_speed_percent.store(speed);
        });

        ha_integration->setDisplayCallback([](bool state) {
            display.set_enabled(state);
            display_enabled.store(state);
        });

        ha_integration->setConfigSaveCallback([](const std::string& key, int value) {
            if (key == cfg::keys::enable_display) ConfigManager::getInstance().putBool(key.c_str(), (bool)value);
            else ConfigManager::getInstance().putInt(key.c_str(), value);
            
            if (key == cfg::keys::display_interval) display_each_measurement_for_in_millis.store(value * 1000);
            if (key == cfg::keys::report_interval) report_interval_in_seconds.store(value * 60);
        });

        ha_integration->begin();

        // Register sensors
        ha_integration->addSensor(MeasurementType::Temperature, "temp", "Temperature", "temperature", "°C");
        ha_integration->addSensor(MeasurementType::Humidity, "hum", "Humidity", "humidity", "%");
        ha_integration->addSensor(MeasurementType::CO2, "co2", "CO2", "carbon_dioxide", "ppm");
        ha_integration->addSensor(MeasurementType::PM1, "pm1", "PM1", "pm1", "µg/m³");
        ha_integration->addSensor(MeasurementType::PM25, "pm25", "PM2.5", "pm25", "µg/m³");
        ha_integration->addSensor(MeasurementType::PM10, "pm10", "PM10", "pm10", "µg/m³");

        // synchronize state when we (re)connect to MQTT
        ha_integration->setReconnectedCallback([]() {
            logger.log(Logger::Level::Info, "MQTT connection established, syncing HA state");
            
            if (ha_integration) {
                ha_integration->syncState(
                    display_enabled.load(),
                    display_each_measurement_for_in_millis.load(),
                    report_interval_in_seconds.load(),
                    fan_speed_percent.load(),
                    fan_speed_percent.load() > 0
                );
            }
        });
    }
}

// ═══════════════════════════════════════════════════════════════
//  MQTT
// ═══════════════════════════════════════════════════════════════

void setup_mqtt(const std::string& mqtt_device_id) {
    auto& cm = ConfigManager::getInstance();
    std::string broker = cm.getString(cfg::keys::mqtt_broker, cfg::defaults::mqtt_broker);
    uint16_t port = cm.getInt(cfg::keys::mqtt_port, cfg::defaults::mqtt_port);
    std::string user = cm.getString(cfg::keys::mqtt_user, cfg::defaults::mqtt_user);
    std::string password = cm.getString(cfg::keys::mqtt_pass, cfg::defaults::mqtt_pass);

    mqtt_configured = !broker.empty() && port > 0;
    if (!mqtt_configured) {
        return;
    }
    if (!WiFi.isConnected()) {
        return;
    }

    reconnecting_mqtt_client = std::make_shared<ReconnectingPubSubClient>(
        broker, port, user, password, mqtt_device_id);
}

// ═══════════════════════════════════════════════════════════════
//  Sensors
// ═══════════════════════════════════════════════════════════════

void initialize_sensors() {
    sensors.push_back(std::make_unique<DHT20Wrapper>(i2c_mutex));
    sensors.push_back(std::make_unique<MHZ19Wrapper>(MHZ19_RX, MHZ19_TX, mhz19_baud_rate));
    sensors.push_back(std::make_unique<PMWrapper>(PMS5003, PMS_TX, PMS_RX));

    for (const auto& sensor : sensors) {
        if (!sensor->begin()) {
            logger.log(Logger::Level::Error, "Failed to initialize sensor");
            display.show("Sensor Error!");
            delay(2000);
        }
    }
}

void sensorTask(void* parameter) {
    uint32_t last_sensor_read_millis = 0;
    uint32_t last_heap_log = 0;

    for (;;) {
        esp_task_wdt_reset();

        if (ota_in_progress.load()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (is_setup) {
            uint32_t now = millis();

            uint32_t interval = report_interval_in_seconds.load() * 1000;
            if (now - last_sensor_read_millis >= interval || last_sensor_read_millis == 0) {
                std::vector<std::unique_ptr<Measurement>> new_measurements;
                for (const auto& sensor : sensors) {
                    sensor->provide_measurements(new_measurements);
                }

                {
                    std::lock_guard<std::mutex> lock(measurements_mutex);
                    measurements.clear();
                    for (auto& m : new_measurements) {
                        measurements.push_back(std::move(m));
                    }
                    current_display_index = 0;
                }

                last_sensor_read_millis = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ═══════════════════════════════════════════════════════════════
//  OTA
// ═══════════════════════════════════════════════════════════════

void start_ota_safe_mode() {
    if (ota_in_progress.load()) return;
    ota_in_progress.store(true);
    esp_task_wdt_delete(NULL);  // Remove loop task from WDT
    WiFi.setSleep(false);
    web_config.stop();
    delay(100);
    if (fan) fan->turn_off();
    if (sensor_task_handle && eTaskGetState(sensor_task_handle) != eSuspended) {
        vTaskSuspend(sensor_task_handle);
    }
    display.show("Updating...");
}

void stop_ota_safe_mode() {
    if (!ota_in_progress.load()) return;
    ota_in_progress.store(false);
    WiFi.setSleep(true);
    web_config.restart();
    esp_task_wdt_add(NULL);  // Re-add loop task to WDT
    if (sensor_task_handle && eTaskGetState(sensor_task_handle) == eSuspended) {
        vTaskResume(sensor_task_handle);
    }
}

void setup_ota() {
    ArduinoOTA.setHostname(ConfigManager::getInstance().getHostName().c_str());

    ArduinoOTA.onStart([]() {
        start_ota_safe_mode();
    });

    ArduinoOTA.onEnd([]() {
        display.show("OTA Done!");
        stop_ota_safe_mode();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int last_percent = -1;
        int percent = progress / (total / 100);
        if (percent % 10 == 0 && percent != last_percent) {
            last_percent = percent;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        display.show("OTA Error!");
        stop_ota_safe_mode();
    });

    ArduinoOTA.begin();
    logger.log(Logger::Level::Info, "OTA ready");
}

// ═══════════════════════════════════════════════════════════════
//  Boot Animation
// ═══════════════════════════════════════════════════════════════

std::atomic<bool> boot_anim_active{false};
std::atomic<const char*> boot_msg{"Booting..."};

void bootAnimTask(void* parameter) {
    int frame = 0;
    while (boot_anim_active.load()) {
        display.show_boot_step(boot_msg.load(), frame++);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════════
//  Setup & Loop
// ═══════════════════════════════════════════════════════════════

void setup() {
    Wire.begin();
    Serial.begin(115200);
    display.setup();

    // Initialize ConfigManager (Preferences / NVS)
    auto& cm = ConfigManager::getInstance();
    cm.begin();

    // WiFi mode must be set before buildMacId()
    WiFi.mode(WIFI_STA);
    cm.buildMacId();

    boot_anim_active.store(true);
    xTaskCreatePinnedToCore(bootAnimTask, "BootAnim", 2048, NULL, 1, NULL, 0);

    applyConfig();
    fan->begin(fan_speed_percent.load());

    boot_msg.store("Connecting WiFi...");
    bool wifi_connected = connectWifi();

    // Start web config (works in both STA and AP mode)
    web_config.begin(onWebConfigChanged);

    if (!wifi_connected) {
        std::string hostName = cm.getHostName();
        web_config.setupCaptivePortal(hostName);
        ip_address = WiFi.softAPIP();

        boot_anim_active.store(false);
        delay(150);

        std::string message = "WiFi failed. Arr" + hostName;
        display.show(message);
        return;
    }

    ip_address = WiFi.localIP();
    mac_id = cm.getMacId();
    display.set_ip_address(ip_address.toString().c_str());

    setup_ota();

    logger.setup_serial(Logger::Level::Debug);

    std::string syslog_ip = cm.getString(cfg::keys::syslog_server_ip, cfg::defaults::syslog_server_ip);
    if (!syslog_ip.empty()) {
        IPAddress syslog_addr;
        syslog_addr.fromString(syslog_ip.c_str());
        uint16_t syslog_port = cm.getInt(cfg::keys::syslog_server_port, cfg::defaults::syslog_server_port);
        logger.setup_syslog(syslog_addr, syslog_port, mac_id, Logger::Level::Debug);
    }

    std::string friendly_name = cm.getString(cfg::keys::friendly_name, cfg::defaults::friendly_name);
    
    setup_mqtt(mac_id); 

    // Initialize HA Integration
    ha_integration = std::make_unique<HAIntegration>(device_prefix, mac_id, friendly_name, app_version, reconnecting_mqtt_client);
    
    setup_ha();

    boot_msg.store("Sensors...");
    initialize_sensors();

    boot_anim_active.store(false);
    delay(150);

    is_setup = true;

    // Watchdog: 60s timeout
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

    xTaskCreatePinnedToCore(sensorTask, "SensorTask", 16384, NULL, 1, &sensor_task_handle, 0);

    logger.log(Logger::Level::Info, "Setup complete. IP: %s", ip_address.toString().c_str());
}

void loop() {
    static uint32_t last_stack_check = 0;
    if (millis() - last_stack_check > 1000) {
        last_stack_check = millis();
    }

    esp_task_wdt_reset();
    ArduinoOTA.handle();

    if (ota_in_progress.load() || Update.isRunning()) {
        return;
    }

    if (!is_setup) return;

    uint32_t now = millis();

    if (reconnecting_mqtt_client) {
         reconnecting_mqtt_client->loop();
    }
    if (ha_integration && reconnecting_mqtt_client && reconnecting_mqtt_client->isConnected()) {
        ha_integration->loop();

        static IPAddress last_known_ip;
        if (last_known_ip != WiFi.localIP()) {
            last_known_ip = WiFi.localIP();
            ip_address = last_known_ip;
            display.set_ip_address(ip_address.toString().c_str());
            if (ha_integration) {
                ha_integration->updateIpAddress(ip_address.toString().c_str());
            }
        }
    }

    display.set_connectivity(WiFi.isConnected(), reconnecting_mqtt_client ? reconnecting_mqtt_client->isConnected() : false);
    uint32_t disp_interval = display_each_measurement_for_in_millis.load();
    if (now - last_display_update_millis >= disp_interval || last_display_update_millis == 0) {
        std::lock_guard<std::mutex> lock(measurements_mutex);
        if (!measurements.empty()) {
            if (ha_integration) {
                ha_integration->report(measurements);
            }
            display.show(measurements[current_display_index]);
            current_display_index = (current_display_index + 1) % measurements.size();
            last_display_update_millis = now;
        }
    }

    delay(10); // Yield to other tasks
}

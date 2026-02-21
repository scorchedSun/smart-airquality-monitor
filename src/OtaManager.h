#pragma once

#include <atomic>
#include <functional>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <WiFi.h>

#include "AppState.h"
#include "ConfigManager.h"
#include "Display.h"
#include "WebConfig.h"
#include "PWMFan.h"
#include "Logger.h"

class OtaManager {
public:
    OtaManager(Display& display, WebConfig& web_config, 
               std::unique_ptr<PWMFan>& fan, AppState& state)
        : display_(display)
        , web_config_(web_config)
        , fan_(fan)
        , state_(state)
    {}

    void startSafeMode(bool stop_web) {
        if (state_.ota_in_progress.load()) return;
        state_.ota_in_progress.store(true);
        esp_task_wdt_delete(NULL);
        WiFi.setSleep(false);
        if (stop_web) web_config_.stop();
        delay(100);
        if (fan_) fan_->turnOff();
        if (state_.sensor_task_handle && eTaskGetState(state_.sensor_task_handle) != eSuspended) {
            vTaskSuspend(state_.sensor_task_handle);
        }
        display_.show("Updating...");
    }

    void stopSafeMode(bool restart_web) {
        if (!state_.ota_in_progress.load()) return;
        state_.ota_in_progress.store(false);
        WiFi.setSleep(true);
        if (restart_web) web_config_.restart();
        esp_task_wdt_add(NULL);
        if (state_.sensor_task_handle && eTaskGetState(state_.sensor_task_handle) == eSuspended) {
            vTaskResume(state_.sensor_task_handle);
        }
    }

    void setup() {
        ArduinoOTA.setHostname(ConfigManager::getInstance().getHostName().c_str());

        ArduinoOTA.onStart([this]() {
            startSafeMode(true);
        });

        ArduinoOTA.onEnd([this]() {
            display_.show("OTA Done!");
            stopSafeMode(true);
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            static int last_percent = -1;
            int percent = progress / (total / 100);
            if (percent % 10 == 0 && percent != last_percent) {
                last_percent = percent;
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            display_.show("OTA Error!");
            stopSafeMode(true);
        });

        ArduinoOTA.begin();
        logger.log(Logger::Level::Info, "OTA ready");
    }

    void handle() {
        ArduinoOTA.handle();
    }

private:
    Display& display_;
    WebConfig& web_config_;
    std::unique_ptr<PWMFan>& fan_;
    AppState& state_;
};

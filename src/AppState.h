#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <Arduino.h>
#include "Measurement.h"

struct AppState {
    // ── Cross-core atomics ─────────────────────────────────────
    std::atomic<uint32_t> report_interval_in_seconds{300};
    std::atomic<uint32_t> display_each_measurement_for_in_millis{10000};
    std::atomic<uint8_t>  fan_speed_percent{20};
    std::atomic<bool>     display_enabled{true};
    std::atomic<bool>     ota_in_progress{false};

    // ── Shared hardware mutex ──────────────────────────────────
    std::mutex i2c_mutex;

    // ── Sensor data ────────────────────────────────────────────
    std::vector<std::unique_ptr<Measurement>> measurements;
    std::mutex measurements_mutex;

    // ── Runtime state ──────────────────────────────────────────
    bool is_setup = false;
    bool mqtt_configured = false;
    IPAddress ip_address;
    std::string mac_id;

    // ── Display timing ─────────────────────────────────────────
    uint32_t last_display_update_millis = 0;
    size_t current_display_index = 0;

    // ── Task handles ───────────────────────────────────────────
    TaskHandle_t sensor_task_handle = nullptr;
};

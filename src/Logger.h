#pragma once

#include <memory>
#include <format>
#include <unordered_map>
#include <mutex>

#include "IPAddress.h"
#include "WiFiUdp.h"
#include <WiFi.h>

class Logger {

public:
    enum class Level { Error, Warning, Info, Debug };

    static constexpr bool try_get_level_by_name(std::string_view name, Level& level) {
        static const std::unordered_map<std::string_view, Level> name_to_level = { 
            { "Error", Level::Error }, 
            {"Warning", Level::Warning}, 
            {"Info", Level::Info},
            {"Debug", Level::Debug} 
        };

        auto it = name_to_level.find(name);
        if (it == name_to_level.cend()) {
            return false;
        }

        level = it->second;
        return true;
    }

private:
    Logger() = default;

    bool serial_enabled = false;
    Level serial_level = Level::Info;

    bool syslog_enabled = false;
    Level syslog_level = Level::Info;
    std::string syslog_host;
    uint16_t syslog_port;
    std::string device_id;

    WiFiUDP udp;
    std::mutex mutex;
    
public:   
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setup_serial(const Level level) {
        serial_enabled = true;
        serial_level = level;
    }

    void setup_syslog(const IPAddress& host, const uint16_t port, const std::string& mac_id, const Level level) {
        syslog_enabled = true;
        syslog_host = std::string(host.toString().c_str());
        syslog_port = port;
        syslog_level = level;
        device_id = mac_id;
    }

    // Simplified log for stability
    template <typename... Args>
    void log(const Level level, const char* format, Args ...args) {
        if (!serial_enabled || level > serial_level) return;

        static const char* log_level_strings[] = { "ERROR", "WARN", "INFO", "DEBUG" };
        const char* level_name = log_level_strings[static_cast<uint8_t>(level)];
        
        std::lock_guard<std::mutex> lock(mutex);
        Serial.printf("[%s] ", level_name);
        Serial.printf(format, args...);
        Serial.println();
    }
    
};

inline Logger& logger = Logger::getInstance();
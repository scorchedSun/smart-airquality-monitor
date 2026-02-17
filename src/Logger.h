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

    static constexpr bool tryGetLevelByName(std::string_view name, Level& level) {
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

    bool serial_enabled_ = false;
    Level serial_level_ = Level::Info;

    bool syslog_enabled_ = false;
    Level syslog_level_ = Level::Info;
    std::string syslog_host_;
    uint16_t syslog_port_;
    std::string device_id_;

    WiFiUDP udp_;
    std::mutex logger_mutex_;
    
public:   
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setupSerial(const Level level) {
        serial_enabled_ = true;
        serial_level_ = level;
    }

    void setupSyslog(const IPAddress& host, const uint16_t port, const std::string& mac_id, const Level level) {
        syslog_enabled_ = true;
        syslog_host_ = std::string(host.toString().c_str());
        syslog_port_ = port;
        syslog_level_ = level;
        device_id_ = mac_id;
    }

    // Simplified log for stability
    template <typename... Args>
    void log(const Level level, const char* format, Args ...args) {
        if (!serial_enabled_ || level > serial_level_) return;

        static const char* log_level_strings[] = { "ERROR", "WARN", "INFO", "DEBUG" };
        const char* level_name = log_level_strings[static_cast<uint8_t>(level)];
        
        std::lock_guard<std::mutex> lock(logger_mutex_);
        Serial.printf("[%s] ", level_name);
        Serial.printf(format, args...);
        Serial.println();
    }
    
};

inline Logger& logger = Logger::getInstance();
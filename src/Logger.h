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

    static bool tryGetLevelByName(std::string_view name, Level& level) {
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

    void setupSyslog(const IPAddress& host, const uint16_t port, std::string_view mac_id, const Level level) {
        syslog_enabled_ = true;
        syslog_host_ = std::string(host.toString().c_str());
        syslog_port_ = port;
        syslog_level_ = level;
        device_id_ = std::string{mac_id};
    }

    // Simplified log for stability
    template <typename... Args>
    void log(const Level level, const char* format, Args ...args) {
        bool should_serial = serial_enabled_ && level <= serial_level_;
        bool should_syslog = syslog_enabled_ && level <= syslog_level_ && WiFi.isConnected();

        if (!should_serial && !should_syslog) return;

        static constexpr const char* log_level_strings[] = { "ERROR", "WARN", "INFO", "DEBUG" };
        const char* level_name = log_level_strings[static_cast<uint8_t>(level)];
        
        // Format the message once
        char msg_buf[256];
        snprintf(msg_buf, sizeof(msg_buf), format, args...);

        std::lock_guard<std::mutex> lock(logger_mutex_);
        
        if (should_serial) {
            Serial.printf("[%s] ", level_name);
            Serial.println(msg_buf);
        }

        if (should_syslog) {
            // RFC 3164 syslog: <priority>message
            // facility=1 (user), severity: 3=error, 4=warning, 6=info, 7=debug
            static constexpr uint8_t syslog_severities[] = { 3, 4, 6, 7 };
            uint8_t priority = (1 << 3) | syslog_severities[static_cast<uint8_t>(level)];
            
            IPAddress syslog_ip;
            syslog_ip.fromString(syslog_host_.c_str());
            udp_.beginPacket(syslog_ip, syslog_port_);
            udp_.printf("<%d>%s %s: %s", priority, device_id_.c_str(), level_name, msg_buf);
            udp_.endPacket();
        }
    }
    
};

inline Logger& logger = Logger::getInstance();
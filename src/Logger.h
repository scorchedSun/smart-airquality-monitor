#pragma once

#include <memory>
#include <format>
#include <unordered_map>

#include "IPAddress.h"
#include "WiFiUdp.h"

class Logger {

public:
    enum class Level { Error, Warning, Debug };

    static constexpr bool try_get_level_by_name(std::string_view name, Level& level) {
        static const std::unordered_map<std::string_view, Level> name_to_level = { { "Error", Level::Error }, {"Warning", Level::Warning}, {"Debug", Level::Debug} };

        auto it = name_to_level.find(name);
        if (it == name_to_level.cend()) {
            return false;
        }

        level = it->second;
        return true;
    }

private:
    Logger() = default;

    bool serial_enabled;
    Level serial_level;

    bool syslog_enabled;
    Level syslog_level;
    std::string syslog_host;
    uint16_t syslog_port;
    std::string device_id;

    WiFiUDP udp;
    
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

    template <typename... Args>
    constexpr void log(const Level level, std::string_view message, Args ...args) {
        static const char* log_level_strings[] = { "ERROR", "WARN", "DEBUG" };

        const char* level_name = log_level_strings[static_cast<uint8_t>(level)];

        std::string formatted = std::vformat(message, std::make_format_args(args...));

        if (serial_enabled && level <= serial_level) {
            Serial.printf("[%s] %s\r\n", level_name, formatted.c_str());
        }
        if (syslog_enabled && WiFi.isConnected() && level <= syslog_level) {
            udp.beginPacket(syslog_host.c_str(), syslog_port);

            std::string syslog_message = std::format("[{}] [{}] {}", device_id, level_name, formatted);
            udp.println(syslog_message.c_str());

            udp.endPacket();
        }
    }
    
};

extern Logger& logger = Logger::getInstance();
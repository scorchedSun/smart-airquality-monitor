#pragma once

#include <cstdint>
#include <string>

namespace cfg {

namespace keys {
    constexpr const char* wifi_ssid         = "wifi_ssid";
    constexpr const char* wifi_pass         = "wifi_pass";
    constexpr const char* mqtt_broker       = "mqtt_broker";
    constexpr const char* mqtt_port         = "mqtt_port";
    constexpr const char* mqtt_user         = "mqtt_user";
    constexpr const char* mqtt_pass         = "mqtt_pass";
    constexpr const char* friendly_name     = "friendly_name";
    constexpr const char* host_name         = "host_name";
    constexpr const char* report_interval   = "report_interval";
    constexpr const char* display_interval  = "display_interval";
    constexpr const char* enable_display    = "enable_display";
    constexpr const char* fan_speed         = "fan_speed";
    constexpr const char* syslog_server_ip  = "syslog_ip";
    constexpr const char* syslog_server_port = "syslog_port";
    constexpr const char* log_level         = "log_level";
}

namespace defaults {
    constexpr const char* wifi_ssid         = "";
    constexpr const char* wifi_pass         = "";
    constexpr const char* mqtt_broker       = "";
    constexpr uint16_t    mqtt_port         = 1883;
    constexpr const char* mqtt_user         = "";
    constexpr const char* mqtt_pass         = "";
    constexpr const char* friendly_name     = "Smart Air Quality Monitor";
    constexpr const char* host_name         = "smaq";
    constexpr uint32_t    report_interval   = 5;   // minutes
    constexpr uint32_t    display_interval  = 10;  // seconds
    constexpr bool        enable_display    = true;
    constexpr uint8_t     fan_speed         = 20;  // percent
    constexpr const char* syslog_server_ip  = "";
    constexpr uint16_t    syslog_server_port = 514;
    constexpr const char* log_level         = "Error";
}

}

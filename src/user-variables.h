#pragma once

const char* VARIABLES_DEF_YAML = R"~(
Wifi settings:
    - st_ssid:
        label: Name of WiFi to connect to
        default: ""
    - st_pass:
        label: WiFi password
        default: ""

MQTT settings:
    - mqtt_broker:
        label: The MQTT broker
        default: ""
    - mqtt_port:
        label: Port on the broker to reach MQTT
        default: 1883
    - mqtt_user:
        label: User to connect to MQTT
        default: ""
    - mqtt_pass:
        label: Password of mqtt_user
        default: ""

Device settings:
    - friendly_name: 
        label: Friendly name of the device
        default: "Smart Air Quality Monitor"
    - report_interval:
        label: Interval in which new values will be reported (in minutes)
        range: 1, 15, 1
        default: 5
    - display_interval:
        label: Interval in which to switch between display values (in seconds)
        range: 5, 15, 5
        default: 10
    - enable_display:
        label: Turn off if you don't want to show anything on the display (f.e. at night)
        checked: True

Logging settings:
    - syslog_server_ip:
        label: IP address of the logging server. Leave blank to disable logging to a server
        default: ""
    - syslog_server_port:
        label: Port of the logging server
        default: 514
    - log_level:
        label: Maximum level of log messages to report 
        options:
            - Error: 0
            - Warning: 1
            - Debug: 2
        default: Error
)~";
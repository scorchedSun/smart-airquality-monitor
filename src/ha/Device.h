#pragma once

#include <string>
#include <ArduinoJson.h>

namespace ha {

class Device {
private:
    const std::string mac_id;
    const std::string device_name;
    const std::string software_version;
    const std::string device_id;
    const std::string device_prefix;
    DynamicJsonDocument device_json;

public:
    Device(const std::string device_prefix,
             const std::string mac_id,
             const std::string device_name,
             const std::string software_version)
        : device_prefix(device_prefix)
        , mac_id(mac_id)
        , device_name(device_name)
        , software_version(software_version)
        , device_id(std::string(device_prefix).append(mac_id))
        , device_json(512)
    {
        JsonArray identifiers(device_json.createNestedArray("ids"));
        identifiers.add(device_id);
        device_json["name"] = device_name;
        device_json["sw"] = software_version;
    }

    std::string get_device_id() const {
        return device_id;
    }

    const DynamicJsonDocument& get_device_info_json() const {
        return device_json;
    }
    
    std::string get_mac_id() const {
        return mac_id;
    }
};

} // namespace ha

#pragma once

#include <string>
#include <ArduinoJson.h>

namespace ha {

class Device {
private:
    const std::string mac_id_;
    const std::string device_name_;
    const std::string software_version_;
    const std::string device_id_;
    const std::string device_prefix_;
    DynamicJsonDocument device_json_;

public:
    Device(const std::string device_prefix,
             const std::string mac_id,
             const std::string device_name,
             const std::string software_version)
        : mac_id_(mac_id)
        , device_name_(device_name)
        , software_version_(software_version)
        , device_id_(std::string(device_prefix).append(mac_id))
        , device_prefix_(device_prefix)
        , device_json_(512)
    {
        JsonArray identifiers(device_json_.createNestedArray("ids"));
        identifiers.add(device_id_);
        device_json_["name"] = device_name_;
        device_json_["sw"] = software_version_;
    }

    std::string getDeviceId() const {
        return device_id_;
    }

    std::string getAvailabilityTopic() const {
        return device_prefix_ + mac_id_ + "/status";
    }

    std::string getAvailabilityPayloadOnline() const {
        return "online";
    }

    std::string getAvailabilityPayloadOffline() const {
        return "offline";
    }

    const DynamicJsonDocument& getDeviceInfoJson() const {
        return device_json_;
    }
    
    std::string getMacId() const {
        return mac_id_;
    }
};

} // namespace ha

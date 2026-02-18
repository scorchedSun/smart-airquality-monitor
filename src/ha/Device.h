#pragma once

#include <string_view>
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
    Device(std::string_view device_prefix,
             std::string_view mac_id,
             std::string_view device_name,
             std::string_view software_version)
        : mac_id_(mac_id)
        , device_name_(device_name)
        , software_version_(software_version)
        , device_id_(std::string(device_prefix.data(), device_prefix.length()) + std::string(mac_id.data(), mac_id.length()))
        , device_prefix_(std::string(device_prefix.data(), device_prefix.length()))
        , availability_topic_(std::string(device_prefix.data(), device_prefix.length()) + std::string(mac_id.data(), mac_id.length()) + "/status")
        , device_json_(512)
    {
        JsonArray identifiers(device_json_.createNestedArray("ids"));
        identifiers.add(device_id_);
        device_json_["name"] = device_name_;
        device_json_["sw"] = software_version_;
    }

    std::string_view getDeviceId() const {
        return device_id_;
    }

    std::string_view getAvailabilityTopic() const {
        return availability_topic_;
    }

    constexpr std::string_view getAvailabilityPayloadOnline() const {
        return "online";
    }

    constexpr std::string_view getAvailabilityPayloadOffline() const {
        return "offline";
    }

    const DynamicJsonDocument& getDeviceInfoJson() const {
        return device_json_;
    }
    
    std::string_view getMacId() const {
        return mac_id_;
    }
    
private:
    const std::string availability_topic_;
};

} // namespace ha

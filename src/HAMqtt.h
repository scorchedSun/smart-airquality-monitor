#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <cstddef>

namespace ha {

class MqttClient {
public:
    using MessageCallback = std::function<void(const char* topic, const uint8_t* payload, size_t length)>;

    virtual ~MqttClient() = default;

    virtual bool publish(const std::string& topic, const std::string& payload, bool retain = false) = 0;
    virtual void subscribe(const std::string& topic) = 0;
    virtual void set_callback(MessageCallback callback) = 0;
    virtual bool isConnected() const = 0;
};

} // namespace ha

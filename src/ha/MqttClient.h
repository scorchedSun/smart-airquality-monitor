#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <cstddef>

#include <string_view>

namespace ha {

class MqttClient {
public:
    using MessageCallback = std::function<void(std::string_view topic, const uint8_t* payload, size_t length)>;

    virtual ~MqttClient() = default;

    virtual bool publish(std::string_view topic, std::string_view payload, bool retain = false) = 0;
    virtual void subscribe(const std::string& topic) = 0;
    virtual void setCallback(MessageCallback callback) = 0;
    virtual bool isConnected() const = 0;
};

} // namespace ha

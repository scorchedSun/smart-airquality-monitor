#pragma once
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>
#include <memory>
#include "ArduinoJson.h"
#include "Logger.h"
#include <PubSubClient.h>
#include <functional>
#include <vector>
#include <algorithm>

class ReconnectingPubSubClient {
public:
    using MessageCallback = std::function<void(char*, uint8_t*, unsigned int)>;

    enum class Error { None, ReconnectFailed, PublishFailed };

private:
    WiFiClient wifi_client;
    PubSubClient mqtt_client;
    const std::string broker;
    const uint16_t port;
    const std::string mqtt_user;
    const std::string mqtt_password;
    const std::string client_id;

    uint32_t last_connection_attempt_timestamp = 0;
    uint32_t current_backoff_ms = 1000;
    static constexpr uint32_t min_backoff_ms = 1000;
    static constexpr uint32_t max_backoff_ms = 60000;

    std::vector<std::string> subscribed_topics;
    MessageCallback message_callback;
    
    using ConnectionCallback = std::function<void()>;
    ConnectionCallback on_connect_callback;

    bool establish_connection_to_broker() {
        bool is_conn = mqtt_client.connected();
        
        if (is_conn) {
            current_backoff_ms = min_backoff_ms;
            return true;
        }

        const uint32_t now = millis();
        if (now - last_connection_attempt_timestamp < current_backoff_ms &&
            last_connection_attempt_timestamp != 0) {
            return false;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }

        last_connection_attempt_timestamp = now;

        wifi_client.stop(); // Ensure clean state 
        delay(100);

        const char* user_ptr = mqtt_user.empty() ? nullptr : mqtt_user.c_str();
        const char* pass_ptr = mqtt_password.empty() ? nullptr : mqtt_password.c_str();

        if (!mqtt_client.connect(client_id.c_str(), user_ptr, pass_ptr)) {
            int state = mqtt_client.state();
            logger.log(Logger::Level::Warning, "MQTT connect failed (state %d), retry in %ums",
                       state, current_backoff_ms);
            current_backoff_ms = std::min(current_backoff_ms * 2, max_backoff_ms);
            return false;
        }

        logger.log(Logger::Level::Info, "MQTT connected");
        current_backoff_ms = min_backoff_ms;

        for (const auto& topic : subscribed_topics) {
            mqtt_client.subscribe(topic.c_str());
        }

        if (on_connect_callback) {
            on_connect_callback();
        }

        return true;
    }

public:
public:
    ReconnectingPubSubClient(const std::string& broker,
                             uint16_t port,
                             const std::string& mqtt_user,
                             const std::string& mqtt_password,
                             const std::string& client_id)
        : mqtt_client(wifi_client)
        , broker(broker)
        , port(port)
        , mqtt_user(mqtt_user)
        , mqtt_password(mqtt_password)
        , client_id(client_id)
    {
        mqtt_client.setServer(broker.c_str(), port);
        mqtt_client.setBufferSize(2048);
    }

    void set_callback(MessageCallback callback) {
        this->message_callback = callback;
        mqtt_client.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
            if (this->message_callback) {
                this->message_callback(topic, payload, length);
            }
        });
    }

    void set_on_connect_callback(ConnectionCallback callback) {
        this->on_connect_callback = callback;
    }

    void subscribe(const std::string& topic) {
        subscribed_topics.push_back(topic);
        if (mqtt_client.connected()) {
            mqtt_client.subscribe(topic.c_str());
        }
    }

    void loop() {
        if (establish_connection_to_broker()) {
            mqtt_client.loop();
        }
    }

    bool is_connected() {
        return mqtt_client.connected();
    }

    void disconnect() {
        mqtt_client.disconnect();
    }

    Error publish(const std::string& topic, const std::string& payload, bool retain = false) {
        if (mqtt_client.connected()) {
            if (!mqtt_client.publish(topic.c_str(), payload.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           mqtt_client.getWriteError());
                return Error::PublishFailed;
            }
            return Error::None;
        }
        return Error::ReconnectFailed;
    }

    Error publish(const std::string& topic, const JsonDocument& data, bool retain = false) {
        if (mqtt_client.connected()) {
            std::string buffer;
            serializeJson(data, buffer);

            if (!mqtt_client.publish(topic.c_str(), buffer.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           mqtt_client.getWriteError());
                return Error::PublishFailed;
            }

            return Error::None;
        }

        return Error::ReconnectFailed;
    }
};

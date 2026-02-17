#pragma once
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>
#include <memory>
#include <mutex>
#include "ArduinoJson.h"
#include "Logger.h"
#include <PubSubClient.h>
#include <functional>
#include <vector>
#include <algorithm>

#include "HAMqtt.h"

class ReconnectingPubSubClient : public ha::MqttClient {
public:
    using MessageCallback = std::function<void(char*, uint8_t*, unsigned int)>;

    enum class Error { None, ReconnectFailed, PublishFailed };

private:
    WiFiClient wifi_client;
    mutable PubSubClient pubsub_client;
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
    
    mutable std::recursive_mutex mqtt_mutex;

    bool establish_connection_to_broker() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        
        if (pubsub_client.connected()) {
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

        const char* user_ptr = mqtt_user.empty() ? nullptr : mqtt_user.c_str();
        const char* pass_ptr = mqtt_password.empty() ? nullptr : mqtt_password.c_str();

        // Ensure we're using the correct address type
        IPAddress broker_ip;
        if (broker_ip.fromString(broker.c_str())) {
             pubsub_client.setServer(broker_ip, port);
        } else {
             pubsub_client.setServer(broker.c_str(), port);
        }

        logger.log(Logger::Level::Info, "Attempting MQTT connection to %s:%d as %s", 
                   broker.c_str(), port, client_id.c_str());

        if (!pubsub_client.connect(client_id.c_str(), user_ptr, pass_ptr)) {
            int state = pubsub_client.state();
            logger.log(Logger::Level::Warning, "MQTT connect failed (state %d), retry in %ums",
                       state, current_backoff_ms);
            
            // Explicitly stop the client on failure to clear the socket
            wifi_client.stop();

            current_backoff_ms = std::min(current_backoff_ms * 2, max_backoff_ms);
            return false;
        }

        logger.log(Logger::Level::Info, "MQTT connected");
        current_backoff_ms = min_backoff_ms;

        for (const auto& topic : subscribed_topics) {
            pubsub_client.subscribe(topic.c_str());
        }

        return true;
    }

public:
    ReconnectingPubSubClient(const std::string& broker,
                             uint16_t port,
                             const std::string& mqtt_user,
                             const std::string& mqtt_password,
                             const std::string& client_id)
        : pubsub_client(wifi_client)
        , broker(broker)
        , port(port)
        , mqtt_user(mqtt_user)
        , mqtt_password(mqtt_password)
        , client_id(client_id)
    {
        pubsub_client.setBufferSize(2048);
    }

    void set_callback(ha::MqttClient::MessageCallback callback) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        pubsub_client.setCallback([this, callback](char* topic, uint8_t* payload, unsigned int length) {
            if (callback) {
                callback(topic, payload, length);
            }
        });
    }

    void subscribe(const std::string& topic) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        auto it = std::find(subscribed_topics.begin(), subscribed_topics.end(), topic);
        if (it == subscribed_topics.end()) {
            subscribed_topics.push_back(topic);
        }
        if (pubsub_client.connected()) {
            pubsub_client.subscribe(topic.c_str());
        }
    }

    void loop() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        if (establish_connection_to_broker()) {
            pubsub_client.loop();
        }
    }

    bool isConnected() const override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        return pubsub_client.connected();
    }

    void disconnect() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        pubsub_client.disconnect();
    }

    bool publish(const std::string& topic, const std::string& payload, bool retain = false) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        if (pubsub_client.connected()) {
            if (!pubsub_client.publish(topic.c_str(), payload.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           pubsub_client.getWriteError());
                return false;
            }
            return true;
        }
        return false;
    }

    Error publishJson(const std::string& topic, const JsonDocument& data, bool retain = false) {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex);
        if (pubsub_client.connected()) {
            std::string buffer;
            serializeJson(data, buffer);

            if (!pubsub_client.publish(topic.c_str(), buffer.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           pubsub_client.getWriteError());
                return Error::PublishFailed;
            }

            return Error::None;
        }

        return Error::ReconnectFailed;
    }
};

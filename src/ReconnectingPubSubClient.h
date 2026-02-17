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

#include "ha/MqttClient.h"

class ReconnectingPubSubClient : public ha::MqttClient {
public:
    using MessageCallback = std::function<void(char*, uint8_t*, unsigned int)>;

    enum class Error { None, ReconnectFailed, PublishFailed };

private:
    WiFiClient wifi_client_;
    mutable PubSubClient pubsub_client_;
    const std::string broker_;
    const uint16_t port_;
    const std::string mqtt_user_;
    const std::string mqtt_password_;
    const std::string client_id_;

    uint32_t last_connection_attempt_timestamp_ = 0;
    uint32_t current_backoff_ms_ = 1000;

    static constexpr uint32_t min_backoff_ms = 1000;
    static constexpr uint32_t max_backoff_ms = 60000;

    std::vector<std::string> subscribed_topics_;
    
    mutable std::recursive_mutex mqtt_mutex_;

    bool establishConnectionToBroker() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        
        if (pubsub_client_.connected()) {
            current_backoff_ms_ = min_backoff_ms;
            return true;
        }

        const uint32_t now = millis();
        if (now - last_connection_attempt_timestamp_ < current_backoff_ms_ &&
            last_connection_attempt_timestamp_ != 0) {
            return false;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }

        last_connection_attempt_timestamp_ = now;

        const char* user_ptr = mqtt_user_.empty() ? nullptr : mqtt_user_.c_str();
        const char* pass_ptr = mqtt_password_.empty() ? nullptr : mqtt_password_.c_str();

        // Ensure we're using the correct address type
        IPAddress broker_ip;
        if (broker_ip.fromString(broker_.c_str())) {
             pubsub_client_.setServer(broker_ip, port_);
        } else {
             pubsub_client_.setServer(broker_.c_str(), port_);
        }

        logger.log(Logger::Level::Info, "Attempting MQTT connection to %s:%d as %s", 
                   broker_.c_str(), port_, client_id_.c_str());

        if (!pubsub_client_.connect(client_id_.c_str(), user_ptr, pass_ptr)) {
            int state = pubsub_client_.state();
            logger.log(Logger::Level::Warning, "MQTT connect failed (state %d), retry in %ums",
                       state, current_backoff_ms_);
            
            // Explicitly stop the client on failure to clear the socket
            wifi_client_.stop();

            current_backoff_ms_ = std::min(current_backoff_ms_ * 2, max_backoff_ms);
            return false;
        }

        logger.log(Logger::Level::Info, "MQTT connected");
        current_backoff_ms_ = min_backoff_ms;

        for (const auto& topic : subscribed_topics_) {
            pubsub_client_.subscribe(topic.c_str());
        }

        return true;
    }

public:
    ReconnectingPubSubClient(const std::string& broker,
                             uint16_t port,
                             const std::string& mqtt_user,
                             const std::string& mqtt_password,
                             const std::string& client_id)
        : pubsub_client_(wifi_client_)
        , broker_(broker)
        , port_(port)
        , mqtt_user_(mqtt_user)
        , mqtt_password_(mqtt_password)
        , client_id_(client_id)
    {
        pubsub_client_.setBufferSize(2048);
    }

    void setCallback(ha::MqttClient::MessageCallback callback) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        pubsub_client_.setCallback([this, callback](char* topic, uint8_t* payload, unsigned int length) {
            if (callback) {
                callback(topic, payload, length);
            }
        });
    }

    void subscribe(const std::string& topic) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        auto it = std::find(subscribed_topics_.begin(), subscribed_topics_.end(), topic);
        if (it == subscribed_topics_.end()) {
            subscribed_topics_.push_back(topic);
        }
        if (pubsub_client_.connected()) {
            pubsub_client_.subscribe(topic.c_str());
        }
    }

    void loop() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        if (establishConnectionToBroker()) {
            pubsub_client_.loop();
        }
    }

    bool isConnected() const override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        return pubsub_client_.connected();
    }

    void disconnect() {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        pubsub_client_.disconnect();
    }

    bool publish(const std::string& topic, const std::string& payload, bool retain = false) override {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        if (pubsub_client_.connected()) {
            if (!pubsub_client_.publish(topic.c_str(), payload.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           pubsub_client_.getWriteError());
                return false;
            }
            return true;
        }
        return false;
    }

    Error publishJson(const std::string& topic, const JsonDocument& data, bool retain = false) {
        std::lock_guard<std::recursive_mutex> lock(mqtt_mutex_);
        if (pubsub_client_.connected()) {
            std::string buffer;
            serializeJson(data, buffer);

            if (!pubsub_client_.publish(topic.c_str(), buffer.c_str(), retain)) {
                logger.log(Logger::Level::Warning, "MQTT publish failed: %d",
                           pubsub_client_.getWriteError());
                return Error::PublishFailed;
            }

            return Error::None;
        }

        return Error::ReconnectFailed;
    }
};

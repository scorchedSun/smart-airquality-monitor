#pragma once

#include <ArduinoJson.h>
#include "Logger.h"

#include "ValueReporter.h"
#include "ReconnectingPubSubClient.h"

class MQTTReporter : public ValueReporter {
    
    private:
        const std::shared_ptr<ReconnectingPubSubClient> mqtt_client;
        const std::string topic;

    public:
        MQTTReporter(const std::shared_ptr<ReconnectingPubSubClient> mqtt_client,
                     const std::string& topic)
            : ValueReporter()
            , mqtt_client(mqtt_client)
            , topic(topic) {
        }

        void report(const JsonDocument& data) const override {
            if (WiFi.isConnected()) {
                mqtt_client->publish(topic, data);
            } else {
                logger.log(Logger::Level::Warning, "Cannot publish data to MQTT since WiFi is not connected.");
            }
        }
};
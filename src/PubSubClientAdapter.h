#pragma once

#include "HAMqtt.h"
#include "ReconnectingPubSubClient.h"
#include <memory>

class PubSubClientAdapter : public ha::MqttClient {
private:
    std::shared_ptr<ReconnectingPubSubClient> client;

public:
    PubSubClientAdapter(std::shared_ptr<ReconnectingPubSubClient> client) : client(client) {}

    bool publish(const std::string& topic, const std::string& payload, bool retain) override {
        if (!client->is_connected()) return false;
        
        bool res = client->publish(topic, payload, retain) == ReconnectingPubSubClient::Error::None;
        return res;
    }

    void subscribe(const std::string& topic) override {
        client->subscribe(topic);
    }

    void set_callback(MessageCallback callback) override {
        client->set_callback([callback](char* topic, uint8_t* payload, unsigned int length) {
            callback(topic, payload, length);
        });
    }
};

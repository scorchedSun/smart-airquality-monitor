#pragma once

#include <string>
#include <memory>

#include "ArduinoJson.h"

#include <PubSubClient.h>

class ReconnectingPubSubClient {

    private:
        const std::shared_ptr<PubSubClient> mqtt_client;
        const std::string mqtt_user;
        const std::string mqtt_password;
        const std::string client_id;
        const uint32_t fixed_delay_between_reconnect_attempts_in_ms;
        const uint8_t max_reconnect_attempts;

        bool establish_connection_to_broker() {
            uint8_t number_of_connection_attempts(0);
            int client_state(-1);

            while (!mqtt_client->connected() && client_state < 0 && max_reconnect_attempts > number_of_connection_attempts + 1) {
                if (!mqtt_client->connect(client_id.c_str(), mqtt_user.c_str(), mqtt_password.c_str())) {
                    client_state = mqtt_client->state();
                    Serial.print("Failed to connect to MQTT with state: ");
                    Serial.println(client_state);
                    delay(fixed_delay_between_reconnect_attempts_in_ms);
                    number_of_connection_attempts++;

                    Serial.print("Tried connecting ");
                    Serial.print(number_of_connection_attempts);
                    Serial.print(" of ");
                    Serial.print(max_reconnect_attempts);
                    Serial.println(" times.");
                }
            }
            return mqtt_client->connected();
        }


    public:
        enum class Error { None, ReconnectFailed };

        ReconnectingPubSubClient(const std::shared_ptr<PubSubClient> mqtt_client,
                                 const std::string& mqtt_user,
                                 const std::string& mqtt_password,
                                 const std::string& client_id,
                                 const uint32_t fixed_delay_between_reconnect_attempts_in_ms = 5000,
                                 const uint32_t max_reconnect_attempts = 10)
            : mqtt_client(mqtt_client)
            , mqtt_user(mqtt_user)
            , mqtt_password(mqtt_password)
            , client_id(client_id)
            , fixed_delay_between_reconnect_attempts_in_ms(fixed_delay_between_reconnect_attempts_in_ms)
            , max_reconnect_attempts(max_reconnect_attempts) {
        }

        Error publish(const std::string& topic, const JsonDocument& data) {
            if (establish_connection_to_broker()) {
                Serial.println(std::string("Publishing message to ").append(topic).c_str());

                std::string buffer;
                const size_t size = serializeJson(data, buffer);

                Serial.println(buffer.c_str());
                Serial.println(size);

                if (!mqtt_client->publish(topic.c_str(), buffer.c_str())) {
                    Serial.println("Publish failed!");
                    Serial.println(mqtt_client->getWriteError());
                }

                return Error::None;
            }

            return Error::ReconnectFailed;
        }
};
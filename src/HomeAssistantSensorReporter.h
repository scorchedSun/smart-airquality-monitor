#pragma once

#include "ArduinoJson.h"
#include "WiFi.h"

#include "Logger.h"
#include "ValueReporter.h"
#include "ReconnectingPubSubClient.h"
#include "HomeAssistantDevice.h"

class HomeAssistantSensorReporter : public ValueReporter {

private:
    const std::shared_ptr<ReconnectingPubSubClient> mqtt_client;
    const std::shared_ptr<HomeAssistantDevice> device;

public:
    HomeAssistantSensorReporter(
        const std::shared_ptr<HomeAssistantDevice> device,
        const std::shared_ptr<ReconnectingPubSubClient> mqtt_client)
            : mqtt_client(mqtt_client)
            , device(device) {
        }

    void report(const std::vector<std::unique_ptr<Measurement>>& measurements) override {
        DynamicJsonDocument measurements_json(1024);

        for (const auto& measurement : measurements) {
            const MeasurementDetails& details = measurement->get_details();
            if (!device->has_discovery_message_been_sent_for(details)) {
                ReconnectingPubSubClient::Error error = mqtt_client->publish(device->get_discovery_topic_for(details), device->get_discovery_message_for(details), true);
                
                if (error == ReconnectingPubSubClient::Error::None) {
                    device->set_discovery_message_sent_for(details);
                }
            }

            measurements_json[device->get_device_class_for(measurement->get_details())] = measurement->value_to_string();  
        }

        if (WiFi.isConnected()) {
            ReconnectingPubSubClient::Error publish_error = mqtt_client->publish(device->get_sensor_state_topic(), measurements_json);

            if (publish_error != ReconnectingPubSubClient::Error::None) {
                logger.log(Logger::Level::Warning, "Publishing sensor data to Home Assistant failed due to an error (Error code: {}).", (uint32_t)publish_error);
            }
        } else {
            logger.log(Logger::Level::Warning, "Publishing sensor data to Home Assistant failed: WiFi is not connected.");
        }
    }

};
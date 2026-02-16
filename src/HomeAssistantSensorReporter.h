#pragma once

#include "ArduinoJson.h"
#include "WiFi.h"
#include <map>
#include <memory>
#include <atomic>

#include "Logger.h"
#include "ValueReporter.h"
#include "HASensor.h"
#include "HomeAssistantManager.h"

class HomeAssistantSensorReporter : public ValueReporter {

private:
    std::shared_ptr<ha::Manager> ha_manager;
    std::map<MeasurementType, std::shared_ptr<ha::Sensor>> sensors;
    std::atomic<bool> needs_report{false};

public:
    HomeAssistantSensorReporter(std::shared_ptr<ha::Manager> manager)
            : ha_manager(manager) {
    }
    
    void add_sensor(MeasurementType type, std::shared_ptr<ha::Sensor> sensor) {
        sensors[type] = sensor;
    }

    void report(const std::vector<std::unique_ptr<Measurement>>& measurements) override {
        bool updated = false;
        for (const auto& measurement : measurements) {
            MeasurementType type = measurement->get_details().get_type();
            
            auto it = sensors.find(type);
            if (it == sensors.end()) continue;

            const auto& sensor = it->second;
            sensor->update_state(measurement->value_to_string());
            updated = true;
        }

        if (updated) {
            needs_report.store(true);
        }
    }

    bool check_and_clear_report_flag() {
        return needs_report.exchange(false);
    }

};
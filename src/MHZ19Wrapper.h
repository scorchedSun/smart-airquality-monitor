#pragma once 

#include "MHZ19.h"
#include "SoftwareSerial.h"

#include "Sensor.h"
#include "Logger.h"

class MHZ19Wrapper : public Sensor {

private:
    const MeasurementDetails sensor_details = MeasurementDetails(MeasurementType::CO2, MeasurementUnit::PPM);

    MHZ19 sensor;
    SoftwareSerial serial;
    const uint32_t baud_rate;
    uint32_t warmup_start_millis = 0;
    const uint32_t warmup_duration_millis = 120000; // 2 minutes

public:
    MHZ19Wrapper(int8_t rx_pin, int8_t tx_pin, uint32_t baud_rate) 
        : serial(tx_pin, rx_pin)
        , baud_rate(baud_rate) {
    }

    bool begin() override {
        serial.begin(baud_rate);
        sensor.begin(serial);
        sensor.autoCalibration(true);
        warmup_start_millis = millis();

        return true;
    }

    bool provideMeasurements(std::vector<std::unique_ptr<Measurement>>& measurements) override {
        if (millis() - warmup_start_millis < warmup_duration_millis) {
            return false;
        }

        const uint32_t value = sensor.getCO2(false);
  
        if (value > 0) {
            measurements.push_back(std::make_unique<RoundNumberMeasurement>(sensor_details, value));
            return true;
        } else {
            logger.log(Logger::Level::Warning, "Reading CO2 concentration failed: %u", (uint32_t)sensor.errorCode);
        }
        return false;
    }
};
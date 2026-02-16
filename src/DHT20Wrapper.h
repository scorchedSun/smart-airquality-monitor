#pragma once

#include "DHT20.h"

#include "Sensor.h"
#include "Logger.h"
#include <mutex>

class DHT20Wrapper : public Sensor {

private:
    DHT20 sensor;
    const MeasurementDetails temperature_sensor_details = MeasurementDetails(MeasurementType::Temperature, MeasurementUnit::DegreesCelcius);
    const MeasurementDetails humidity_sensor_details = MeasurementDetails(MeasurementType::Humidity, MeasurementUnit::Percent);
    std::mutex& i2c_mutex;

public:
    DHT20Wrapper(std::mutex& i2c_mutex) : i2c_mutex(i2c_mutex) {}
    bool begin() override {
        std::lock_guard<std::mutex> lock(i2c_mutex);
        return sensor.begin();
    }

    bool provide_measurements(std::vector<std::unique_ptr<Measurement>>& measurements) override {
        int status;
        {
            std::lock_guard<std::mutex> lock(i2c_mutex);
            status = sensor.read();
        }

        switch (status) {
            case DHT20_OK:
                measurements.push_back(std::make_unique<DecimalMeasurement>(humidity_sensor_details, sensor.getHumidity()));
                measurements.push_back(std::make_unique<DecimalMeasurement>(temperature_sensor_details, sensor.getTemperature()));
                return true;
            case DHT20_ERROR_CHECKSUM:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: Checksum error");
                break;
            case DHT20_ERROR_CONNECT:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: Connect error");
                break;
            case DHT20_MISSING_BYTES:
            logger.log(Logger::Level::Error, "Reading DHT20 failed: Missing bytes");
                break;
            case DHT20_ERROR_BYTES_ALL_ZERO:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: All bytes read zero");
                break;
            case DHT20_ERROR_READ_TIMEOUT:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: Read time out");
                break;
            case DHT20_ERROR_LASTREAD:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: Error read too fast");
                break;
            default:
                logger.log(Logger::Level::Error, "Reading DHT20 failed: Unknown error");
                break;
        }

        return false;
    }

};
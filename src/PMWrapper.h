#pragma once

#include "PMserial.h"

#include "Sensor.h"
#include "Logger.h"

class PMWrapper : Sensor {

private:
    const MeasurementDetails pm1_details = MeasurementDetails(MeasurementType::PM1, MeasurementUnit::MicroGramPerCubicMeter);
    const MeasurementDetails pm25_details = MeasurementDetails(MeasurementType::PM25, MeasurementUnit::MicroGramPerCubicMeter);
    const MeasurementDetails pm10_details = MeasurementDetails(MeasurementType::PM10, MeasurementUnit::MicroGramPerCubicMeter);

    SerialPM sensor;

public:
    PMWrapper(PMS type, uint8_t rx_pin, uint8_t tx_pin)
        : sensor(type, rx_pin, tx_pin) {
    }

    bool begin() {
        sensor.init();
        sensor.sleep();
        return true;
    }

    bool provide_measurements(std::vector<std::unique_ptr<Measurement>>& measurements) override {
        bool success(false);

        sensor.wake();
        sensor.read();

        if (sensor) {
          measurements.push_back(std::make_unique<RoundNumberMeasurement>(pm1_details, sensor.pm01));
          measurements.push_back(std::make_unique<RoundNumberMeasurement>(pm25_details, sensor.pm25));
          measurements.push_back(std::make_unique<RoundNumberMeasurement>(pm10_details, sensor.pm10));
          success = true;
        }
        else {
          switch (sensor.status)
          {
          case sensor.OK: // should never come here
            break;     // included to compile without warnings
          case sensor.ERROR_TIMEOUT:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_TIMEOUT);
            break;
          case sensor.ERROR_MSG_UNKNOWN:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_UNKNOWN);
            break;
          case sensor.ERROR_MSG_HEADER:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_HEADER);
            break;
          case sensor.ERROR_MSG_BODY:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_BODY);
            break;
          case sensor.ERROR_MSG_START:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_START);
            break;
          case sensor.ERROR_MSG_LENGTH:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_LENGTH);
            break;
          case sensor.ERROR_MSG_CKSUM:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_MSG_CKSUM);
            break;
          case sensor.ERROR_PMS_TYPE:
            logger.log(Logger::Level::Error, "Reading PMS failed: {}", PMS_ERROR_PMS_TYPE);
            break;
          }
        }
        sensor.sleep();
        return success;
    }
};
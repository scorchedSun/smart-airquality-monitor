#pragma once

#include <cstdint>

class Actuator {
public:
  virtual void begin(uint8_t initial_speed_percent = 0) = 0;
  virtual void turn_to_percent(uint8_t percent) = 0;
  virtual void turn_off() = 0;
  virtual ~Actuator() = default;
};

#pragma once

#include "driver/ledc.h"

class PWMFan {
private:
    ledc_timer_config_t timer_config;
    ledc_channel_config_t channel_config;
    const uint8_t control_pin;
    const uint32_t frequency_hz;
    static constexpr uint8_t duty_resolution = LEDC_TIMER_10_BIT;
    static constexpr uint32_t max_duty = (1 << duty_resolution);

public:
    PWMFan(uint8_t control_pin, uint32_t frequency_hz);
    
    void begin(uint8_t initial_speed_percent = 0);
    void turn_to_percent(uint8_t percent);
    void turn_off();
};

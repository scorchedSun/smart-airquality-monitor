#pragma once

#include "driver/ledc.h"
#include <algorithm>

class PWMFan {
private:
    ledc_timer_config_t timer_config;
    ledc_channel_config_t channel_config;
    const uint8_t control_pin;
    const uint32_t frequency_hz;
    static constexpr uint8_t duty_resolution = LEDC_TIMER_10_BIT;
    static constexpr uint32_t max_duty = (1 << duty_resolution);

public:
    PWMFan(uint8_t control_pin, uint32_t frequency_hz)
        : control_pin(control_pin)
        , frequency_hz(frequency_hz) {
    }
    
    void begin(uint8_t initial_speed_percent = 0) {
        timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = (ledc_timer_bit_t)duty_resolution,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = frequency_hz,
            .clk_cfg = LEDC_AUTO_CLK
        };

        channel_config= {
            .gpio_num = control_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .duty = (uint32_t)(max_duty * (std::min(initial_speed_percent, (uint8_t)100) / 100.0)),
            .hpoint = 0
        };

        ledc_timer_config(&timer_config);
        ledc_channel_config(&channel_config);
    }

    void turn_to_percent(uint8_t percent) {
        if (percent > 100) {
            percent = 100;
        }

        ledc_set_duty(channel_config.speed_mode, channel_config.channel, max_duty * (percent / 100.0));
        ledc_update_duty(channel_config.speed_mode, channel_config.channel);
    }

    void turn_off() {
        turn_to_percent(0);
    }
};

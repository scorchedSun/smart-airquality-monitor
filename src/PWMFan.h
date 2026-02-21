#pragma once

#include "driver/ledc.h"
#include <algorithm>

class PWMFan {
private:
    ledc_timer_config_t timer_config_;
    ledc_channel_config_t channel_config_;
    const uint8_t control_pin_;
    const uint32_t frequency_hz_;
    static constexpr uint8_t duty_resolution = LEDC_TIMER_10_BIT;
    static constexpr uint32_t max_duty = (1 << duty_resolution);

public:
    PWMFan(uint8_t control_pin, uint32_t frequency_hz)
        : control_pin_(control_pin)
        , frequency_hz_(frequency_hz) {
    }
    
    void begin(uint8_t initial_speed_percent = 0) {
        timer_config_ = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = (ledc_timer_bit_t)duty_resolution,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = frequency_hz_,
            .clk_cfg = LEDC_AUTO_CLK
        };

        channel_config_= {
            .gpio_num = control_pin_,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .duty = (max_duty * std::min<uint8_t>(initial_speed_percent, 100)) / 100,
            .hpoint = 0
        };

        ledc_timer_config(&timer_config_);
        ledc_channel_config(&channel_config_);
    }

    void turnToPercent(uint8_t percent) {
        if (percent > 100) {
            percent = 100;
        }

        ledc_set_duty(channel_config_.speed_mode, channel_config_.channel, (max_duty * percent) / 100);
        ledc_update_duty(channel_config_.speed_mode, channel_config_.channel);
    }

    void turnOff() {
        turnToPercent(0);
    }
};

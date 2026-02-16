#include "PWMFan.h"
#include <algorithm>

PWMFan::PWMFan(uint8_t control_pin, uint32_t frequency_hz)
    : control_pin(control_pin)
    , frequency_hz(frequency_hz) {
}

void PWMFan::begin(uint8_t initial_speed_percent) {
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

void PWMFan::turn_to_percent(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }

    ledc_set_duty(channel_config.speed_mode, channel_config.channel, max_duty * (percent / 100.0));
    ledc_update_duty(channel_config.speed_mode, channel_config.channel);
}

void PWMFan::turn_off() {
    turn_to_percent(0);
}

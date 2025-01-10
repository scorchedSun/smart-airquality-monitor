#pragma once

#include <string>
#include <memory>

#include "Adafruit_SSD1306.h"

#include "Translator.h"

class Display {

private:
    Adafruit_SSD1306 display;

    const std::vector<char> wait_animation_chars { '-', '\\', '|', '/' };

    bool is_setup = false;
    bool is_enabled = true;

    std::string ip_address;
    const std::unique_ptr<Translator<MeasurementType>> measurement_type_translator;
    const std::unique_ptr<Translator<MeasurementUnit>> measurement_unit_translator;
    
    void add_details() {
        if (!ip_address.empty()) {
            display.setCursor(0,0);
            display.print("IP: ");
            display.print(ip_address.c_str());
        }
    }

public:
    Display(uint8_t width,
            uint8_t height,
            int8_t mosi_pin,
            int8_t clk_pin,
            int8_t dc_pin,
            int8_t reset_pin,
            int8_t cs_pin)
        : display(width, height, mosi_pin, clk_pin, dc_pin, reset_pin, cs_pin)
        , measurement_type_translator(std::make_unique<FriendlyNameTypeTranslator>())
        , measurement_unit_translator(std::make_unique<DisplayUnitTranslator>()) {
            
    }

    void setup() {
        if (is_setup) return;

        if (!display.begin(SSD1306_SWITCHCAPVCC))
        {
            Serial.println("Display could not be set up.");
            for (;;);
        }

        is_setup = true;

        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.clearDisplay();
        display.display();
        delay(100);
    }

    void set_enabled(bool enabled) {
        this->is_enabled = enabled;

        if (enabled) {
            turn_on();
        } else {
            turn_off();
        }
    }

    void turn_off() {
        if (!is_setup) return;
        display.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void turn_on() {
        if (!is_setup) return;
        display.ssd1306_command(SSD1306_DISPLAYON);
    }

    void set_ip_address(const std::string& ip_address) {
        this->ip_address = ip_address;
    }

    void show(const std::string& message) {
        if (!is_setup || !is_enabled) return;

        display.clearDisplay();
        add_details();
        display.setCursor(5,28);
        display.println(message.c_str());
        display.display();
    }

    void show(const std::unique_ptr<Measurement>& measurement) {
        std::string message(measurement_type_translator->translate(measurement->get_details().get_type()));
        message
            .append(": ")
            .append(measurement->value_to_string())
            .append(" ")
            .append(measurement_unit_translator->translate(measurement->get_details().get_unit()));
        show(message);
    }

    void wait_with_animation(const std::string& message, const uint32_t time_millis) {
        uint32_t start_time = millis();
        uint8_t animation_index = 0;
        uint8_t max_animation_index = (uint8_t)wait_animation_chars.size();

        while (millis() - start_time <= time_millis) {
            if (is_enabled && is_setup) {
                display.clearDisplay();
                add_details();
                display.setCursor(0,16);
                display.println(message.c_str());
                display.setCursor(60, 36);
                display.print(wait_animation_chars.at(animation_index));
                display.display();
                animation_index = ++animation_index % max_animation_index;
            }
            delay(100);
        }
    }

};
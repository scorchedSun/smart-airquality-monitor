#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "Adafruit_SSD1306.h"
#include <SPI.h>

#include "Translator.h"
#include "Translator.h"
#include "Icons.h"

class Display {

private:
    Adafruit_SSD1306 display;

    bool is_setup = false;
    bool is_enabled = true;
    bool wifi_connected = false;
    bool mqtt_connected = false;

    std::string ip_address;
    const std::unique_ptr<Translator<MeasurementType>> measurement_type_translator;
    const std::unique_ptr<Translator<MeasurementUnit>> measurement_unit_translator;
    std::mutex& i2c_mutex;
    
    void drawWifiStatus() {
        if (WiFi.status() != WL_CONNECTED) {
             display.drawBitmap(118, 0, wifi_disconnected_outline_10x10, 10, 10, SSD1306_WHITE);
             return;
        }
        display.drawBitmap(118, 0, wifi_static_10x10, 10, 10, SSD1306_WHITE);
    }

    void draw_status_bar() {
        display.drawLine(0, 11, 127, 11, SSD1306_WHITE);
        drawWifiStatus();
        display.drawBitmap(106, 0, mqtt_connected ? mqtt_ico : mqtt_none_ico, 10, 10, SSD1306_WHITE);
    }



    const uint8_t* get_icon_for_type(MeasurementType type) {
        switch (type) {
            case MeasurementType::Temperature: return temp_ico;
            case MeasurementType::Humidity: return hum_ico;
            case MeasurementType::CO2: return co2_ico;
            case MeasurementType::PM1:
            case MeasurementType::PM25:
            case MeasurementType::PM10: return pm_ico;
            default: return nullptr;
        }
    }

public:
    Display(uint8_t width,
            uint8_t height,
            int8_t dc_pin,
            int8_t reset_pin,
            int8_t cs_pin,
            std::mutex& i2c_mutex)
        : display(width, height, &SPI, dc_pin, reset_pin, cs_pin)
        , measurement_type_translator(std::make_unique<FriendlyNameTypeTranslator>())
        , measurement_unit_translator(std::make_unique<DisplayUnitTranslator>())
        , i2c_mutex(i2c_mutex) {
            
    }

    void setup() {
        if (is_setup) return;
        
        {
            std::lock_guard<std::mutex> lock(i2c_mutex);
            SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
            
            if (!display.begin(SSD1306_SWITCHCAPVCC))
            {
                Serial.println("Display could not be set up.");
                for (;;);
            }
    
            is_setup = true;
            display.clearDisplay();
            display.display();
    
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
        }
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

    bool get_enabled() const {
        return is_enabled;
    }

    void turn_off() {
        if (!is_setup) return;
        std::lock_guard<std::mutex> lock(i2c_mutex);
        display.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void turn_on() {
        if (!is_setup) return;
        std::lock_guard<std::mutex> lock(i2c_mutex);
        display.ssd1306_command(SSD1306_DISPLAYON);
    }

    void set_connectivity(bool wifi, bool mqtt) {
        this->wifi_connected = wifi;
        this->mqtt_connected = mqtt;
    }

    void set_ip_address(const std::string& ip_address) {
        this->ip_address = ip_address;
    }

    void show(const std::string& message) {
        if (!is_setup || !is_enabled) return;

        std::lock_guard<std::mutex> lock(i2c_mutex);
        display.clearDisplay();
        draw_status_bar();
        display.setCursor(0, 16);
        display.setTextSize(1);
        display.println(message.c_str());
        display.display();
    }

    void show(const std::unique_ptr<Measurement>& measurement) {
        if (!is_setup || !is_enabled) return;

        MeasurementType m_type = measurement->get_details().get_type();
        std::string type_name(measurement_type_translator->translate(m_type));
        std::string value = measurement->value_to_string();
        std::string unit(measurement_unit_translator->translate(measurement->get_details().get_unit()));
        const uint8_t* icon = get_icon_for_type(m_type);

        std::lock_guard<std::mutex> lock(i2c_mutex);
        display.clearDisplay();
        draw_status_bar();

        std::string val_unit = value + unit;
        int16_t x1, y1;
        uint16_t w_val, h_val;
        display.setTextSize(2);
        display.getTextBounds(val_unit.c_str(), 0, 0, &x1, &y1, &w_val, &h_val);
        
        int icon_w = 32;
        int icon_h = 32;
        int y_pos = 26;

        int spacing = 6;
        int total_w = icon_w + spacing + w_val;
        int start_x = (128 - total_w) / 2;

        if (icon) {
            display.drawBitmap(start_x, y_pos, icon, icon_w, icon_h, SSD1306_WHITE);
        }

        display.setTextSize(1);
        uint16_t w_type, h_type;
        display.getTextBounds(type_name.c_str(), 0, 0, &x1, &y1, &w_type, &h_type);
        display.setCursor((128 - w_type) / 2, 16);
        display.println(type_name.c_str());
        
        display.setTextSize(2);
        display.setCursor(start_x + icon_w + spacing, y_pos + (icon_h - h_val) / 2);
        display.println(val_unit.c_str());
        
        display.display();
    }


    void show_boot_step(const std::string& message, int frame) {
        if (!is_setup || !is_enabled) return;

        std::lock_guard<std::mutex> lock(i2c_mutex);
        display.clearDisplay();

        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(message.c_str());

        display.drawBitmap(39, 14, boot_anim_data[frame % BOOT_ANIM_FRAMES], BOOT_ANIM_WIDTH, BOOT_ANIM_HEIGHT, SSD1306_WHITE);

        if (!ip_address.empty()) {
            display.setTextSize(1);
            display.setCursor(0, 56);
            display.print("IP: ");
            display.print(ip_address.c_str());
        }

        display.display();
    }
};
#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <mutex>

#include "Adafruit_SSD1306.h"
#include <SPI.h>

#include "Translator.h"
#include "Icons.h"

class Display {

private:
    Adafruit_SSD1306 display_;

    bool is_setup_ = false;
    bool is_enabled_ = true;
    bool wifi_connected_ = false;
    bool mqtt_connected_ = false;

    std::string ip_address_;
    const std::unique_ptr<Translator<MeasurementType>> measurement_type_translator_;
    const std::unique_ptr<Translator<MeasurementUnit>> measurement_unit_translator_;
    std::mutex& i2c_mutex_;
    
    void drawWifiStatus() {
        if (WiFi.status() != WL_CONNECTED) {
             display_.drawBitmap(118, 0, wifi_disconnected_outline_10x10, 10, 10, SSD1306_WHITE);
             return;
        }
        display_.drawBitmap(118, 0, wifi_static_10x10, 10, 10, SSD1306_WHITE);
    }

    void drawStatusBar() {
        display_.drawLine(0, 11, 127, 11, SSD1306_WHITE);
        drawWifiStatus();
        display_.drawBitmap(106, 0, mqtt_connected_ ? mqtt_ico : mqtt_none_ico, 10, 10, SSD1306_WHITE);
    }

    static const uint8_t* getIconForType(MeasurementType type) {
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
        : display_(width, height, &SPI, dc_pin, reset_pin, cs_pin)
        , measurement_type_translator_(std::make_unique<FriendlyNameTypeTranslator>())
        , measurement_unit_translator_(std::make_unique<DisplayUnitTranslator>())
        , i2c_mutex_(i2c_mutex) {
            
    }

    void setup() {
        if (is_setup_) return;
        
        {
            std::lock_guard<std::mutex> lock(i2c_mutex_);
            SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
            
            if (!display_.begin(SSD1306_SWITCHCAPVCC))
            {
                Serial.println("Display could not be set up.");
                return;
            }
    
            is_setup_ = true;
            display_.clearDisplay();
            display_.display();
    
            display_.setTextSize(1);
            display_.setTextColor(SSD1306_WHITE);
        }
        delay(100);
    }

    void setEnabled(bool enabled) {
        is_enabled_ = enabled;

        if (enabled) {
            turnOn();
        } else {
            turnOff();
        }
    }

    bool getEnabled() const {
        return is_enabled_;
    }

    void turnOff() {
        if (!is_setup_) return;
        std::lock_guard<std::mutex> lock(i2c_mutex_);
        display_.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void turnOn() {
        if (!is_setup_) return;
        std::lock_guard<std::mutex> lock(i2c_mutex_);
        display_.ssd1306_command(SSD1306_DISPLAYON);
    }

    void setConnectivity(bool wifi, bool mqtt) {
        wifi_connected_ = wifi;
        mqtt_connected_ = mqtt;
    }

    void setIpAddress(const char* ip_address) {
        ip_address_ = std::string(ip_address);
    }

    void show(const char* message) {
        if (!is_setup_ || !is_enabled_) return;

        std::lock_guard<std::mutex> lock(i2c_mutex_);
        display_.clearDisplay();
        drawStatusBar();
        display_.setCursor(0, 16);
        display_.setTextSize(1);
        display_.println(message);
        display_.display();
    }

    void show(const std::unique_ptr<Measurement>& measurement) {
        if (!is_setup_ || !is_enabled_) return;

        MeasurementType m_type = measurement->getDetails().getType();
        
        std::string_view type_name_sv = measurement_type_translator_->translate(m_type);
        std::string type_name(type_name_sv.data(), type_name_sv.length());
        
        std::string value = measurement->valueToString();
        
        std::string_view unit_sv = measurement_unit_translator_->translate(measurement->getDetails().getUnit());
        std::string unit(unit_sv.data(), unit_sv.length());
        
        const uint8_t* icon = getIconForType(m_type);

        std::lock_guard<std::mutex> lock(i2c_mutex_);
        display_.clearDisplay();
        drawStatusBar();

        std::string val_unit = value + unit;
        int16_t x1, y1;
        uint16_t w_val, h_val;
        display_.setTextSize(2);
        display_.getTextBounds(val_unit.c_str(), 0, 0, &x1, &y1, &w_val, &h_val);
        
        int icon_w = 32;
        int icon_h = 32;
        int y_pos = 26;

        int spacing = 6;
        int total_w = icon_w + spacing + w_val;
        int start_x = (128 - total_w) / 2;

        if (icon) {
            display_.drawBitmap(start_x, y_pos, icon, icon_w, icon_h, SSD1306_WHITE);
        }

        display_.setTextSize(1);
        uint16_t w_type, h_type;
        display_.getTextBounds(type_name.c_str(), 0, 0, &x1, &y1, &w_type, &h_type);
        display_.setCursor((128 - w_type) / 2, 16);
        display_.println(type_name.c_str());
        
        display_.setTextSize(2);
        display_.setCursor(start_x + icon_w + spacing, y_pos + (icon_h - h_val) / 2);
        display_.println(val_unit.c_str());
        
        display_.display();
    }


    void showBootStep(const char* message, int frame) {
        if (!is_setup_ || !is_enabled_) return;

        std::lock_guard<std::mutex> lock(i2c_mutex_);
        display_.clearDisplay();

        display_.setTextSize(1);
        display_.setCursor(0, 0);
        display_.print(message);

        display_.drawBitmap(39, 14, boot_anim_data[frame % BOOT_ANIM_FRAMES], BOOT_ANIM_WIDTH, BOOT_ANIM_HEIGHT, SSD1306_WHITE);

        if (!ip_address_.empty()) {
            display_.setTextSize(1);
            display_.setCursor(0, 56);
            display_.print("IP: ");
            display_.print(ip_address_.c_str());
        }

        display_.display();
    }
};
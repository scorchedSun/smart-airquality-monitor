#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <Preferences.h>
#include <Esp.h>
#include "ConfigKeys.h"

class ConfigManager {
private:
    Preferences prefs_;
    std::mutex mutex_;
    std::string mac_id_cache_;
    bool initialized_ = false;
    static constexpr const char* namespace_name = "smaq";

    ConfigManager() = default;

public:
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void begin() {
        if (initialized_) return;
        prefs_.begin(namespace_name, false);
        initialized_ = true;
    }

    void buildMacId() {
        uint64_t mac = ESP.getEfuseMac();
        char buf[13];
        uint8_t mac_bytes[6];
        memcpy(mac_bytes, &mac, 6);
        snprintf(buf, sizeof(buf), "%02X%02X%02X", mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        mac_id_cache_ = std::string(buf);
    }

    std::string getString(const char* key, const char* defaultValue = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return std::string(defaultValue);
        return std::string(prefs_.getString(key, defaultValue).c_str());
    }

    void putString(const char* key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        prefs_.putString(key, value.c_str());
    }

    int32_t getInt(const char* key, int32_t defaultValue = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return defaultValue;
        return prefs_.getInt(key, defaultValue);
    }

    void putInt(const char* key, int32_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        prefs_.putInt(key, value);
    }

    bool getBool(const char* key, bool defaultValue = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return defaultValue;
        return prefs_.getBool(key, defaultValue);
    }

    void putBool(const char* key, bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) return;
        prefs_.putBool(key, value);
    }

    std::string getMacId() const {
        return mac_id_cache_;
    }

    std::string getHostName() {
        std::string name = getString(cfg::keys::host_name, cfg::defaults::host_name);
        if (name.empty()) name = cfg::defaults::host_name;
        
        std::string suffix = "0000";
        if (mac_id_cache_.size() >= 4) {
             suffix = mac_id_cache_.substr(mac_id_cache_.size() - 4);
        }
        return name + "-" + suffix;
    }
};

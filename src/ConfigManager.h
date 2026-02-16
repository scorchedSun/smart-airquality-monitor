#pragma once

#include <mutex>
#include <string>
#include <Preferences.h>
#include <Esp.h>
#include "config_keys.h"

class ConfigManager {
private:
    Preferences prefs;
    std::mutex mutex;
    std::string mac_id_cache;
    bool initialized = false;
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
        if (initialized) return;
        prefs.begin(namespace_name, false);
        initialized = true;
    }

    void buildMacId() {
        uint64_t mac = ESP.getEfuseMac();
        char buf[13];
        // Extract bytes: mac is 48-bit (6 bytes). 
        // ESP.getEfuseMac() returns the MAC address as a 64-bit integer.
        // The bytes are in little-endian order in the uint64_t? 
        // Actually, the previous code did:
        // memcpy(mac, &efuse, 6); -> mac[0]..mac[5]
        // And printed mac[0]..mac[5].
        // The user wants the LAST 3 bytes.
        // If we use the same memcpy approach:
        uint8_t mac_bytes[6];
        memcpy(mac_bytes, &mac, 6);
        snprintf(buf, sizeof(buf), "%02X%02X%02X", mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        mac_id_cache = std::string(buf);
    }

    std::string getString(const char* key, const char* defaultValue = "") {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return std::string(defaultValue);
        return std::string(prefs.getString(key, defaultValue).c_str());
    }

    void putString(const char* key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return;
        prefs.putString(key, value.c_str());
    }

    int32_t getInt(const char* key, int32_t defaultValue = 0) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return defaultValue;
        return prefs.getInt(key, defaultValue);
    }

    void putInt(const char* key, int32_t value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return;
        prefs.putInt(key, value);
    }

    bool getBool(const char* key, bool defaultValue = false) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return defaultValue;
        return prefs.getBool(key, defaultValue);
    }

    void putBool(const char* key, bool value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) return;
        prefs.putBool(key, value);
    }

    std::string getMacId() const {
        return mac_id_cache;
    }

    std::string getHostName() {
        std::string name = getString(cfg::keys::host_name, cfg::defaults::host_name);
        if (name.empty()) name = cfg::defaults::host_name;
        std::string suffix = mac_id_cache.size() >= 4
            ? mac_id_cache.substr(mac_id_cache.size() - 4)
            : "0000";
        return name + "-" + suffix;
    }
};

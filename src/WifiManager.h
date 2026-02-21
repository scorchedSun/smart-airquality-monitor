#pragma once

#include <string>
#include <WiFi.h>
#include "ConfigManager.h"
#include "ConfigKeys.h"

class WifiManager {
public:
    bool connect() {
        auto& cm = ConfigManager::getInstance();
        std::string ssid = cm.getString(cfg::keys::wifi_ssid, cfg::defaults::wifi_ssid);
        std::string pass = cm.getString(cfg::keys::wifi_pass, cfg::defaults::wifi_pass);

        if (ssid.empty()) return false;

        WiFi.setHostname(cm.getHostName().c_str());
        WiFi.setAutoReconnect(true);
        WiFi.begin(ssid.c_str(), pass.c_str());

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
        }

        return WiFi.status() == WL_CONNECTED;
    }

    void setupCaptivePortal(const std::string& hostname) {
        WiFi.softAP(hostname.c_str());
    }

    bool isConnected() const {
        return WiFi.isConnected();
    }

    IPAddress localIP() const {
        return WiFi.localIP();
    }

    IPAddress softAPIP() const {
        return WiFi.softAPIP();
    }
};

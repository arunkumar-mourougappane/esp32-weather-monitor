#include "WiFiManager.h"
#include <esp_log.h>
#include <stdint.h>
#include <string.h>

static const char* TAG = "WiFiManager";

RTC_DATA_ATTR uint8_t rtc_bssid[6] = {0};
RTC_DATA_ATTR int32_t rtc_channel = 0;

WiFiManager& WiFiManager::getInstance() {
    static WiFiManager instance;
    return instance;
}

bool WiFiManager::startAP(const String& ssid, const String& password) {
    WiFi.mode(WIFI_AP_STA);
    bool ok;
    if (password.isEmpty()) {
        ok = WiFi.softAP(ssid.c_str());
    } else {
        ok = WiFi.softAP(ssid.c_str(), password.c_str());
    }
    if (ok) {
        ESP_LOGI(TAG, "SoftAP started: SSID=%s  IP=%s",
                 ssid.c_str(), WiFi.softAPIP().toString().c_str());
    } else {
        ESP_LOGE(TAG, "SoftAP failed to start");
    }
    return ok;
}

void WiFiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    ESP_LOGI(TAG, "SoftAP stopped");
}

bool WiFiManager::connectSTA(const String& ssid, const String& password,
                              uint32_t timeoutMs) {
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    WiFi.mode(WIFI_STA);

    // Deep Sleep Fast-Connect (Skip 13-channel AP Scan)
    if (rtc_channel != 0) {
        ESP_LOGI(TAG, "Fast-Connect engaged (Channel: %d, BSSID: %02x:%02x:%02x:%02x:%02x:%02x)", 
                 (int)rtc_channel, rtc_bssid[0], rtc_bssid[1], rtc_bssid[2], rtc_bssid[3], rtc_bssid[4], rtc_bssid[5]);
        WiFi.begin(ssid.c_str(), password.c_str(), rtc_channel, rtc_bssid, true);
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            ESP_LOGW(TAG, "STA connect timed out");
            rtc_channel = 0; // Wipe Fast-Connect Cache on Failure
            return false;
        }
        delay(250);
    }

    // Capture BSSID parameters for the next Deep Sleep wakeup
    if (rtc_channel == 0) {
        rtc_channel = WiFi.channel();
        uint8_t* native_bssid = WiFi.BSSID();
        if (native_bssid != nullptr) {
            memcpy(rtc_bssid, native_bssid, 6);
            ESP_LOGI(TAG, "Native BSSID and Channel (%d) locked to RTC memory", (int)rtc_channel);
        }
    }

    ESP_LOGI(TAG, "STA connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getStaIP() const {
    return WiFi.localIP().toString();
}

String WiFiManager::getApIP() const {
    return WiFi.softAPIP().toString();
}

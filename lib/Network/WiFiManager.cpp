#include "WiFiManager.h"
#include <esp_log.h>

static const char* TAG = "WiFiManager";

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
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            ESP_LOGW(TAG, "STA connect timed out");
            return false;
        }
        delay(250);
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

#include "NTPManager.h"
#include <esp_sntp.h>
#include <esp_log.h>

static const char* TAG = "NTPManager";

NTPManager& NTPManager::getInstance() {
    static NTPManager instance;
    return instance;
}

bool NTPManager::sync(const String& ntpServer, const String& timezone,
                      uint32_t timeoutMs) {
    // Configure timezone and set primary NTP server
    configTzTime(timezone.c_str(), ntpServer.c_str());
    setenv("TZ", timezone.c_str(), 1);
    tzset();

    ESP_LOGI(TAG, "Waiting for NTP sync (server: %s, tz: %s)...",
             ntpServer.c_str(), timezone.c_str());

    uint32_t start = millis();
    struct tm timeinfo = {};
    while (!::getLocalTime(&timeinfo, 0)) {
        if (millis() - start > timeoutMs) {
            ESP_LOGW(TAG, "NTP sync timed out after %u ms", timeoutMs);
            return false;
        }
        delay(500);
    }
    _synced = true;
    char buf[64];
    strftime(buf, sizeof(buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "NTP synced. Local time: %s", buf);
    return true;
}

bool NTPManager::getLocalTime(struct tm& timeinfo) const {
    return ::getLocalTime(&timeinfo, 0);
}

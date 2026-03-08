#include "NTPManager.h"
#include <esp_sntp.h>
#include <esp_log.h>
#include <time.h>

static const char* TAG = "NTPManager";

NTPManager& NTPManager::getInstance() {
    static NTPManager instance;
    return instance;
}

bool NTPManager::sync(const String& ntpServer, const String& timezone,
                      uint32_t timeoutMs) {
    ESP_LOGI(TAG, "Starting NTP sync (server: %s, tz: %s)...",
             ntpServer.c_str(), timezone.c_str());

    // Reset the system clock to zero to wipe out any stale RTC time
    // from previous bad syncs that survived reboot.
    struct timeval tv = {0, 0};
    settimeofday(&tv, NULL);

    // Provide the timezone string (POSIX format) and the NTP server.
    configTzTime(timezone.c_str(), ntpServer.c_str());

    uint32_t start = millis();
    
    // Block until SNTP officially completes a network sync
    while (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        if (millis() - start > timeoutMs) {
            ESP_LOGW(TAG, "NTP sync timed out after %u ms", timeoutMs);
            return false;
        }
        delay(500);
    }
    
    delay(100); // give it a moment to settle
    time_t raw_time = time(NULL);

    struct tm utc_tm;
    gmtime_r(&raw_time, &utc_tm);
    char utcBuf[64];
    strftime(utcBuf, sizeof(utcBuf), "%Y-%m-%d %H:%M:%S", &utc_tm);
    ESP_LOGI(TAG, "[DIAG] SNTP completed. Raw Epoch: %lld -> UTC Time: %s", (long long)raw_time, utcBuf);

    struct tm loc_tm;
    localtime_r(&raw_time, &loc_tm);
    char locBuf[64];
    strftime(locBuf, sizeof(locBuf), "%Y-%m-%d %H:%M:%S", &loc_tm);
    ESP_LOGI(TAG, "NTP synced. Platform local time: %s", locBuf);
    
    _synced = true;
    return true;
}

bool NTPManager::getLocalTime(struct tm& timeinfo) const {
    if (!_synced) return false;
    
    // Fall back to the standard Platform call. Because configTzTime() was used, 
    // the underlying C library applies the POSIX timezone offset correctly.
    return ::getLocalTime(&timeinfo, 0);
}

#include "NTPManager.h"
#include <esp_sntp.h>
#include <esp_log.h>
#include <time.h>
#include <M5Unified.h>

static const char* TAG = "NTPManager";

NTPManager& NTPManager::getInstance() {
    static NTPManager instance;
    return instance;
}

bool NTPManager::sync(const String& ntpServer, const String& timezone,
                      uint32_t timeoutMs) {
    ESP_LOGI(TAG, "Starting NTP sync (server: %s, tz: %s)...",
             ntpServer.c_str(), timezone.c_str());

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

    // Hard-write the synchronized UTC time straight into the BM8563 I2C chip so it survives power loss!
    m5::rtc_datetime_t rtc_time;
    rtc_time.date.year = utc_tm.tm_year + 1900;
    rtc_time.date.month = utc_tm.tm_mon + 1;
    rtc_time.date.date = utc_tm.tm_mday;
    rtc_time.time.hours = utc_tm.tm_hour;
    rtc_time.time.minutes = utc_tm.tm_min;
    rtc_time.time.seconds = utc_tm.tm_sec;
    M5.Rtc.setDateTime(rtc_time);

    struct tm loc_tm;
    localtime_r(&raw_time, &loc_tm);
    char locBuf[64];
    strftime(locBuf, sizeof(locBuf), "%Y-%m-%d %H:%M:%S", &loc_tm);
    ESP_LOGI(TAG, "NTP synced. Platform local time: %s", locBuf);
    
    _synced = true;
    return true;
}

bool NTPManager::getLocalTime(struct tm& timeinfo) const {
    // Rely on the standard OS timeval layer first.
    if (::getLocalTime(&timeinfo, 0)) {
        if (timeinfo.tm_year + 1900 > 2023) {
            return true;
        }
    }

    // OS Clock is destroyed (board was just flashed?). Attempt fallback extraction from BM8563 I2C Chip.
    m5::rtc_datetime_t bb_time = M5.Rtc.getDateTime();
    if (bb_time.date.year > 2023) {
        ESP_LOGW(TAG, "OS Clock wiped! Retrieving persistent exact hardware time from BM8563: %04d-%02d-%02d", bb_time.date.year, bb_time.date.month, bb_time.date.date);
        struct tm utc_tm = {0};
        utc_tm.tm_year = bb_time.date.year - 1900;
        utc_tm.tm_mon = bb_time.date.month - 1;
        utc_tm.tm_mday = bb_time.date.date;
        utc_tm.tm_hour = bb_time.time.hours;
        utc_tm.tm_min = bb_time.time.minutes;
        utc_tm.tm_sec = bb_time.time.seconds;
        
        // Push the BM8563 hardware time back into the OS native software tracker as UTC Epoch
        time_t raw_time = mktime(&utc_tm); 
        struct timeval tv = { .tv_sec = raw_time, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        
        // Recalculate POSIX Local Time dynamically now that OS is synced
        if (::getLocalTime(&timeinfo, 0)) {
            return true;
        }
    }

    return false;
}

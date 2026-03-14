#include "WiFiManager.h"
#include <esp_log.h>
#include <stdint.h>
#include <string.h>

static const char* TAG = "WiFiManager";

RTC_DATA_ATTR uint8_t rtc_bssid[6] = {0};
RTC_DATA_ATTR int32_t rtc_channel = 0;
RTC_DATA_ATTR char    rtc_cached_ssid[33] = {}; ///< SSID for the cached BSSID/channel

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
            // Clear all three RTC fast-connect fields so a stale BSSID/channel
            // cannot mislead the next boot into using the wrong AP.
            rtc_channel = 0;
            memset(rtc_bssid, 0, sizeof(rtc_bssid));
            rtc_cached_ssid[0] = '\0';
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
            strncpy(rtc_cached_ssid, ssid.c_str(), sizeof(rtc_cached_ssid) - 1);
            rtc_cached_ssid[sizeof(rtc_cached_ssid) - 1] = '\0';
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

// ── connectBestSTA ─────────────────────────────────────────────────────────────
bool WiFiManager::connectBestSTA(const String* ssids, const String* passes,
                                  int count, uint32_t timeoutMs) {
    if (count <= 0) return false;
    WiFi.mode(WIFI_STA);

    // ── 1. Fast-connect: cached BSSID/channel requires the same SSID to be valid ──
    if (rtc_channel != 0 && rtc_cached_ssid[0] != '\0') {
        for (int i = 0; i < count; i++) {
            if (!ssids[i].isEmpty() && ssids[i] == rtc_cached_ssid) {
                ESP_LOGI(TAG, "Fast-connect: SSID=%s ch=%d",
                         ssids[i].c_str(), (int)rtc_channel);
                WiFi.begin(ssids[i].c_str(), passes[i].c_str(),
                           rtc_channel, rtc_bssid, /*cleanConnect=*/true);
                uint32_t t0 = millis();
                while (WiFi.status() != WL_CONNECTED) {
                    if (millis() - t0 > timeoutMs / 3) {
                        ESP_LOGW(TAG, "Fast-connect timed out — clearing cache");
                        WiFi.disconnect(true);
                        rtc_channel = 0;
                        memset(rtc_bssid, 0, sizeof(rtc_bssid));
                        rtc_cached_ssid[0] = '\0';
                        break;
                    }
                    delay(250);
                }
                if (WiFi.status() == WL_CONNECTED) {
                    ESP_LOGI(TAG, "Fast-connect OK. IP: %s",
                             WiFi.localIP().toString().c_str());
                    return true;
                }
                break; // only one fast-connect attempt
            }
        }
    }

    // ── 2. Active scan to rank matching SSIDs by signal strength ────────────────
    static constexpr int kMaxCands = 5;
    struct Candidate { int cfgIdx; int32_t rssi; };
    Candidate cands[kMaxCands];
    int nCands = 0;

    ESP_LOGI(TAG, "Scanning for %d configured network(s)...", count);
    int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false,
                                  /*passive=*/false, /*max_ms_per_chan=*/300);
    if (found > 0) {
        bool seen[kMaxCands] = {};
        for (int s = 0; s < found && nCands < kMaxCands; s++) {
            String scanned = WiFi.SSID(s);
            for (int i = 0; i < count && i < kMaxCands; i++) {
                if (!seen[i] && !ssids[i].isEmpty() && scanned == ssids[i]) {
                    cands[nCands++] = {i, WiFi.RSSI(s)};
                    seen[i] = true; // one entry per config SSID
                    break;
                }
            }
        }
        WiFi.scanDelete();
        // Insertion-sort by RSSI descending (strongest first)
        for (int i = 1; i < nCands; i++) {
            Candidate key = cands[i];
            int j = i - 1;
            while (j >= 0 && cands[j].rssi < key.rssi) { cands[j+1] = cands[j]; j--; }
            cands[j+1] = key;
        }
        ESP_LOGI(TAG, "Scan found %d match(es) from %d configured network(s)",
                 nCands, count);
    } else {
        WiFi.scanDelete();
        ESP_LOGW(TAG, "Scan returned %d — falling back to config order", found);
        // Build blind candidate list in config order
        for (int i = 0; i < count && i < kMaxCands; i++) {
            if (!ssids[i].isEmpty()) cands[nCands++] = {i, 0};
        }
    }

    // ── 3. Try each candidate in order ──────────────────────────────────────────
    auto _cacheAndReturn = [&](int cfgIdx) -> bool {
        rtc_channel = WiFi.channel();
        uint8_t* bssid = WiFi.BSSID();
        if (bssid) memcpy(rtc_bssid, bssid, 6);
        strncpy(rtc_cached_ssid, ssids[cfgIdx].c_str(), sizeof(rtc_cached_ssid) - 1);
        rtc_cached_ssid[sizeof(rtc_cached_ssid) - 1] = '\0';
        ESP_LOGI(TAG, "Connected to '%s'. IP: %s",
                 ssids[cfgIdx].c_str(), WiFi.localIP().toString().c_str());
        return true;
    };

    for (int c = 0; c < nCands; c++) {
        int i = cands[c].cfgIdx;
        ESP_LOGI(TAG, "Trying SSID '%s' (RSSI %d dBm)...",
                 ssids[i].c_str(), cands[c].rssi);
        WiFi.begin(ssids[i].c_str(), passes[i].c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - t0 > timeoutMs) {
                WiFi.disconnect(true);
                ESP_LOGW(TAG, "Timed out on '%s'", ssids[i].c_str());
                break;
            }
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED) return _cacheAndReturn(i);
    }

    ESP_LOGE(TAG, "Failed to connect to any of %d configured network(s)", count);
    rtc_channel = 0;
    return false;
}

#include "ConfigManager.h"
#include <algorithm>
#include <mbedtls/sha256.h>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    _mutex = xSemaphoreCreateMutex();
}

ConfigManager::~ConfigManager() {
    if (_mutex) vSemaphoreDelete(_mutex);
}

void ConfigManager::begin() {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);
        _provisioned = _prefs.getBool("provisioned", false);
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
}

bool ConfigManager::isProvisioned() const {
    return _provisioned;
}

void ConfigManager::setForceProvisioning(bool force) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);
        _prefs.putBool("force_prov", force);
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
}

bool ConfigManager::isForceProvisioning() const {
    bool force = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);
        force = _prefs.getBool("force_prov", false);
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
    return force;
}

WeatherConfig ConfigManager::load() const {
    WeatherConfig cfg;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);

        // ── WiFi Networks ─────────────────────────────────────────────────────
        // w_count == -1 means the key has never been written (old single-network
        // firmware). In that case migrate wifi_ssid / wifi_pass into slot 0.
        int wCount = _prefs.getInt("w_count", -1);
        if (wCount < 0) {
            // Legacy migration — pre-multi-WiFi firmware
            String legacySsid = _prefs.getString("wifi_ssid", "");
            String legacyPass = _prefs.getString("wifi_pass", "");
            if (!legacySsid.isEmpty()) {
                cfg.wifi_ssids[0]  = legacySsid;
                cfg.wifi_passes[0] = legacyPass;
                cfg.wifi_count     = 1;
            }
        } else {
            cfg.wifi_count = std::min(wCount, WeatherConfig::kMaxWifi);
            for (int i = 0; i < cfg.wifi_count; i++) {
                char sk[10], pk[10];
                snprintf(sk, sizeof(sk), "w_ssid_%d", i);
                snprintf(pk, sizeof(pk), "w_pass_%d", i);
                cfg.wifi_ssids[i]  = _prefs.getString(sk, "");
                cfg.wifi_passes[i] = _prefs.getString(pk, "");
            }
        }

        cfg.api_key     = _prefs.getString("api_key",    "");
        cfg.city        = _prefs.getString("city",       "");
        cfg.state       = _prefs.getString("state",      "");
        cfg.country     = _prefs.getString("country",    "");
        cfg.lat         = _prefs.getString("lat",        "");
        cfg.lon         = _prefs.getString("lon",        "");
        cfg.timezone    = _prefs.getString("timezone",   "CST6CDT,M3.2.0,M11.1.0");
        cfg.ntp_server  = _prefs.getString("ntp_server", "pool.ntp.org");
        cfg.sync_interval_m = _prefs.getInt("sync_interval", 30);
        cfg.webhook_url = _prefs.getString("webhook_url", "");
        cfg.pin_hash    = _prefs.getString("pin_hash",   "");
        _prefs.end();

        xSemaphoreGive(_mutex);
    }
    return cfg;
}


void ConfigManager::save(const WeatherConfig& cfg) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);

        // ── WiFi Networks ─────────────────────────────────────────────────────
        int count = std::min(cfg.wifi_count, WeatherConfig::kMaxWifi);
        _prefs.putInt("w_count", count);
        for (int i = 0; i < WeatherConfig::kMaxWifi; i++) {
            char sk[10], pk[10];
            snprintf(sk, sizeof(sk), "w_ssid_%d", i);
            snprintf(pk, sizeof(pk), "w_pass_%d", i);
            if (i < count) {
                _prefs.putString(sk, cfg.wifi_ssids[i]);
                _prefs.putString(pk, cfg.wifi_passes[i]);
            } else {
                // Erase any stale entries from a previous save with more networks
                _prefs.remove(sk);
                _prefs.remove(pk);
            }
        }
        // Keep legacy key for potential rollback compatibility
        if (count > 0) {
            _prefs.putString("wifi_ssid", cfg.wifi_ssids[0]);
            _prefs.putString("wifi_pass", cfg.wifi_passes[0]);
        } else {
            _prefs.remove("wifi_ssid");
            _prefs.remove("wifi_pass");
        }

        _prefs.putString("api_key",    cfg.api_key);
        _prefs.putString("city",       cfg.city);
        _prefs.putString("state",      cfg.state);
        _prefs.putString("country",    cfg.country);
        _prefs.putString("lat",        cfg.lat);
        _prefs.putString("lon",        cfg.lon);
        _prefs.putString("timezone",   cfg.timezone);
        _prefs.putString("ntp_server", cfg.ntp_server);
        _prefs.putInt("sync_interval", cfg.sync_interval_m);
        if (!cfg.webhook_url.isEmpty()) _prefs.putString("webhook_url", cfg.webhook_url);
        else _prefs.remove("webhook_url");
        _prefs.putString("pin_hash",   cfg.pin_hash);
        _prefs.putBool("provisioned",  true);
        _prefs.end();
        _provisioned = true;
        xSemaphoreGive(_mutex);
    }
}

void ConfigManager::clear() {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);
        _prefs.clear();
        _prefs.end();
        _provisioned = false;
        xSemaphoreGive(_mutex);
    }
}

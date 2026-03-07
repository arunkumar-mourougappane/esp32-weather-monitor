#include "ConfigManager.h"
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

WeatherConfig ConfigManager::load() const {
    WeatherConfig cfg;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/true);
        cfg.wifi_ssid   = _prefs.getString("wifi_ssid",  "");
        cfg.wifi_pass   = _prefs.getString("wifi_pass",  "");
        cfg.api_key     = _prefs.getString("api_key",    "");
        cfg.city        = _prefs.getString("city",       "");
        cfg.state       = _prefs.getString("state",      "");
        cfg.country     = _prefs.getString("country",    "");
        cfg.lat         = _prefs.getString("lat",        "");
        cfg.lon         = _prefs.getString("lon",        "");
        cfg.timezone    = _prefs.getString("timezone",   "UTC0");
        cfg.ntp_server  = _prefs.getString("ntp_server", "pool.ntp.org");
        cfg.pin_hash    = _prefs.getString("pin_hash",   "");
        _prefs.end();
        xSemaphoreGive(_mutex);
    }
    return cfg;
}

void ConfigManager::save(const WeatherConfig& cfg) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        _prefs.begin(kNamespace, /*readOnly=*/false);
        _prefs.putString("wifi_ssid",  cfg.wifi_ssid);
        _prefs.putString("wifi_pass",  cfg.wifi_pass);
        _prefs.putString("api_key",    cfg.api_key);
        _prefs.putString("city",       cfg.city);
        _prefs.putString("state",      cfg.state);
        _prefs.putString("country",    cfg.country);
        _prefs.putString("lat",        cfg.lat);
        _prefs.putString("lon",        cfg.lon);
        _prefs.putString("timezone",   cfg.timezone);
        _prefs.putString("ntp_server", cfg.ntp_server);
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

#pragma once
#include <Arduino.h>
#include <Preferences.h>

/// All user-configurable settings stored in NVS.
struct WeatherConfig {
    String wifi_ssid;
    String wifi_pass;
    String api_key;       ///< Google Weather API key
    String city;          ///< Display city name
    String state;         ///< State or Province (optional)
    String country;       ///< ISO 3166-1 alpha-2 (e.g. "US")
    String lat;           ///< Latitude  (e.g. "41.8781")
    String lon;           ///< Longitude (e.g. "-87.6298")
    String timezone;      ///< POSIX TZ string (e.g. "CST6CDT,M3.2.0,M11.1.0")
    String ntp_server;    ///< NTP server URL or IP
    String pin_hash;      ///< SHA-256 hex of user PIN
};

/// Thread-safe NVS-backed configuration manager (singleton).
class ConfigManager {
public:
    static ConfigManager& getInstance();

    /// Must be called once from setup().
    void begin();

    /// Returns true if the device has completed provisioning.
    bool isProvisioned() const;

    /// Load all settings from NVS.
    WeatherConfig load() const;

    /// Persist all settings to NVS and mark as provisioned.
    void save(const WeatherConfig& cfg);

    /// Erase all stored config (factory reset).
    void clear();

private:
    ConfigManager() = default;

    mutable Preferences _prefs;
    bool _provisioned = false;

    static constexpr const char* kNamespace = "wcfg";
};

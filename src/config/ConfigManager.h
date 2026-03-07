#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct WeatherConfig {
    String wifi_ssid;
    String wifi_pass;
    String api_key;       ///< Google Weather API key
    String city;          ///< Display city name
    String state;         ///< State or Province (optional)
    String country;       ///< ISO 3166-1 alpha-2 (e.g. "US")
    String lat;           ///< Latitude  (e.g. "41.8781")
    String lon;           ///< Longitude (e.g. "-87.6298")
    String timezone;      ///< POSIX timezone string (e.g. "CST6CDT,M3.2.0,M11.1.0")
    String ntp_server;
    String pin_hash;      ///< SHA-256 hash of the 4-8 digit PIN
};

/// Handles loading and saving user configuration to non-volatile storage (NVS).
class ConfigManager {
public:
    static ConfigManager& getInstance();

    /// Initialize NVS and read the 'provisioned' flag.
    void begin();

    /// Returns true if the device has completed provisioning.
    bool isProvisioned() const;

    /// Set a flag to force provisioning mode on next boot
    void setForceProvisioning(bool force);

    /// Check if provisioning mode is forced
    bool isForceProvisioning() const;

    /// Load all settings from NVS.
    WeatherConfig load() const;

    /// Persist all settings to NVS and mark as provisioned.
    void save(const WeatherConfig& cfg);

    /// Erase all stored config (factory reset).
    void clear();

private:
    ConfigManager();
    ~ConfigManager();

    mutable Preferences _prefs;
    SemaphoreHandle_t _mutex;
    bool _provisioned = false;

    static constexpr const char* kNamespace = "wcfg";
};

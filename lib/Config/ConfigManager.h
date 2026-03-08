/**
 * @file ConfigManager.h
 * @brief Non-volatile storage (NVS) backed configuration manager.
 *
 * Provides thread-safe read/write access to the device's persistent settings,
 * which include Wi-Fi credentials, the Google Weather API key, location
 * parameters (lat/lon, city, state, country), timezone, NTP server URL, and
 * an optional SHA-256 hashed PIN.
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @struct WeatherConfig
 * @brief Plain-data bag holding all user-configurable settings.
 */
struct WeatherConfig {
    String wifi_ssid;           ///< Wi-Fi SSID to connect to in station mode.
    String wifi_pass;           ///< Wi-Fi password (WPA2).
    String api_key;             ///< Google Weather API key.
    String city;                ///< Human-readable city name for the display.
    String state;               ///< State or province abbreviation (optional, e.g. "IL").
    String country;             ///< ISO 3166-1 alpha-2 country code (e.g. "US").
    String lat;                 ///< Latitude in decimal degrees (e.g. "41.8781").
    String lon;                 ///< Longitude in decimal degrees (e.g. "-87.6298").
    String timezone;            ///< POSIX TZ string (e.g. "CST6CDT,M3.2.0,M11.1.0").
    String ntp_server;          ///< Hostname or IP of the NTP server.
    String pin_hash;            ///< SHA-256 hex digest of the 4–8 digit security PIN.
};

/**
 * @class ConfigManager
 * @brief Singleton that persists WeatherConfig to ESP32 NVS via Preferences.
 *
 * All public methods are protected by an internal FreeRTOS mutex, making them
 * safe to call from multiple tasks.
 *
 * Example:
 * @code
 *   ConfigManager::getInstance().begin();
 *   if (ConfigManager::getInstance().isProvisioned()) {
 *       WeatherConfig cfg = ConfigManager::getInstance().load();
 *   }
 * @endcode
 */
class ConfigManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single ConfigManager object.
     */
    static ConfigManager& getInstance();

    /**
     * @brief Open the NVS namespace and read the provisioned flag.
     *
     * Must be called once before any other method.
     */
    void begin();

    /**
     * @brief Check if the device has been provisioned.
     * @return @c true if a valid configuration has been saved to NVS.
     */
    bool isProvisioned() const;

    /**
     * @brief Override the provisioned flag to force the provisioning portal.
     * @param force  Set @c true to force provisioning on the next reboot.
     */
    void setForceProvisioning(bool force);

    /**
     * @brief Query whether provisioning was forced by the user or code.
     * @return @c true if the force-provisioning flag is set in NVS.
     */
    bool isForceProvisioning() const;

    /**
     * @brief Load all settings from NVS.
     * @return A WeatherConfig struct populated from persisted values.
     *         Fields will be empty strings if not previously saved.
     */
    WeatherConfig load() const;

    /**
     * @brief Persist a WeatherConfig to NVS and mark the device as provisioned.
     * @param cfg  The configuration to save.
     */
    void save(const WeatherConfig& cfg);

    /**
     * @brief Erase all stored configuration (factory reset).
     *
     * Clears the NVS namespace and resets the provisioned flag to false.
     */
    void clear();

private:
    ConfigManager();
    ~ConfigManager();

    mutable Preferences _prefs;   ///< Underlying Preferences (NVS) handle.
    SemaphoreHandle_t _mutex;     ///< Protects concurrent NVS access.
    bool _provisioned = false;    ///< Cached provisioned flag.

    static constexpr const char* kNamespace = "wcfg"; ///< NVS namespace key.
};

#endif // CONFIG_MANAGER_H

/**
 * @file WiFiManager.h
 * @brief ESP32 wireless connectivity manager supporting SoftAP and Station modes.
 *
 * WiFiManager is a thin singleton wrapper around the Arduino WiFi library that
 * provides a clean, blocking API for provisioning (SoftAP) and normal internet
 * access (STA) without requiring the caller to manage WiFi event callbacks.
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include <Arduino.h>
#include <WiFi.h>
#include <SystemState.h>

/**
 * @class WiFiManager
 * @brief Singleton that abstracts SoftAP and Station mode wireless operations.
 *
 * Typical lifecycle during provisioning:
 * @code
 *   WiFiManager::getInstance().startAP("M5Paper-Setup");
 *   // … serve the provisioning portal …
 *   WiFiManager::getInstance().stopAP();
 * @endcode
 *
 * Typical lifecycle during normal operation:
 * @code
 *   bool ok = WiFiManager::getInstance().connectSTA(ssid, pass);
 * @endcode
 */
class WiFiManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single WiFiManager object.
     */
    static WiFiManager& getInstance();

    /**
     * @brief Bind the WiFiManager to the shared RTC state.
     *
     * Must be called once before connectSTA() or connectBestSTA() so that
     * the fast-connect BSSID/channel cache reads from and writes to the
     * single RTC_DATA_ATTR SystemState g_state rather than private globals.
     *
     * @param state  Reference to the application-wide RTC state (g_state).
     */
    void begin(SystemState& state);

    /**
     * @brief Start a Wi-Fi SoftAP.
     *
     * Opens a password-protected (WPA2) or open access point depending on
     * whether @p password is non-empty.
     *
     * @param ssid      Network name to broadcast.
     * @param password  WPA2 passphrase; pass an empty string for an open AP.
     * @return          @c true if the AP started successfully.
     */
    bool startAP(const String& ssid, const String& password = "");

    /**
     * @brief Tear down the SoftAP and release related resources.
     */
    void stopAP();

    /**
     * @brief Connect to a Wi-Fi network in Station mode.
     *
     * Blocks until the ESP32 obtains an IP address or until @p timeoutMs
     * milliseconds have elapsed.
     *
     * @param ssid       Target network SSID.
     * @param password   WPA2 passphrase.
     * @param timeoutMs  Maximum time to wait for an IP (default 15 000 ms).
     * @return           @c true if connected and an IP was assigned.
     */
    bool connectSTA(const String& ssid, const String& password,
                    uint32_t timeoutMs = 15000);

    /**
     * @brief Connect to the best available network from a list of candidates.
     *
     * 1. If the previously connected SSID (cached in RTC) is present in the
     *    list, attempts a fast-connect using the cached BSSID + channel first.
     * 2. Performs an active WiFi scan and ranks matching SSIDs by RSSI.
     * 3. Tries each candidate in signal-strength order until one succeeds.
     * 4. Falls back to a blind connect (config order) when no APs were found
     *    in the scan (e.g. hidden SSID, scan error).
     *
     * The BSSID, channel, and SSID of the successfully connected network are
     * cached in @c RTC_DATA_ATTR so the next call can use fast-connect.
     *
     * @param ssids      Array of SSID strings (length @p count).
     * @param passes     Array of passwords parallel to @p ssids.
     * @param count      Number of valid entries in the arrays.
     * @param timeoutMs  Per-network connect timeout in milliseconds (default 15 000 ms).
     * @return           @c true if connected to any network and an IP was assigned.
     */
    bool connectBestSTA(const String* ssids, const String* passes, int count,
                        uint32_t timeoutMs = 15000);

    /**
     * @brief Query the current station connection state.
     * @return @c true if the STA interface is associated and has a valid IP.
     */
    bool isConnected() const;

    /**
     * @brief Retrieve the Station IP address as a dotted-decimal string.
     * @return IP string (e.g. @c "192.168.1.42"), or @c "" if not connected.
     */
    String getStaIP() const;

    /**
     * @brief Retrieve the SoftAP IP address as a dotted-decimal string.
     * @return IP string (default @c "192.168.4.1").
     */
    String getApIP() const;

private:
    WiFiManager() = default;

    SystemState* _pState = nullptr; ///< Pointer to shared RTC state; set by begin().
};

#endif // WIFI_MANAGER_H

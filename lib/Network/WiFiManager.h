#pragma once
#include <Arduino.h>
#include <WiFi.h>

/// Manages ESP32 wireless operation: SoftAP for provisioning and
/// station mode for normal operation.
class WiFiManager {
public:
    static WiFiManager& getInstance();

    /// Start a SoftAP.  Pass empty password for open network.
    bool startAP(const String& ssid, const String& password = "");

    /// Tear down the SoftAP.
    void stopAP();

    /// Connect as a station.  Blocks until connected or timeout.
    /// @return true on success.
    bool connectSTA(const String& ssid, const String& password,
                    uint32_t timeoutMs = 15000);

    /// @return true if STA is associated and has an IP.
    bool isConnected() const;

    /// @return current STA IP address string, or "" if not connected.
    String getStaIP() const;

    /// @return SoftAP IP address string (default 192.168.4.1).
    String getApIP() const;

private:
    WiFiManager() = default;
};

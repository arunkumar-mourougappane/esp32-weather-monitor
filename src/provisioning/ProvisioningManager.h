#pragma once
#include <Arduino.h>

/// Orchestrates the first-boot (or forced) provisioning flow:
///   1. Start SoftAP
///   2. Show QR code on e-ink
///   3. Run AsyncWebServer
///   4. On save → persist config to NVS → restart
class ProvisioningManager {
public:
    static ProvisioningManager& getInstance();

    /// Set everything up.  Call once from setup() when provisioning is needed.
    void begin();

    /// Pump the provisioning loop (call from loop() — just feeds delay/yield,
    /// AsyncWebServer handles requests via its own tasks).
    void run();

    /// @return true after the user submitted the form and the ESP is about
    ///         to restart (i.e. you may stop calling run()).
    bool isDone() const { return _done; }

private:
    ProvisioningManager() = default;
    bool _done = false;

    static constexpr const char* kSoftApSSID = "M5Paper-Setup";
};

#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

/// Callback invoked when the user submits the provisioning form.
using ProvisionSaveCallback =
    std::function<void(const String& ssid, const String& pass,
                       const String& apiKey, const String& city,
                       const String& country, const String& lat,
                       const String& lon,   const String& tz,
                       const String& ntp,   const String& pinHash)>;

/// Lightweight async HTTP server that serves the provisioning page and
/// processes the save form submission.
class ProvisionWebServer {
public:
    static ProvisionWebServer& getInstance();

    /// Start the web server on the given port (default 80).
    void begin(uint16_t port = 80);

    /// Stop and destroy the server.
    void stop();

    /// Register the callback invoked on successful form submission.
    void onSave(ProvisionSaveCallback cb) { _saveCallback = std::move(cb); }

private:
    ProvisionWebServer() = default;

    AsyncWebServer* _server = nullptr;
    ProvisionSaveCallback _saveCallback;

    /// SHA-256 helper (uses mbedTLS).
    static String _sha256(const String& input);
};

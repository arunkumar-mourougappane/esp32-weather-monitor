/**
 * @file WebServer.h
 * @brief Async HTTP server that serves the provisioning web portal.
 *
 * ProvisionWebServer uses ESPAsyncWebServer to:
 *  - Serve the embedded HTML/CSS provisioning form (`html/provision.h`).
 *  - Handle the POST form submission, SHA-256 the PIN, and invoke a registered
 *    callback so ProvisioningManager can persist the settings via ConfigManager.
 *
 * All route handlers run in the async-web-server task context, so the
 * registered ProvisionSaveCallback must be safe to execute there.
 */
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

/**
 * @typedef ProvisionSaveCallback
 * @brief Invoked when the user successfully submits the provisioning form.
 *
 * All parameters are the raw (or hashed, in the case of pinHash) strings
 * extracted from the HTTP POST body.
 *
 * @param ssid     Wi-Fi SSID entered by the user.
 * @param pass     Wi-Fi password.
 * @param apiKey   Google Weather API key.
 * @param city     City name for display.
 * @param state    State/province abbreviation (may be empty).
 * @param country  ISO 3166-1 alpha-2 country code.
 * @param lat      Latitude string.
 * @param lon      Longitude string.
 * @param tz       POSIX timezone string.
 * @param ntp      NTP server hostname.
 * @param pinHash  SHA-256 hex digest of the user-entered PIN.
 */
using ProvisionSaveCallback =
    std::function<void(const String& ssid, const String& pass,
                       const String& apiKey, const String& city,
                       const String& state,
                       const String& country, const String& lat,
                       const String& lon,   const String& tz,
                       const String& ntp,   const String& pinHash)>;

/**
 * @class ProvisionWebServer
 * @brief Singleton async HTTP server for the device provisioning portal.
 *
 * Routes registered:
 *  - `GET  /`      – serves the embedded HTML provisioning form.
 *  - `POST /save`  – processes the submitted form data and fires the callback.
 *
 * Example:
 * @code
 *   ProvisionWebServer& ws = ProvisionWebServer::getInstance();
 *   ws.onSave([](auto ssid, auto pass, ...) { ConfigManager::getInstance().save(...); });
 *   ws.begin();
 * @endcode
 */
class ProvisionWebServer {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single ProvisionWebServer object.
     */
    static ProvisionWebServer& getInstance();

    /**
     * @brief Start the async HTTP server and register all routes.
     * @param port  TCP port to listen on (default 80).
     */
    void begin(uint16_t port = 80);

    /**
     * @brief Stop the server and free its resources.
     */
    void stop();

    /**
     * @brief Register the callback to invoke on a successful form submission.
     * @param cb  Callable matching the ProvisionSaveCallback signature.
     */
    void onSave(ProvisionSaveCallback cb) { _saveCallback = std::move(cb); }

private:
    ProvisionWebServer() = default;

    AsyncWebServer* _server = nullptr;         ///< Underlying async server instance.
    ProvisionSaveCallback _saveCallback;       ///< User-registered save handler.

    /**
     * @brief Compute SHA-256 of @p input using mbedTLS.
     * @param input  Arbitrary string to hash.
     * @return       Lowercase hex-encoded 64-character digest.
     */
    static String _sha256(const String& input);
};

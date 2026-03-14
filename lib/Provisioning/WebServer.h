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
#ifndef WEB_SERVER_H
#define WEB_SERVER_H
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ConfigManager.h>
#include <functional>

/**
 * @typedef ProvisionSaveCallback
 * @brief Invoked when the user successfully submits the provisioning form.
 *
 * Receives a fully-validated @c WeatherConfig (including SHA-256 PIN hash
 * and all WiFi networks) ready to be persisted by ConfigManager.
 *
 * @param cfg  Validated configuration built from the HTTP POST body.
 */
using ProvisionSaveCallback = std::function<void(const WeatherConfig& cfg)>;

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

#endif // WEB_SERVER_H

/**
 * @file ProvisioningManager.h
 * @brief Orchestrates the first-boot captive-portal provisioning flow.
 *
 * When the device is unprovisioned (or the user forces reprovisioning via
 * G38), ProvisioningManager coordinates all subsystems required for the
 * guided setup experience:
 *  1. Starts a SoftAP named "M5Paper-Setup".
 *  2. Shows a QR code and URL on the e-ink display.
 *  3. Runs the async HTTP server (ProvisionWebServer) on port 80.
 *  4. When the user submits the form, persists config to NVS via ConfigManager
 *     and triggers a device restart.
 */
#pragma once
#include <Arduino.h>

/**
 * @class ProvisioningManager
 * @brief Singleton that runs the captive-portal provisioning experience.
 *
 * Intended usage from setup():
 * @code
 *   ProvisioningManager& pm = ProvisioningManager::getInstance();
 *   pm.begin();
 *   while (!pm.isDone()) {
 *       pm.run();
 *   }
 * @endcode
 *
 * @note After isDone() returns @c true the ESP32 will restart automatically;
 *       the caller should stop calling run() but need not take any other action.
 */
class ProvisioningManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single ProvisioningManager object.
     */
    static ProvisioningManager& getInstance();

    /**
     * @brief Initialise all provisioning subsystems.
     *
     * Starts the SoftAP, registers web-server routes, and renders the QR code
     * screen.  Must be called once before the run() loop.
     */
    void begin();

    /**
     * @brief Yield processing time to the provisioning loop.
     *
     * Because ESPAsyncWebServer handles HTTP requests from its own internal
     * FreeRTOS tasks, this method simply calls `delay(10)` / `yield()` to
     * prevent the watchdog timer from triggering while waiting for the user.
     * Call this repeatedly from loop() until isDone() returns true.
     */
    void run();

    /**
     * @brief Check if provisioning is complete and a restart is pending.
     * @return @c true once the user has submitted the form and config is saved.
     */
    bool isDone() const { return _done; }

private:
    ProvisioningManager() = default;

    bool _done = false; ///< Becomes true after a successful form submission.

    /** @brief SSID of the provisioning SoftAP network. */
    static constexpr const char* kSoftApSSID = "M5Paper-Setup";
};

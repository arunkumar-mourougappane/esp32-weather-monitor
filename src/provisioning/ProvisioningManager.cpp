#include "ProvisioningManager.h"
#include "WebServer.h"
#include "../config/ConfigManager.h"
#include "../display/DisplayManager.h"
#include "../network/WiFiManager.h"
#include <esp_log.h>

static const char* TAG = "ProvisioningManager";

ProvisioningManager& ProvisioningManager::getInstance() {
    static ProvisioningManager instance;
    return instance;
}

void ProvisioningManager::begin() {
    ESP_LOGI(TAG, "Starting provisioning mode");

    // 1. Start SoftAP (open network so user can scan QR and connect easily)
    auto& wifi = WiFiManager::getInstance();
    wifi.startAP(kSoftApSSID);

    String apIP  = wifi.getApIP();          // typically 192.168.4.1
    String apUrl = "http://" + apIP;

    // 2. Display QR code + instructions on e-ink
    DisplayManager::getInstance().showProvisioningScreen(kSoftApSSID, apUrl);

    // 3. Configure the web server's save callback
    ProvisionWebServer::getInstance().onSave(
        [this](const String& ssid, const String& pass,
               const String& apiKey, const String& city,
               const String& country, const String& lat,
               const String& lon,   const String& tz,
               const String& ntp,   const String& pinHash)
        {
            WeatherConfig cfg;
            cfg.wifi_ssid  = ssid;
            cfg.wifi_pass  = pass;
            cfg.api_key    = apiKey;
            cfg.city       = city;
            cfg.country    = country;
            cfg.lat        = lat;
            cfg.lon        = lon;
            cfg.timezone   = tz;
            cfg.ntp_server = ntp;
            cfg.pin_hash   = pinHash;

            ConfigManager::getInstance().save(cfg);
            ESP_LOGI(TAG, "Config saved — restarting in 2 s");
            _done = true;

            // Give the HTTP response time to reach the browser
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }
    );

    // 4. Start web server
    ProvisionWebServer::getInstance().begin(80);

    ESP_LOGI(TAG, "AP SSID: %s  IP: %s", kSoftApSSID, apIP.c_str());
}

void ProvisioningManager::run() {
    // AsyncWebServer runs in its own FreeRTOS tasks; we just yield here.
    // A watchdog-friendly delay also handles background tasks.
    delay(10);
}

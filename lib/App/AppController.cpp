#include "AppController.h"
#include <ConfigManager.h>
#include <DisplayManager.h>
#include <WiFiManager.h>
#include <NTPManager.h>
#include <WeatherService.h>
#include <InputManager.h>
#include <ProvisioningManager.h>
#include <esp_log.h>

static const char* TAG = "AppController";

// ── Weather fetch interval: 10 minutes ────────────────────────────────────────
static constexpr uint32_t kWeatherIntervalMs = 10UL * 60UL * 1000UL;

// ── NTP re-sync interval: 1 hour ─────────────────────────────────────────────
static constexpr uint32_t kNtpIntervalMs = 60UL * 60UL * 1000UL;

AppController& AppController::getInstance() {
    static AppController instance;
    return instance;
}

void AppController::begin() {
    _weatherMutex = xSemaphoreCreateMutex();
    configASSERT(_weatherMutex);

    // Weather task — core 0, priority 2
    xTaskCreatePinnedToCore(_weatherTaskFn, "WeatherTask",
                            8192, this, 2, &_weatherTask, 0);

    // Display task — core 1, priority 3
    xTaskCreatePinnedToCore(_displayTaskFn, "DisplayTask",
                            8192, this, 3, &_displayTask, 1);

    // NTP re-sync task — core 0, priority 1
    xTaskCreatePinnedToCore(_ntpTaskFn, "NtpTask",
                            4096, this, 1, &_ntpTask, 0);

    ESP_LOGI(TAG, "All tasks started");
}

WeatherData AppController::getWeather() {
    WeatherData data;
    if (xSemaphoreTake(_weatherMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        data = _weather;
        xSemaphoreGive(_weatherMutex);
    }
    return data;
}

void AppController::requestDisplayUpdate() {
    _displayDirty = true;
}

// ── Weather Task ──────────────────────────────────────────────────────────────
void AppController::_weatherTaskFn(void* param) {
    auto* self = static_cast<AppController*>(param);
    auto cfg   = ConfigManager::getInstance().load();
    auto& svc  = WeatherService::getInstance();

    for (;;) {
        if (WiFiManager::getInstance().isConnected()) {
            ESP_LOGI(TAG, "Fetching weather...");
            WeatherData data = svc.fetch(cfg.lat, cfg.lon, cfg.api_key);

            if (xSemaphoreTake(self->_weatherMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                self->_weather = data;
                xSemaphoreGive(self->_weatherMutex);
            }
            self->_displayDirty = true;
        } else {
            ESP_LOGW(TAG, "No WiFi — skipping weather fetch");
        }
        vTaskDelay(pdMS_TO_TICKS(kWeatherIntervalMs));
    }
}

// ── Display Task ──────────────────────────────────────────────────────────────
void AppController::_displayTaskFn(void* param) {
    auto* self  = static_cast<AppController*>(param);
    auto& disp  = DisplayManager::getInstance();
    auto  cfgd  = ConfigManager::getInstance().load();
    auto& input = InputManager::getInstance();

    String locationStr = cfgd.city;
    if (cfgd.state.length() > 0) {
        locationStr += ", " + cfgd.state;
    }

    // Show a loading screen immediately — WeatherTask is already running
    disp.showLoadingScreen(locationStr);

    int lastMinute  = -1;  // track minute-boundary for clock ticks
    bool hasWeather = false;

    for (;;) {
        // ── G38 re-provisioning trigger ──────────────────────────────
        if (input.isProvisioningTriggered()) {
            ESP_LOGW(TAG, "Provisioning re-trigger detected — restarting");
            input.clearProvisioningTrigger();
            ConfigManager::getInstance().setForceProvisioning(true);
            delay(500);
            esp_restart();
        }

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);
        int currentMinute = localTime.tm_min;

        bool swiped = false;
        if (input.checkSwipeLeft()) {
            if (self->_forecastOffset < self->getWeather().forecastDays - 3) {
                self->_forecastOffset++;
                swiped = true;
            }
        } else if (input.checkSwipeRight()) {
            if (self->_forecastOffset > 0) {
                self->_forecastOffset--;
                swiped = true;
            }
        }

        if (self->_displayDirty) {
            // New weather data arrived — full-quality redraw screen
            self->_displayDirty = false;
            hasWeather = true;
            WeatherData wd = self->getWeather();
            disp.showWeatherUI(wd, localTime, locationStr, /*fastMode=*/false, self->_forecastOffset);
            lastMinute = currentMinute;

        } else if (swiped && hasWeather) {
            // User scrolled — do a fast partial update of just the forecast region
            WeatherData wd = self->getWeather();
            disp.updateForecastUI(wd, self->_forecastOffset);

        } else if (hasWeather && currentMinute != lastMinute) {
            // Just the clock changed — fast partial refresh of the upper UI
            WeatherData wd = self->getWeather();
            disp.showWeatherUI(wd, localTime, locationStr, /*fastMode=*/true, self->_forecastOffset);
            lastMinute = currentMinute;
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // poll every 5 s
    }
}

// ── NTP Re-sync Task ──────────────────────────────────────────────────────────
void AppController::_ntpTaskFn(void* param) {
    auto  cfg = ConfigManager::getInstance().load();
    auto& ntp = NTPManager::getInstance();

    // Initial sync is already done in setup(); wait before re-syncing
    vTaskDelay(pdMS_TO_TICKS(kNtpIntervalMs));

    for (;;) {
        if (WiFiManager::getInstance().isConnected()) {
            ESP_LOGI(TAG, "Re-syncing NTP...");
            ntp.sync(cfg.ntp_server, cfg.timezone);
        }
        vTaskDelay(pdMS_TO_TICKS(kNtpIntervalMs));
    }
}

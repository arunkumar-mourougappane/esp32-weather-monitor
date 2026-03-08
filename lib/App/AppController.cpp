#include "AppController.h"
#include <ConfigManager.h>
#include <DisplayManager.h>
#include <WiFiManager.h>
#include <NTPManager.h>
#include <WeatherService.h>
#include <InputManager.h>
#include <ProvisioningManager.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

static const char* TAG = "AppController";

// ── RTC Memory Data (Survives Deep Sleep) ────────────────────────────────────
RTC_DATA_ATTR WeatherData rtcCachedWeather;
RTC_DATA_ATTR int         rtcForecastOffset = 0;

// ── Configuration ────────────────────────────────────────────────────────────
static constexpr uint64_t kSleepDurationUs     = 30ULL * 60ULL * 1000000ULL; // 30 minutes
static constexpr uint32_t kInteractiveTimeoutMs= 10UL * 60UL * 1000UL;      // 10 minutes

AppController& AppController::getInstance() {
    static AppController instance;
    return instance;
}

void AppController::begin() {
    auto wakeup_reason = esp_sleep_get_wakeup_cause();
    auto cfg           = ConfigManager::getInstance().load();
    auto& disp         = DisplayManager::getInstance();

    String locationStr = cfg.city;
    if (cfg.state.length() > 0) {
        locationStr += ", " + cfg.state;
    }

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // ── 1. Woken by physical button (G38) ────────
        ESP_LOGI(TAG, "Woken by EXT0 (Button) - entering 10m interactive mode");
        
        // Force fully-qualified timezone initialization for accurate offline RTC parsing
        setenv("TZ", cfg.timezone.c_str(), 1);
        tzset();

        // Render whatever is currently in RTC memory immediately (fast refresh)
        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);

        if (rtcCachedWeather.valid) {
            disp.showWeatherUI(rtcCachedWeather, localTime, locationStr, /*fastMode=*/true, rtcForecastOffset);
        }

        // Run the interactive loop for X minutes
        _runInteractiveSession(locationStr);

    } else {
        // ── 2. Woken by Timer OR Power-On ───────────
        ESP_LOGI(TAG, "Woken by Timer/Reset - fetching background update");
        
        if (!rtcCachedWeather.valid) {
            disp.showLoadingScreen(locationStr);
        }

        if (WiFiManager::getInstance().isConnected() || WiFiManager::getInstance().connectSTA(cfg.wifi_ssid, cfg.wifi_pass)) {
            NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
            
            WeatherData data = WeatherService::getInstance().fetch(cfg.lat, cfg.lon, cfg.api_key);
            if (data.valid) {
                rtcCachedWeather = data; // persist to RTC
            }
        }

        if (rtcCachedWeather.valid) {
            struct tm localTime = {};
            NTPManager::getInstance().getLocalTime(localTime);
            // Full quality screen redraw
            disp.showWeatherUI(rtcCachedWeather, localTime, locationStr, /*fastMode=*/false, rtcForecastOffset);
        } else {
            disp.showMessage("Network Error", "Unable to fetch data");
        }
        
        // Immediately go back to sleep
        enterDeepSleep();
    }
}

void AppController::_runInteractiveSession(const String& locationStr) {
    auto& disp  = DisplayManager::getInstance();
    auto  cfg   = ConfigManager::getInstance().load();
    auto& input = InputManager::getInstance();

    uint32_t lastActivityMs = millis();
    int lastMinute = -1;

    while ((millis() - lastActivityMs) < kInteractiveTimeoutMs) {
        bool activity = false;

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);
        if (lastMinute == -1) lastMinute = localTime.tm_min;

        // Force UI redraw if clock minute rolls over natively
        if (localTime.tm_min != lastMinute) {
            lastMinute = localTime.tm_min;
            activity = true;
        }

        // Provisioning Trigger (Hold 10s is handled internally by InputManager)
        if (input.isProvisioningTriggered()) {
            ESP_LOGW(TAG, "Provisioning re-trigger detected");
            input.clearProvisioningTrigger();
            ConfigManager::getInstance().setForceProvisioning(true);
            delay(500);
            esp_restart();
        }

        // Handle native capacitive swiping OR wheel scrolling
        if (input.checkSwipeLeft()) {
            if (rtcForecastOffset < rtcCachedWeather.forecastDays - 3) {
                rtcForecastOffset++;
                activity = true;
            }
        } else if (input.checkSwipeRight()) {
            if (rtcForecastOffset > 0) {
                rtcForecastOffset--;
                activity = true;
            }
        }

        if (activity) {
            lastActivityMs = millis();
            disp.showWeatherUI(rtcCachedWeather, localTime, locationStr, true, rtcForecastOffset);
        }

        delay(50); // small pump delay
    }

    ESP_LOGI(TAG, "Interactive session timed out. Returning to sleep.");
    enterDeepSleep();
}

void AppController::enterDeepSleep() {
    ESP_LOGI(TAG, "Configuring Deep Sleep boundaries...");
    
    // Enable 30-minute timer wakeup
    esp_sleep_enable_timer_wakeup(kSleepDurationUs);

    // Enable EXT0 physical button wakeup mapped to G38 (Push Button) 
    constexpr gpio_num_t ext_wakeup_pin = GPIO_NUM_38;
    rtc_gpio_pullup_en(ext_wakeup_pin);
    rtc_gpio_pulldown_dis(ext_wakeup_pin);
    esp_sleep_enable_ext0_wakeup(ext_wakeup_pin, 0); // 0 = wakeup on LOW

    ESP_LOGI(TAG, "Entering Deep Sleep now! -> Zzz");
    
    // Shut down gracefully
    delay(500); // flush UART
    esp_deep_sleep_start();
}

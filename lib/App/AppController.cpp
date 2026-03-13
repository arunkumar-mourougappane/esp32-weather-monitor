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
RTC_DATA_ATTR uint32_t    rtcWakeupCount = 0;
RTC_DATA_ATTR Page        rtcActivePage = Page::Dashboard;
RTC_DATA_ATTR int         rtcSettingsCursor = 0;

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
            disp.setActivePage(rtcActivePage); 
            disp.renderActivePage(rtcCachedWeather, localTime, locationStr, /*fastMode=*/true, rtcForecastOffset, rtcSettingsCursor);
        } else {
            // No cached data yet — avoid showing the misleading "Fetching weather"
            // placeholder, which implies a fetch is in progress when it is not.
            disp.showMessage("No Data Yet", "Weather syncs every 30 min");
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
            if (rtcWakeupCount % 48 == 0) {
                ESP_LOGI(TAG, "Executing 24-hour NTP Sync (iteration %lu)", (unsigned long)rtcWakeupCount);
                NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
            } else {
                ESP_LOGI(TAG, "Bypassing NTP Sync (Hardware RTC BM8563 active, iteration %lu)", (unsigned long)rtcWakeupCount);
                setenv("TZ", cfg.timezone.c_str(), 1);
                tzset();
            }
            rtcWakeupCount++;
            
            WeatherData data = WeatherService::getInstance().fetch(cfg.lat, cfg.lon, cfg.api_key);
            if (data.valid) {
                rtcCachedWeather = data; // persist to RTC
            }
        }

        if (rtcCachedWeather.valid) {
            struct tm localTime = {};
            NTPManager::getInstance().getLocalTime(localTime);
            // Full quality screen redraw
            disp.setActivePage(rtcActivePage);
            disp.renderActivePage(rtcCachedWeather, localTime, locationStr, /*fastMode=*/false, rtcForecastOffset, rtcSettingsCursor);
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
    uint32_t lastActivityMs = millis();
    int lastMinute = -1;
    auto& input = InputManager::getInstance();
    Page lastPage = disp.getActivePage();

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

        // Consolidate all buffered physical wheel/click events in one UI tick
        int upEvents   = input.checkScrollUp();
        int downEvents = input.checkScrollDown();
        int clickCount = input.checkClick();

        if (upEvents > 0) {
            for (int i = 0; i < upEvents; i++) {
                if (disp.getActivePage() == Page::Forecast && rtcForecastOffset < rtcCachedWeather.forecastDays - 3) {
                    rtcForecastOffset++;
                    activity = true;
                } else if (disp.getActivePage() == Page::Settings && rtcSettingsCursor < 2) {
                    rtcSettingsCursor++;
                    activity = true;
                }
            }
        }
        
        if (downEvents > 0) {
            for (int i = 0; i < downEvents; i++) {
                if (disp.getActivePage() == Page::Forecast && rtcForecastOffset > 0) {
                    rtcForecastOffset--;
                    activity = true;
                } else if (disp.getActivePage() == Page::Settings && rtcSettingsCursor > 0) {
                    rtcSettingsCursor--;
                    activity = true;
                }
            }
        }

        // Settings page: tap a menu row to trigger the action
        // Rows are laid out at startY=180, itemHeight=80; each row spans ±40px around its centre.
        constexpr int kSettingsStartY = 180;
        constexpr int kSettingsItemH  = 80;
        int tapX = 0, tapY = 0;
        if (input.checkTap(tapX, tapY) && disp.getActivePage() == Page::Settings) {
            for (int i = 0; i < 3; i++) {
                int cy = kSettingsStartY + i * kSettingsItemH;
                if (tapY >= cy - 40 && tapY < cy + 40) {
                    if (i == 0) {
                        ESP_LOGI(TAG, "Force Sync Triggered via tap");
                        disp.showMessage("Syncing...", "Fetching fresh weather data");
                        rtcWakeupCount = 0;
                        rtcCachedWeather.valid = false;
                        _enterDeepSleepForImmediateWakeup();
                    } else if (i == 1) {
                        ESP_LOGI(TAG, "Web Setup Triggered via tap");
                        disp.showMessage("Starting Setup", "Rebooting to portal...");
                        delay(1500);
                        ConfigManager::getInstance().setForceProvisioning(true);
                        esp_restart();
                    } else if (i == 2) {
                        ESP_LOGI(TAG, "Sleep Triggered via tap");
                        disp.showMessage("Going to Sleep", "Press G38 to wake");
                        delay(1500);
                        enterDeepSleep();
                    }
                    break;
                }
            }
        }

        if (clickCount > 0 && disp.getActivePage() != Page::Settings) {
            // Cycle pages on G38 click for non-settings views
            int nextPage = (static_cast<int>(disp.getActivePage()) + 1) % 3;
            disp.setActivePage(static_cast<Page>(nextPage));
            rtcActivePage = disp.getActivePage();
            activity = true;
        }

        // Touch Swipes for Page Navigation
        if (input.checkSwipeLeft()) {
            int nextPage = (static_cast<int>(disp.getActivePage()) + 1) % 3;
            disp.setActivePage(static_cast<Page>(nextPage));
            rtcActivePage = disp.getActivePage();
            activity = true;
        }
        if (input.checkSwipeRight()) {
            int prevPage = (static_cast<int>(disp.getActivePage()) + 2) % 3;
            disp.setActivePage(static_cast<Page>(prevPage));
            rtcActivePage = disp.getActivePage();
            activity = true;
        }

        if (activity) {
            lastActivityMs = millis();
            if (rtcCachedWeather.valid) {
                bool pageChanged = (disp.getActivePage() != lastPage);
                // If page changed, use high-quality refresh to clear the screen properly
                disp.renderActivePage(rtcCachedWeather, localTime, locationStr, !pageChanged, rtcForecastOffset, rtcSettingsCursor);
                lastPage = disp.getActivePage();
            }
        }

        delay(10); // increased polling frequency (100Hz)
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

void AppController::_enterDeepSleepForImmediateWakeup() {
    ESP_LOGI(TAG, "Force-sync sleep: waking in 1s to run fetch path");

    // 1-second timer so the normal timer-wakeup path (WiFi → fetch → render)
    // runs almost immediately instead of waiting the full 30-minute cycle.
    esp_sleep_enable_timer_wakeup(1ULL * 1000000ULL);

    constexpr gpio_num_t ext_wakeup_pin = GPIO_NUM_38;
    rtc_gpio_pullup_en(ext_wakeup_pin);
    rtc_gpio_pulldown_dis(ext_wakeup_pin);
    esp_sleep_enable_ext0_wakeup(ext_wakeup_pin, 0);

    delay(500);
    esp_deep_sleep_start();
}

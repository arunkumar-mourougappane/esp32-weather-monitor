#include "AppController.h"
#include <ConfigManager.h>
#include <DisplayManager.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPManager.h>
#include <WeatherService.h>
#include <InputManager.h>
#include <ProvisioningManager.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

static const char* TAG = "AppController";

/// @brief Structured error codes persisted to RTC so Settings page can report cause.
enum AppError : uint8_t {
    kErrNone        = 0x00, ///< No error.
    kErrWiFiFail    = 0x01, ///< Could not connect to configured SSID.
    kErrNtpFail     = 0x02, ///< NTP sync timed out; BM8563 RTC used as fallback.
    kErrWeatherFail = 0x03, ///< Weather API fetch returned no data.
    kErrLowBattery  = 0x04, ///< Battery below threshold; fetch was skipped.
};

// ── RTC Memory Data (Survives Deep Sleep) ────────────────────────────────────
RTC_DATA_ATTR WeatherData rtcCachedWeather;
RTC_DATA_ATTR int         rtcForecastOffset = 0;
RTC_DATA_ATTR uint32_t    rtcWakeupCount = 0;
RTC_DATA_ATTR Page        rtcActivePage = Page::Dashboard;
RTC_DATA_ATTR int         rtcSettingsCursor = 0;
RTC_DATA_ATTR char        rtcLastIP[16] = {};
RTC_DATA_ATTR uint8_t     rtcLastError = 0; ///< AppError code from most recent fetch cycle
RTC_DATA_ATTR uint8_t     rtcGhostCount = 0; ///< Full-quality redraw counter for ghost cleanup

// ── Configuration ────────────────────────────────────────────────────────────
static constexpr uint64_t kSleepDurationUs     = 30ULL * 60ULL * 1000000ULL; // 30 minutes
static constexpr uint32_t kInteractiveTimeoutMs= 10UL * 60UL * 1000UL;      // 10 minutes
static constexpr uint64_t kLowBatSleepUs       = 2ULL * 60ULL * 60ULL * 1000000ULL; // 2 hours
static constexpr int32_t  kLowBatThresholdMv   = 3500; // mV below which we skip fetch
static constexpr int      kForecastColsPerView  = 3;   // forecast columns visible at once

/// Configure G38 (GPIO_NUM_38) as EXT0 RTC wakeup source.
/// GPIO34–39 are input-only with no internal pull resistors; rtc_gpio_init() is
/// mandatory to transfer the pin from the GPIO-matrix domain into the RTC IO
/// domain before esp_sleep_enable_ext0_wakeup() can see it.
static void _configureEXT0Wakeup() {
    constexpr gpio_num_t kWakePin = GPIO_NUM_38;
    rtc_gpio_init(kWakePin);
    rtc_gpio_set_direction(kWakePin, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(kWakePin);
    esp_sleep_enable_ext0_wakeup(kWakePin, 0); // 0 = wakeup on LOW
}

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
            disp.setLastKnownIP(rtcLastIP);
            disp.setActivePage(rtcActivePage);
            disp.setLastError(rtcLastError);
            disp.setLastSyncTime(rtcCachedWeather.fetchTime);
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

        // ── Low-battery guard: skip WiFi radio and extend sleep to 2 hours ───
        // Sampling the 1/2 divider on GPIO 35 before powering up the WiFi radio
        // avoids draining the last 5 % and risking NVS corruption on brown-out.
        int32_t batMv = analogReadMilliVolts(35) * 2;
        if (batMv > 0 && batMv < kLowBatThresholdMv) {
            ESP_LOGW(TAG, "Low battery (%.2f V < %.2f V) — skipping fetch, extending sleep to 2h",
                     batMv / 1000.0f, kLowBatThresholdMv / 1000.0f);
            disp.showMessage("Low Battery",
                             String("Only ") + String(batMv / 1000.0f, 2) + " V — charge soon");
            // Extended sleep — skip WiFi radio entirely
            esp_sleep_enable_timer_wakeup(kLowBatSleepUs);
            _configureEXT0Wakeup();
            rtcLastError = kErrLowBattery;
            delay(2000);
            esp_deep_sleep_start();
        }
        
        if (!rtcCachedWeather.valid) {
            disp.showLoadingScreen(locationStr);
        } else {
            disp.showRefreshingBadge(); // brief badge — screen already shows cached data
        }

        if (WiFiManager::getInstance().isConnected() || WiFiManager::getInstance().connectBestSTA(cfg.wifi_ssids, cfg.wifi_passes, cfg.wifi_count)) {
            // Cache IP immediately after connect so it survives deep sleep
            strncpy(rtcLastIP, WiFi.localIP().toString().c_str(), sizeof(rtcLastIP) - 1);
            rtcLastIP[sizeof(rtcLastIP) - 1] = '\0';
            disp.updateLoadingStep(1); // WiFi connected — advance to NTP
            if (rtcWakeupCount % 48 == 0) {
                ESP_LOGI(TAG, "Executing 24-hour NTP Sync (iteration %lu)", (unsigned long)rtcWakeupCount);
                bool ntpOk = NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
                if (!ntpOk) rtcLastError = kErrNtpFail;
            } else {
                ESP_LOGI(TAG, "Bypassing NTP Sync (Hardware RTC BM8563 active, iteration %lu)", (unsigned long)rtcWakeupCount);
                setenv("TZ", cfg.timezone.c_str(), 1);
                tzset();
            }
            rtcWakeupCount++;

            disp.updateLoadingStep(2); // NTP done — advance to weather fetch
            WeatherData data = WeatherService::getInstance().fetch(cfg.lat, cfg.lon, cfg.api_key);
            if (data.valid) {
                rtcCachedWeather = data; // persist to RTC
                rtcLastError = NTPManager::getInstance().isNtpFailed() ? kErrNtpFail : kErrNone;
                disp.updateLoadingStep(3); // 100% bar — no-ops if no loading screen active
            } else {
                rtcLastError = kErrWeatherFail;
            }
        } else {
            rtcLastError = kErrWiFiFail;
        }

        // Propagate NTP failure status to display so badge appears on Dashboard
        disp.setNtpFailed(NTPManager::getInstance().isNtpFailed());
        disp.setLastError(rtcLastError);

        if (rtcCachedWeather.valid) {
            struct tm localTime = {};
            NTPManager::getInstance().getLocalTime(localTime);
            // Ghost cleanup cycle every 48 full-quality redraws to prevent e-ink artifact buildup.
            // Raised from 20 → 48 so the 1.3-second W→B→W flash doesn’t disrupt interactive scrolling.
            constexpr uint8_t kGhostCleanupInterval = 48;
            rtcGhostCount++;
            if (rtcGhostCount >= kGhostCleanupInterval) {
                rtcGhostCount = 0;
                ESP_LOGI(TAG, "Ghost cleanup threshold reached — running cleanup before redraw");
                disp.ghostingCleanup();
            }
            // Full quality screen redraw
            disp.setLastKnownIP(rtcLastIP);
            disp.setActivePage(rtcActivePage);
            disp.setLastSyncTime(rtcCachedWeather.fetchTime);
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
    bool overlayActive = false;
    bool overlayChanged = false;

    int consecutiveClicks = 0;
    uint32_t lastClickMs = 0;

    while ((millis() - lastActivityMs) < kInteractiveTimeoutMs) {
        bool activity = false;

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);
        if (lastMinute == -1) lastMinute = localTime.tm_min;

        // Force UI redraw if clock minute rolls over natively
        if (localTime.tm_min != lastMinute) {
            lastMinute = localTime.tm_min;
            // On the Dashboard, use a tight partial refresh for the clock strip only.
            // On other pages a full render doesn't tick, so just set activity to
            // update weather data-driven fields when the user navigates.
            if (disp.getActivePage() == Page::Dashboard && rtcCachedWeather.valid) {
                disp.updateClockOnly(localTime, NTPManager::getInstance().isNtpFailed());
                lastActivityMs = millis();
                continue;
            }
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
                if (disp.getActivePage() == Page::Forecast && rtcForecastOffset < rtcCachedWeather.forecastDays - kForecastColsPerView) {
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

        // G38 long-press (2–3 s) = force sync from any page
        if (input.checkLongPress()) {
            ESP_LOGI(TAG, "G38 long-press → force sync");
            disp.showMessage("Syncing...", "Fetching fresh weather data");
            rtcWakeupCount = 0;
            rtcCachedWeather.valid = false;
            _enterDeepSleepForImmediateWakeup();
        }

        // Settings page: tap a column icon to trigger the action.
        // Layout: 3 equal columns (180 px each); icon+label zone Y 200-370.
        constexpr int kSettingsColW    = 180; // kWidth / 3
        constexpr int kSettingsTapTop  = 200;
        constexpr int kSettingsTapBot  = 370;
        // Pagination dots at Y=940, spacing=24, centred on kWidth/2 over 4 pages
        constexpr int kDotY       = 940;
        constexpr int kDotSpacing = 24;
        constexpr int kDotHitR    = 24; // half the extended hit-zone width
        // Forecast scroll triangle hit zones (Y 820–860, X < 60 or X > 480)
        constexpr int kTriTop = 820;
        constexpr int kTriBot = 860;
        int tapX = 0, tapY = 0;
        if (input.checkTap(tapX, tapY)) {
            // ── Pagination dot tap — works on every page ──────────────────────
            if (tapY >= kDotY - kDotHitR && tapY <= kDotY + kDotHitR) {
                int dotStartX = (540 / 2) - ((4 - 1) * kDotSpacing) / 2;
                for (int d = 0; d < 4; d++) {
                    int dotX = dotStartX + d * kDotSpacing;
                    if (abs(tapX - dotX) <= kDotHitR) {
                        disp.setActivePage(static_cast<Page>(d));
                        rtcActivePage = disp.getActivePage();
                        activity = true;
                        break;
                    }
                }
            }
            // ── Settings icon tap ────────────────────────────────────────────
            else if (disp.getActivePage() == Page::Settings
                    && tapY >= kSettingsTapTop && tapY < kSettingsTapBot) {
                int col = tapX / kSettingsColW;
                if (col == 0) {
                    ESP_LOGI(TAG, "Force Sync Triggered via tap");
                    disp.showMessage("Syncing...", "Fetching fresh weather data");
                    rtcWakeupCount = 0;
                    rtcCachedWeather.valid = false;
                    _enterDeepSleepForImmediateWakeup();
                } else if (col == 1) {
                    ESP_LOGI(TAG, "Web Setup Triggered via tap");
                    disp.showMessage("Starting Setup", "Rebooting to portal...");
                    delay(1500);
                    ConfigManager::getInstance().setForceProvisioning(true);
                    esp_restart();
                } else if (col == 2) {
                    ESP_LOGI(TAG, "Sleep Triggered via tap");
                    disp.showMessage("Going to Sleep", "Press G38 to wake");
                    delay(1500);
                    enterDeepSleep();
                }
            }
            // ── Forecast scroll triangles tap ────────────────────────────────
            else if (disp.getActivePage() == Page::Forecast
                    && tapY >= kTriTop && tapY <= kTriBot) {
                if (tapX < 60 && rtcForecastOffset > 0) {
                    rtcForecastOffset--;
                    activity = true;
                } else if (tapX > 480 && rtcForecastOffset + kForecastColsPerView < rtcCachedWeather.forecastDays) {
                    rtcForecastOffset++;
                    activity = true;
                }
            }
        }

        if (clickCount > 0) {
            uint32_t now = millis();
            if (now - lastClickMs < 600) {
                consecutiveClicks++;
            } else {
                consecutiveClicks = 1;
            }
            lastClickMs = now;

            if (consecutiveClicks == 2 && !cfg.webhook_url.isEmpty()) {
                ESP_LOGI(TAG, "Double click detected! Firing webhook...");
                disp.showMessage("Webhook Sent", "Double tap detected");
                
                HTTPClient http;
                http.setTimeout(5000); // 5-second timeout; unresponsive server must not block indefinitely
                http.begin(cfg.webhook_url);
                int code = http.GET();
                if (code != HTTP_CODE_OK) {
                    ESP_LOGW(TAG, "Webhook returned HTTP %d", code);
                }
                http.end();
                
                delay(1000);
                disp.renderActivePage(rtcCachedWeather, localTime, locationStr, false, rtcForecastOffset, rtcSettingsCursor, overlayActive);
                consecutiveClicks = 0; // reset
            } else if (disp.getActivePage() == Page::Dashboard) {
                // Dashboard: click = force sync (same as long-press shortcut)
                ESP_LOGI(TAG, "G38 on Dashboard → force sync");
                disp.showMessage("Syncing...", "Fetching fresh weather data");
                rtcWakeupCount = 0;
                rtcCachedWeather.valid = false;
                _enterDeepSleepForImmediateWakeup();
            } else if (disp.getActivePage() == Page::Forecast) {
                // Forecast: click = jump back to today (offset 0)
                rtcForecastOffset = 0;
                activity = true;
            } else if (disp.getActivePage() == Page::Hourly) {
                // Hourly: click = cycle pages
                int nextPage = (static_cast<int>(disp.getActivePage()) + 1) % 4;
                disp.setActivePage(static_cast<Page>(nextPage));
                rtcActivePage = disp.getActivePage();
                activity = true;
            }
        }

        // Touch Swipes for Page Navigation
        if (input.checkSwipeLeft()) {
            int nextPage = (static_cast<int>(disp.getActivePage()) + 1) % 4;
            disp.setActivePage(static_cast<Page>(nextPage));
            rtcActivePage = disp.getActivePage();
            activity = true;
        }
        if (input.checkSwipeRight()) {
            int prevPage = (static_cast<int>(disp.getActivePage()) + 3) % 4;
            disp.setActivePage(static_cast<Page>(prevPage));
            rtcActivePage = disp.getActivePage();
            activity = true;
        }
        if (input.checkSwipeUp()) {
            if (!overlayActive) {
                overlayActive = true;
                overlayChanged = true;
                activity = true;
            }
        }
        if (input.checkSwipeDown()) {
            if (overlayActive) {
                overlayActive = false;
                overlayChanged = true;
                activity = true;
            }
        }

        if (activity) {
            lastActivityMs = millis();
            if (rtcCachedWeather.valid) {
                bool pageChanged = (disp.getActivePage() != lastPage);
                // If page changed or overlay changed, use high-quality refresh
                disp.renderActivePage(rtcCachedWeather, localTime, locationStr, !(pageChanged || overlayChanged), rtcForecastOffset, rtcSettingsCursor, overlayActive);
                lastPage = disp.getActivePage();
                overlayChanged = false;
            }
        }

        delay(10); // increased polling frequency (100Hz)
    }

    ESP_LOGI(TAG, "Interactive session timed out. Returning to sleep.");
    enterDeepSleep();
}

void AppController::enterDeepSleep() {
    ESP_LOGI(TAG, "Configuring Deep Sleep boundaries...");
    
    // Get user-configured sync interval
    WeatherConfig cfg = ConfigManager::getInstance().load();
    uint64_t syncIntevalM = cfg.sync_interval_m > 0 ? cfg.sync_interval_m : 30;

    // Battery-adaptive sync rate logic: double interval if battery is low (below 3.65V = ~40%)
    uint32_t batMv = analogReadMilliVolts(35) * 2;
    if (batMv > 0 && batMv < 3650) {
        ESP_LOGI(TAG, "Battery voltage low (%d mV), doubling sync interval to save power.", batMv);
        syncIntevalM *= 2;
    }

    uint64_t sleepUs = syncIntevalM * 60ULL * 1000000ULL;
    ESP_LOGI(TAG, "Sleep interval set to %llu minutes.", syncIntevalM);

    // Enable timer wakeup
    esp_sleep_enable_timer_wakeup(sleepUs);

    // Enable EXT0 physical button wakeup mapped to G38 (Push Button).
    // GPIO34-39 are input-only with no internal pull resistors, so
    // rtc_gpio_pullup_en() would silently fail. M5Paper PCB has an external
    // pull-up that keeps G38 HIGH when not pressed.
    _configureEXT0Wakeup();

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
    _configureEXT0Wakeup();

    delay(500);
    esp_deep_sleep_start();
}

#include "AppController.h"
#include <SystemState.h>
#include <ConfigManager.h>
#include <DisplayManager.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPManager.h>
#include <WeatherService.h>
#include <InputManager.h>
#include <ProvisioningManager.h>
#include <PageRouter.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
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
// All RTC-persistent state is consolidated in the single g_state struct
// (RTC_DATA_ATTR SystemState g_state) defined in src/main.cpp.
// See lib/Models/SystemState.h for the full field list.

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

    // ── Always-On Minimal mode takes its own fast path ────────────────────────
    if (cfg.display_mode == DisplayMode::MinimalAlwaysOn) {
        _runMinimalAlwaysOnMode();
        return;
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

        if (g_state.currentWeather.valid) {
            disp.setLastKnownIP(g_state.lastIP);
            disp.setActivePage(static_cast<Page>(g_state.activePage));
            disp.setLastError(g_state.lastError);
            disp.setLastSyncTime(g_state.currentWeather.fetchTime);
            disp.renderActivePage(g_state.currentWeather, localTime, locationStr, /*fastMode=*/true, g_state.forecastOffset, g_state.settingsCursor);
        } else {
            // No cached data yet — avoid showing the misleading "Fetching weather"
            // placeholder, which implies a fetch is in progress when it is not.
            disp.showMessage("No Data Yet", "Weather syncs every 30 min");
        }

        // Run the interactive loop for X minutes
        _runInteractiveSession(locationStr);

    } else {
        // ── 2. Woken by Timer OR Power-On ───────────
        if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            ESP_LOGI(TAG, "Cold boot (power-on) — showing full splash, fetching weather");
        } else {
            ESP_LOGI(TAG, "Warm boot (timer wakeup) — showing sync badge, fetching weather");
        }

        // ── Low-battery guard: skip WiFi radio and extend sleep to 2 hours ───
        // Use DisplayManager::sampleBattery() so the 4-sample averaged ADC read
        // is cached — _drawBattery() (called during rendering) reuses it rather
        // than issuing a second burst.
        int32_t batMv = disp.sampleBattery();
        if (batMv > 0 && batMv < kLowBatThresholdMv) {
            ESP_LOGW(TAG, "Low battery (%.2f V < %.2f V) — skipping fetch, extending sleep to 2h",
                     batMv / 1000.0f, kLowBatThresholdMv / 1000.0f);
            disp.showMessage("Low Battery",
                             String("Only ") + String(batMv / 1000.0f, 2) + " V — charge soon");
            // Extended sleep — skip WiFi radio entirely
            esp_sleep_enable_timer_wakeup(kLowBatSleepUs);
            _configureEXT0Wakeup();
            g_state.lastError = kErrLowBattery;
            delay(2000);
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            esp_wifi_stop();
            esp_deep_sleep_start();
        }
        
        if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            // Cold boot (power-on / first flash): full-screen splash clears any panel
            // ghosting and shows progress steps while the first weather payload is
            // fetched. RTC memory is zero-initialised on cold boot so loading always
            // starts from step 0.
            disp.showLoadingScreen(locationStr);
        } else {
            // Warm boot (timer wakeup): e-ink retains its last image across deep sleep —
            // never flash the screen before new data is ready. A small badge signals the
            // background sync without disturbing the existing display content.
            // updateLoadingStep() is a no-op while _loadingScreenActive is false, so
            // the progress bar steps below are cleanly skipped on this path.
            disp.showRefreshingBadge();
        }

        if (WiFiManager::getInstance().isConnected() || WiFiManager::getInstance().connectBestSTA(cfg.wifi_ssids, cfg.wifi_passes, cfg.wifi_count)) {
            // DTIM-aligned modem sleep: radio dozes between beacon intervals while
            // the CPU runs HTTP fetches, cutting average radio current ~20–30%.
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

            // Cache IP immediately after connect so it survives deep sleep
            strncpy(g_state.lastIP, WiFi.localIP().toString().c_str(), sizeof(g_state.lastIP) - 1);
            g_state.lastIP[sizeof(g_state.lastIP) - 1] = '\0';
            disp.updateLoadingStep(1); // WiFi connected — advance to NTP
            esp_task_wdt_reset();      // keep WDT alive: network phase progressing
            if (g_state.wakeupCount % 48 == 0) {
                ESP_LOGI(TAG, "Executing 24-hour NTP Sync (iteration %lu)", (unsigned long)g_state.wakeupCount);
                bool ntpOk = NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
                if (!ntpOk) g_state.lastError = kErrNtpFail;
            } else {
                ESP_LOGI(TAG, "Bypassing NTP Sync (Hardware RTC BM8563 active, iteration %lu)", (unsigned long)g_state.wakeupCount);
                setenv("TZ", cfg.timezone.c_str(), 1);
                tzset();
            }
            g_state.wakeupCount++;

            disp.updateLoadingStep(2); // NTP done — advance to weather fetch
            esp_task_wdt_reset();      // keep WDT alive: network phase progressing
            WeatherData data = WeatherService::getInstance().fetch(cfg.lat, cfg.lon, cfg.api_key);
            if (data.valid) {
                g_state.currentWeather = data; // persist to RTC
                g_state.lastError = NTPManager::getInstance().isNtpFailed() ? kErrNtpFail : kErrNone;
                disp.updateLoadingStep(3); // 100% bar — no-ops if no loading screen active
                esp_task_wdt_reset();      // keep WDT alive: fetch completed

                // ── Rolling pressure ring for barometric trend ────────────────
                // Shift ring left and insert newest reading; trend = newest - oldest.
                // Three readings at the default 30-min sync cadence ≈ 1-hour window.
                if (g_state.currentWeather.pressureHpa > 0.0f) {
                    g_state.pressureRing[0] = g_state.pressureRing[1];
                    g_state.pressureRing[1] = g_state.pressureRing[2];
                    g_state.pressureRing[2] = g_state.currentWeather.pressureHpa;
                    if (g_state.pressureCount < 3) g_state.pressureCount++;
                    if (g_state.pressureCount >= 3) {
                        g_state.currentWeather.pressureTrend = g_state.pressureRing[2] - g_state.pressureRing[0];
                    }
                }
                // ── Rolling battery ring for runtime estimation ──────────────────────
                // Store the current battery voltage in a fixed 8-slot ring buffer
                // that survives deep sleep.  Once ≥2 samples exist we can estimate
                // discharge rate and project remaining runtime.
                {
                    int32_t nowMv = (int32_t)(DisplayManager::getInstance().getBatVoltage() * 1000.0f);
                    if (nowMv > 1000) { // sanity: ignore 0V/invalid reads
                        g_state.batRing[g_state.batRingHead] = nowMv;
                        g_state.batRingHead = (g_state.batRingHead + 1) % 8;
                        if (g_state.batRingCount < 8) g_state.batRingCount++;
                    }
                    // Derive mV/cycle discharge rate from oldest and newest valid samples.
                    // syncInterval is already in minutes; multiply by sample-count gap.
                    if (g_state.batRingCount >= 2) {
                        int oldest = (g_state.batRingHead - g_state.batRingCount + 8) % 8;
                        int newest = (g_state.batRingHead - 1 + 8) % 8;
                        int32_t deltaMv = g_state.batRing[oldest] - g_state.batRing[newest]; // positive = discharging
                        if (deltaMv > 0) {
                            WeatherConfig _cfg = ConfigManager::getInstance().load();
                            uint64_t intervalM = _cfg.sync_interval_m > 0 ? _cfg.sync_interval_m : 30;
                            // minutes elapsed across (g_state.batRingCount-1) intervals
                            float elapsedMinutes = (float)(g_state.batRingCount - 1) * (float)intervalM;
                            float mvPerMinute    = (float)deltaMv / elapsedMinutes;
                            // remaining mV above 3200 (0%) floor
                            int32_t remainingMv  = g_state.batRing[newest] - 3200;
                            if (remainingMv > 0 && mvPerMinute > 0.0f) {
                                int remainMinutes = (int)(remainingMv / mvPerMinute);
                                int remainHours   = remainMinutes / 60;
                                ESP_LOGI(TAG, "Battery runtime estimate: ~%d h remaining (%.2f mV/min)",
                                         remainHours, mvPerMinute);
                                // Store in RTC and push to display state immediately
                                g_state.currentWeather.batteryRuntimeH = remainHours;
                                disp.setLastBattRuntime(remainHours);
                            }
                        }
                    }
                }            } else {
                g_state.lastError = kErrWeatherFail;
            }
        } else {
            g_state.lastError = kErrWiFiFail;
        }

        // Propagate NTP failure status to display so badge appears on Dashboard
        disp.setNtpFailed(NTPManager::getInstance().isNtpFailed());
        disp.setLastError(g_state.lastError);

        if (g_state.currentWeather.valid) {
            struct tm localTime = {};
            NTPManager::getInstance().getLocalTime(localTime);
            // Ghost cleanup cycle every 48 full-quality redraws to prevent e-ink artifact buildup.
            // Raised from 20 → 48 so the 1.3-second W→B→W flash doesn’t disrupt interactive scrolling.
            constexpr uint8_t kGhostCleanupInterval = 48;
            g_state.ghostCount++;
            if (g_state.ghostCount >= kGhostCleanupInterval) {
                g_state.ghostCount = 0;
                ESP_LOGI(TAG, "Ghost cleanup threshold reached — running cleanup before redraw");
                disp.ghostingCleanup();
            }
            // Full quality screen redraw
            disp.setLastKnownIP(g_state.lastIP);
            disp.setActivePage(static_cast<Page>(g_state.activePage));
            disp.setLastSyncTime(g_state.currentWeather.fetchTime);
            disp.renderActivePage(g_state.currentWeather, localTime, locationStr, /*fastMode=*/false, g_state.forecastOffset, g_state.settingsCursor);

            // If this cycle's fetch failed, overlay a stale-data badge so the user
            // knows the displayed data was not refreshed. The badge uses epd_fastest
            // and only updates Y 910–960, leaving the weather content untouched.
            if (g_state.lastError == kErrWeatherFail || g_state.lastError == kErrWiFiFail) {
                ESP_LOGW(TAG, "Fetch failed — rendering stale-cache badge over cached data");
                disp.showStaleBadge(g_state.currentWeather.fetchTime);
            }
        } else {
            disp.showMessage("Network Error", "Unable to fetch data");
        }
        
        // Immediately go back to sleep
        enterDeepSleep();
    }
}

void AppController::_runInteractiveSession(const String& locationStr) {
    // Reduce CPU to 80 MHz for the duration of the interactive session.
    // Touch/button polling and I²C RTC reads need far less than 240 MHz;
    // 80 MHz is the minimum that keeps WiFi functional (on-demand webhook path).
    // Deep sleep after the session resets the frequency automatically.
    setCpuFrequencyMhz(80);

    auto& disp  = DisplayManager::getInstance();
    auto  cfg   = ConfigManager::getInstance().load();
    uint32_t lastActivityMs = millis();
    int lastMinute = -1;
    auto& input = InputManager::getInstance();
    bool overlayActive = false;
    bool overlayChanged = false;

    int consecutiveClicks = 0;
    uint32_t lastClickMs = 0;

    // Build a PageState snapshot from g_state for the page/router layer.
    auto buildState = [&]() -> PageState {
        struct tm lt = {};
        NTPManager::getInstance().getLocalTime(lt);
        return { g_state.currentWeather, lt, locationStr, g_state.forecastOffset,
                 g_state.settingsCursor, NTPManager::getInstance().isNtpFailed(),
                 overlayActive };
    };

    // Attach the router to the page already shown on screen (no redraw).
    PageRouter router;
    router.restore(g_state.activePage, buildState());

    while ((millis() - lastActivityMs) < kInteractiveTimeoutMs) {
        bool activity = false;
        esp_task_wdt_reset(); // keep WDT alive: interactive session is running normally

        // Process touch and buttons on the main loop (same I2C context as RTC reads).
        input.pollInput();

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);
        if (lastMinute == -1) lastMinute = localTime.tm_min;

        // Force UI redraw if clock minute rolls over natively
        if (localTime.tm_min != lastMinute) {
            lastMinute = localTime.tm_min;
            // On the Dashboard, use a tight partial refresh for the clock strip only.
            // On other pages a full render doesn't tick, so just set activity to
            // update weather data-driven fields when the user navigates.
            if (disp.getActivePage() == Page::Dashboard && g_state.currentWeather.valid) {
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
                if (disp.getActivePage() == Page::Forecast && g_state.forecastOffset < g_state.currentWeather.forecastDays - kForecastColsPerView) {
                    g_state.forecastOffset++;
                    activity = true;
                } else if (disp.getActivePage() == Page::Settings && g_state.settingsCursor < 2) {
                    g_state.settingsCursor++;
                    activity = true;
                }
            }
        }
        
        if (downEvents > 0) {
            for (int i = 0; i < downEvents; i++) {
                if (disp.getActivePage() == Page::Forecast && g_state.forecastOffset > 0) {
                    g_state.forecastOffset--;
                    activity = true;
                } else if (disp.getActivePage() == Page::Settings && g_state.settingsCursor > 0) {
                    g_state.settingsCursor--;
                    activity = true;
                }
            }
        }

        // G38 long-press (2–3 s) = force sync from any page
        if (input.checkLongPress()) {
            ESP_LOGI(TAG, "G38 long-press → force sync");
            disp.showMessage("Syncing...", "Fetching fresh weather data");
            g_state.currentWeather.valid = false;
            _enterDeepSleepForImmediateWakeup();
        }

        // Pagination dots at Y=940, spacing=24, centred on kWidth/2 over 4 pages
        constexpr int kDotY       = 940;
        constexpr int kDotSpacing = 24;
        constexpr int kDotHitR    = 24; // half the extended hit-zone width

        int tapX = 0, tapY = 0;
        if (input.checkTap(tapX, tapY)) {
            // Route through PageRouter first for page-specific handling.
            if (router.handleTouch(tapX, tapY)) {
                PageAction action = router.consumePendingAction();
                lastActivityMs = millis();
                switch (action) {
                    case PageAction::ForceSync:
                        ESP_LOGI(TAG, "Force Sync Triggered via tap");
                        disp.showMessage("Syncing...", "Fetching fresh weather data");
                        g_state.currentWeather.valid = false;
                        _enterDeepSleepForImmediateWakeup();
                        break;
                    case PageAction::StartProvisioning:
                        ESP_LOGI(TAG, "Web Setup Triggered via tap");
                        disp.showMessage("Starting Setup", "Rebooting to portal...");
                        delay(1500);
                        ConfigManager::getInstance().setForceProvisioning(true);
                        esp_restart();
                        break;
                    case PageAction::EnterSleep:
                        ESP_LOGI(TAG, "Sleep Triggered via tap");
                        disp.showMessage("Going to Sleep", "Press G38 to wake");
                        delay(1500);
                        enterDeepSleep();
                        break;
                    case PageAction::IncrementForecastOffset:
                        if (g_state.forecastOffset + kForecastColsPerView < g_state.currentWeather.forecastDays) {
                            g_state.forecastOffset++;
                            router.updateData(buildState());
                            router.render();
                        }
                        break;
                    case PageAction::DecrementForecastOffset:
                        if (g_state.forecastOffset > 0) {
                            g_state.forecastOffset--;
                            router.updateData(buildState());
                            router.render();
                        }
                        break;
                    default: break;
                }
            } else {
                // Pagination dot tap — works on every page.
                if (tapY >= kDotY - kDotHitR && tapY <= kDotY + kDotHitR) {
                    int dotStartX = (540 / 2) - ((4 - 1) * kDotSpacing) / 2;
                    for (int d = 0; d < 4; d++) {
                        int dotX = dotStartX + d * kDotSpacing;
                        if (abs(tapX - dotX) <= kDotHitR) {
                            router.navigateTo(d, buildState());
                            g_state.activePage = router.getActivePageId();
                            disp.setActivePage(static_cast<Page>(g_state.activePage));
                            lastActivityMs = millis();
                            break;
                        }
                    }
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

                bool wifiReady = WiFiManager::getInstance().isConnected();
                if (!wifiReady) {
                    disp.showMessage("Connecting...", "Connecting to WiFi for webhook");
                    wifiReady = WiFiManager::getInstance().connectBestSTA(
                        cfg.wifi_ssids, cfg.wifi_passes, cfg.wifi_count);
                    if (wifiReady) {
                        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
                    }
                }

                if (!wifiReady) {
                    ESP_LOGW(TAG, "Webhook aborted — WiFi unavailable");
                    disp.showMessage("No Connection", "Could not connect to WiFi");
                } else {
                    HTTPClient http;
                    http.setTimeout(5000); // 5-second timeout; unresponsive server must not block indefinitely
                    http.begin(cfg.webhook_url);
                    int code = http.GET();
                    http.end();

                    if (code == HTTP_CODE_OK) {
                        ESP_LOGI(TAG, "Webhook fired successfully (HTTP %d)", code);
                        disp.showMessage("Webhook Sent", "Request delivered successfully");
                    } else {
                        ESP_LOGW(TAG, "Webhook returned HTTP %d", code);
                        disp.showMessage("Webhook Failed", String("Server responded: ") + String(code));
                    }
                }

                delay(1000);
                disp.renderActivePage(g_state.currentWeather, localTime, locationStr, false, g_state.forecastOffset, g_state.settingsCursor, overlayActive);
                consecutiveClicks = 0; // reset
                lastActivityMs = millis();
            } else if (disp.getActivePage() == Page::Dashboard) {
                // Dashboard: click = force sync (same as long-press shortcut)
                ESP_LOGI(TAG, "G38 on Dashboard → force sync");
                disp.showMessage("Syncing...", "Fetching fresh weather data");
                g_state.currentWeather.valid = false;
                _enterDeepSleepForImmediateWakeup();
            } else if (disp.getActivePage() == Page::Forecast) {
                // Forecast: click = jump back to today (offset 0)
                g_state.forecastOffset = 0;
                activity = true;
            } else if (disp.getActivePage() == Page::Hourly) {
                // Hourly: click = cycle pages
                router.navigateNext(buildState());
                g_state.activePage = router.getActivePageId();
                disp.setActivePage(static_cast<Page>(g_state.activePage));
                lastActivityMs = millis();
            }
        }

        // Touch Swipes for Page Navigation
        if (input.checkSwipeLeft()) {
            router.navigateNext(buildState());
            g_state.activePage = router.getActivePageId();
            disp.setActivePage(static_cast<Page>(g_state.activePage));
            lastActivityMs = millis();
        }
        if (input.checkSwipeRight()) {
            router.navigatePrev(buildState());
            g_state.activePage = router.getActivePageId();
            disp.setActivePage(static_cast<Page>(g_state.activePage));
            lastActivityMs = millis();
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
            if (g_state.currentWeather.valid) {
                // Scroll-wheel and overlay changes re-render via DisplayManager
                // (overlay is handled there; page navigation renders were already
                // done by router.navigateTo()).
                disp.renderActivePage(g_state.currentWeather, localTime, locationStr,
                                      !overlayChanged, g_state.forecastOffset,
                                      g_state.settingsCursor, overlayActive);
                overlayChanged = false;
            }
        }

        delay(50); // 20 Hz poll rate — imperceptible for touch latency, saves ~65% dynamic CPU power
    }

    ESP_LOGI(TAG, "Interactive session timed out. Returning to sleep.");
    enterDeepSleep();
}

void AppController::enterDeepSleep() {
    ESP_LOGI(TAG, "Configuring Deep Sleep boundaries...");
    
    // Get user-configured sync interval
    WeatherConfig cfg = ConfigManager::getInstance().load();
    uint64_t syncIntevalM = cfg.sync_interval_m > 0 ? cfg.sync_interval_m : 30;

    // Battery-adaptive sync rate logic: double interval if battery is below ~40% (3.65 V).
    // Re-use DisplayManager's cached voltage — the ADC was already sampled this wakeup
    // cycle by sampleBattery() in the low-battery guard above (timer path) or by
    // _drawBattery() during rendering (button path).
    float batV = DisplayManager::getInstance().getBatVoltage();
    uint32_t batMv = (batV > 0.1f) ? (uint32_t)(batV * 1000.0f)
                                    : (uint32_t)(analogReadMilliVolts(35) * 2); // fallback
    if (batMv > 0 && batMv < 3650) {
        ESP_LOGI(TAG, "Battery voltage low (%u mV), doubling sync interval to save power.", batMv);
        syncIntevalM *= 2;
    }

    // Adaptive sleep: shorten to 10 min when precipitation is rapidly increasing.
    // If any of the next 3 hourly entries has a precipChance > 20% higher than the
    // current hour, the device will sync more often to catch fast-developing rain.
    if (g_state.currentWeather.valid && g_state.currentWeather.hourlyCount >= 2) {
        int basePrecip = g_state.currentWeather.hourly[0].precipChance;
        for (int i = 1; i < 3 && i < g_state.currentWeather.hourlyCount; i++) {
            if (g_state.currentWeather.hourly[i].precipChance - basePrecip > 20) {
                ESP_LOGI(TAG, "Rapid precip increase (+%d%% in %dh) — shortening sync to 10 min",
                         g_state.currentWeather.hourly[i].precipChance - basePrecip, i);
                syncIntevalM = std::min((uint64_t)syncIntevalM, (uint64_t)10);
                break;
            }
        }
    }

    uint64_t sleepUs = syncIntevalM * 60ULL * 1000000ULL;
    ESP_LOGI(TAG, "Sleep interval set to %llu minutes.", syncIntevalM);

    // Night mode: if the current hour falls within the configured overnight window,
    // skip the normal short sync interval and sleep straight through until morning.
    // Setting start == end disables night mode entirely.
    {
        struct tm now = {};
        NTPManager::getInstance().getLocalTime(now);
        int hour    = now.tm_hour;
        int nmStart = cfg.night_mode_start;
        int nmEnd   = cfg.night_mode_end;
        bool isNight = (nmStart != nmEnd) && (hour >= nmStart || hour < nmEnd);
        if (isNight) {
            int minsUntilMorning = (hour >= nmStart)
                ? (24 - hour + nmEnd) * 60 - now.tm_min
                : (nmEnd  - hour)     * 60 - now.tm_min;
            if (minsUntilMorning > 0) {
                sleepUs = (uint64_t)minsUntilMorning * 60ULL * 1000000ULL;
                ESP_LOGI(TAG, "Night mode: sleeping %d min until %02d:00", minsUntilMorning, nmEnd);
            }
        }
    }

    // Enable timer wakeup
    esp_sleep_enable_timer_wakeup(sleepUs);

    // Enable EXT0 physical button wakeup mapped to G38 (Push Button).
    // GPIO34-39 are input-only with no internal pull resistors, so
    // rtc_gpio_pullup_en() would silently fail. M5Paper PCB has an external
    // pull-up that keeps G38 HIGH when not pressed.
    _configureEXT0Wakeup();

    ESP_LOGI(TAG, "Entering Deep Sleep now! -> Zzz");
    
    // Shut down gracefully
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
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
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_deep_sleep_start();
}

// ─────────────────────────────────────────────────────────────────────────────
// Always-On Minimal Mode
// ─────────────────────────────────────────────────────────────────────────────

void AppController::_runMinimalAlwaysOnMode() {
    auto wakeup_reason = esp_sleep_get_wakeup_cause();
    auto cfg           = ConfigManager::getInstance().load();
    auto& disp         = DisplayManager::getInstance();

    String locationStr = cfg.city;
    if (cfg.state.length() > 0) locationStr += ", " + cfg.state;

    // Ensure local timezone is applied for RTC reads on all paths.
    setenv("TZ", cfg.timezone.c_str(), 1);
    tzset();

    uint8_t syncInterval = (cfg.always_on_sync_interval > 0)
                           ? cfg.always_on_sync_interval : 30;

    // ── Button (EXT0) wakeup: fall back to full interactive session ──────────
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "[MinimalAlwaysOn] EXT0 wakeup — entering interactive session");

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);

        if (g_state.currentWeather.valid) {
            disp.setLastKnownIP(g_state.lastIP);
            disp.setLastSyncTime(g_state.currentWeather.fetchTime);
            disp.setLastError(g_state.lastError);
            disp.drawMinimalMode(g_state.currentWeather, localTime, locationStr);
        } else {
            disp.showMessage("No Data Yet", "Weather syncs every few minutes");
        }

        _runInteractiveSession(locationStr);
        return;
    }

    // ── Timer / cold-boot wakeup ─────────────────────────────────────────────
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        g_state.minutesSinceSync++;
        ESP_LOGI(TAG, "[MinimalAlwaysOn] Timer wakeup — minutes since sync: %u / %u",
                 g_state.minutesSinceSync, syncInterval);
    } else {
        // Cold boot — force an immediate sync by setting minutesSinceSync to
        // the sync interval threshold.  (Zero-initialised g_state would otherwise
        // prevent a sync until 30 timer ticks had elapsed.)
        g_state.minutesSinceSync = syncInterval;
        ESP_LOGI(TAG, "[MinimalAlwaysOn] Cold boot — forcing immediate weather sync");
    }

    // ── Low-battery guard: extend sleep to 2 hours, skip WiFi ────────────────
    int32_t batMv = disp.sampleBattery();
    if (batMv > 0 && batMv < kLowBatThresholdMv) {
        ESP_LOGW(TAG, "[MinimalAlwaysOn] Low battery (%.2f V) — skipping sync, sleeping 2 h",
                 batMv / 1000.0f);
        disp.showMessage("Low Battery",
                         String("Only ") + String(batMv / 1000.0f, 2) + " V — charge soon");
        esp_sleep_enable_timer_wakeup(kLowBatSleepUs);
        _configureEXT0Wakeup();
        g_state.lastError = kErrLowBattery;
        delay(2000);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        esp_deep_sleep_start();
    }

    if (g_state.minutesSinceSync >= syncInterval) {
        // ═══════════════════════════════════════════════════════════════════════
        // FULL SYNC CYCLE — WiFi + NTP + weather fetch + full epd_quality render
        // ═══════════════════════════════════════════════════════════════════════
        ESP_LOGI(TAG, "[MinimalAlwaysOn] Full sync cycle starting");

        disp.showRefreshingBadge();

        if (WiFiManager::getInstance().isConnected() ||
            WiFiManager::getInstance().connectBestSTA(cfg.wifi_ssids, cfg.wifi_passes, cfg.wifi_count)) {

            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
            strncpy(g_state.lastIP, WiFi.localIP().toString().c_str(), sizeof(g_state.lastIP) - 1);
            g_state.lastIP[sizeof(g_state.lastIP) - 1] = '\0';
            esp_task_wdt_reset();

            // NTP: full sync every 48 weather-sync cycles (same cadence as standard mode)
            if (g_state.wakeupCount % 48 == 0) {
                bool ntpOk = NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
                if (!ntpOk) g_state.lastError = kErrNtpFail;
            } else {
                setenv("TZ", cfg.timezone.c_str(), 1);
                tzset();
            }
            g_state.wakeupCount++;
            esp_task_wdt_reset();

            WeatherData data = WeatherService::getInstance().fetch(cfg.lat, cfg.lon, cfg.api_key);
            if (data.valid) {
                g_state.currentWeather = data;
                g_state.lastError = NTPManager::getInstance().isNtpFailed() ? kErrNtpFail : kErrNone;
                esp_task_wdt_reset();

                // Rolling pressure ring (same logic as standard mode)
                if (g_state.currentWeather.pressureHpa > 0.0f) {
                    g_state.pressureRing[0] = g_state.pressureRing[1];
                    g_state.pressureRing[1] = g_state.pressureRing[2];
                    g_state.pressureRing[2] = g_state.currentWeather.pressureHpa;
                    if (g_state.pressureCount < 3) g_state.pressureCount++;
                    if (g_state.pressureCount >= 3)
                        g_state.currentWeather.pressureTrend = g_state.pressureRing[2] - g_state.pressureRing[0];
                }
            } else {
                g_state.lastError = kErrWeatherFail;
            }
        } else {
            g_state.lastError = kErrWiFiFail;
        }

        disp.setNtpFailed(NTPManager::getInstance().isNtpFailed());
        disp.setLastError(g_state.lastError);

        // Ghost cleanup: full W→B→W cycle before every sync render to prevent
        // artifact build-up from the 29 fast-refreshes that preceded this draw.
        disp.ghostingCleanup();
        g_state.ghostCount = 0;

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);

        if (g_state.currentWeather.valid) {
            disp.setLastKnownIP(g_state.lastIP);
            disp.setLastSyncTime(g_state.currentWeather.fetchTime);
            disp.drawMinimalMode(g_state.currentWeather, localTime, locationStr);
            if (g_state.lastError == kErrWeatherFail || g_state.lastError == kErrWiFiFail) {
                disp.showStaleBadge(g_state.currentWeather.fetchTime);
            }
        } else {
            disp.showMessage("Network Error", "Unable to fetch data");
        }

        g_state.minutesSinceSync = 0;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();

    } else {
        // ═══════════════════════════════════════════════════════════════════════
        // 1-MINUTE TICK CYCLE — clock-only partial refresh, no WiFi
        // ═══════════════════════════════════════════════════════════════════════
        ESP_LOGI(TAG, "[MinimalAlwaysOn] Clock-tick cycle (minute %u)", g_state.minutesSinceSync);

        struct tm localTime = {};
        NTPManager::getInstance().getLocalTime(localTime);

        disp.updateMinimalClock(localTime, NTPManager::getInstance().isNtpFailed());
    }

    _enterMinimalDeepSleep();
}

void AppController::_enterMinimalDeepSleep() {
    // Sleep until the top of the next minute so wakeups land close to HH:MM:00.
    struct tm now = {};
    NTPManager::getInstance().getLocalTime(now);
    int secsUntilNextMin = 60 - now.tm_sec;
    if (secsUntilNextMin <= 0 || secsUntilNextMin > 60) secsUntilNextMin = 60;

    uint64_t sleepUs = (uint64_t)secsUntilNextMin * 1000000ULL;
    ESP_LOGI(TAG, "[MinimalAlwaysOn] Sleeping %d s until next minute boundary", secsUntilNextMin);

    esp_sleep_enable_timer_wakeup(sleepUs);
    _configureEXT0Wakeup();

    delay(200);
    esp_deep_sleep_start();
}

# System Architecture

The M5Paper Weather App is built on top of the ESP32 framework (`framework-arduinoespressif32`) using a strict Object-Oriented, task-driven architecture. The application decouples hardware interfacing, network operations, UI rendering, and state management into distinct Singleton manager classes.

## 🏗️ Core Components

### 1. `AppController` (The Orchestrator)

The brain of the application. It acts as a synchronous, event-driven state machine orchestrating network fetches, UI rendering, and ultra-low power states.

- **Deep Sleep Lifecycles**: Drifts the ESP32 into hardware hibernation between configurable sync windows (default 30 minutes, user-adjustable via the provisioning portal). The interval is read from `ConfigManager` on every `enterDeepSleep()` call and automatically doubled when battery voltage drops below 3650 mV (~40%).
- **RTC Fast-Wake**: Stores the entire `WeatherData` struct in `RTC_DATA_ATTR` fixed-size buffers (not heap Strings), allowing the device to wake via hardware button press, render instantly from cache, and return to sleep.
- **Wakeup Dispatch**: `esp_sleep_get_wakeup_cause()` branches into two paths:
  - **EXT0 (G38 button)**: enters a 10-minute interactive session, rendering from RTC cache with no network round-trip. If no data is cached yet, displays a "No Data Yet" message.
  - **Timer / Power-On**: connects WiFi, optionally syncs NTP, fetches weather, renders a full-quality page, then immediately calls `enterDeepSleep()`.
- **Force Sync path**: `_enterDeepSleepForImmediateWakeup()` uses a 1-second timer so the device wakes almost immediately into the normal timer-fetch path, avoiding a full sync-interval wait.
- **EXT0 pin initialisation**: extracted into a `static void _configureEXT0Wakeup()` file-local helper called from all three sleep-entry paths (`enterDeepSleep()`, `_enterDeepSleepForImmediateWakeup()`, and the low-battery early-sleep branch). `rtc_gpio_init()` and `rtc_gpio_set_direction()` must be called on `GPIO_NUM_38` before `esp_sleep_enable_ext0_wakeup()`. GPIO34–39 are input-only with no internal pull resistors, so `rtc_gpio_pullup_en()` must not be called on these pins.
- **Ghost cleanup**: `rtcGhostCount` tracks full-quality redraws; after every 48 a W→B→W `ghostingCleanup()` cycle fires before the next render to discharge accumulated e-ink ghost charge.
- **Forecast scroll bounds**: the magic number of columns per view is named `kForecastColsPerView = 3`; scroll-bound checks use this constant rather than a bare `3`.

**RTC-persisted state:**

| Variable | Type | Purpose |
|---|---|---|
| `rtcCachedWeather` | `WeatherData` | Full weather + 10-day forecast |
| `rtcForecastOffset` | `int` | Current forecast scroll position |
| `rtcWakeupCount` | `uint32_t` | Tracks 24-hour NTP sync interval (every 48 wakeups) |
| `rtcActivePage` | `Page` | Active display page across sleeps |
| `rtcSettingsCursor` | `int` | Settings icon focus across sleeps |
| `rtcLastIP` | `char[16]` | Last WiFi-assigned IP for diagnostics display |
| `rtcLastError` | `uint8_t` | `AppError` code from the most recent fetch cycle |
| `rtcGhostCount` | `uint8_t` | Full-quality redraw counter; triggers ghost cleanup at 48 |
| `rtcBatRing[8]` | `int32_t[8]` | Rolling battery voltage samples (mV) across wakeup cycles |
| `rtcBatRingHead` | `uint8_t` | Next write index for `rtcBatRing` (ring buffer head) |
| `rtcBatRingCount` | `uint8_t` | Number of valid samples in `rtcBatRing` (clamped to 8) |

### 2. Network Layer

Responsible for all outbound and inbound connectivity.

- **`WiFiManager`**: Toggles the ESP32 between Station (STA) mode for Internet access and SoftAP mode during initial setup. `connectBestSTA()` performs an active WiFi scan, ranks every SSID in the `WeatherConfig` list by RSSI, and connects to the strongest available network. A fast-connect cache (`rtc_cached_ssid` + `rtc_bssid` + `rtc_channel`) in `RTC_DATA_ATTR` is tried first when the cached SSID is still among the candidates, skipping a second channel scan; all three fields are zeroed on connection timeout to prevent a stale entry from blocking the next boot. Falls back to config-order sequential attempts when scanning fails or returns no matches.
- **`NTPManager`**: Uses `configTzTime` / POSIX locale via `setenv("TZ", ...)` + `tzset()`. Explicitly wipes stale RTC data on cold boots. Full NTP sync runs only every 48 timer-wakeup cycles (~24 hours); the BM8563 hardware RTC keeps time between syncs.
- **`WeatherService`**: TLS HTTPS client issuing up to five sequential fetches per cycle with a 10-second per-request timeout (`http.setTimeout(10000)`): Google Weather `currentConditions:lookup`, Google Weather `forecast/days:lookup` (10-day, `pageSize=10`), Open-Meteo AQI (`&hourly=grass_pollen,birch_pollen,ragweed_pollen` — note: `ragweed_pollen`, not `weed_pollen`), Open-Meteo sun+hourly, and Google Weather `weatherAlerts:lookup`. `http.setReuse()` is not used; each `http.begin()` on a different host is preceded by `client.stop()` to ensure a clean TLS socket state. A `currentOk` flag tracks whether the primary conditions call succeeded; supplemental fetches always proceed regardless, so cached auxiliary data stays fresh during a partial outage. `data.valid` is set only when `currentOk` is true. Results are flattened into `WeatherData` / `DailyForecast` structs that are safe to copy into RTC memory.

### 3. Hardware & Display Layer

- **`DisplayManager`**: Built on M5GFX. Manages a full-screen `M5Canvas` sprite; all drawing targets the sprite, which is then pushed to the display in one call. Never issues a global screen clear on partial updates.
  - `epd_quality` — full-page weather redraws only.
  - `epd_fastest` — clock tick, loading step advances, forecast scroll.
  - **Pages**: `Dashboard` (today), `Hourly` (24-hour strip), `Forecast` (10-day), `Settings`.
  - **Today page**: time/date header; hero icon + temperature; 3-row details grid (feels like, wind+direction, humidity, cloud cover, UV, visibility); moon phase glyph; wind rose compass; AQI gauge + Sun Arc dials with flanking sunrise/sunset times; tomorrow preview card.
  - **Hourly page**: `showHourlyPage()` renders a 24-column strip from `WeatherData::hourly[]` — time (12-hour), vector icon, temperature, precipitation chance, and wind speed per card with grid cell borders.
  - **Swipe-up detail overlay**: rendered as a full-screen layer over any page — shows AQI with EPA category label, active weather alert headline + severity (or reassurance text), and estimated dew point.
  - **Forecast page**: temperature band sparkline (dual Hi/Lo lines); precipitation bar chart; 3-column scrollable cards with weekday labels, icons, H/L text, temperature range bar, and rain chance.
  - **Settings page**: 3-column vector icon grid (Sync / Setup / Sleep); diagnostics row (battery voltage/%, IP, firmware version, last sync time).
  - **Loading screen**: cloud+sun vector icon (fill→hollow technique), 3-step animated progress bar. `updateLoadingStep(step)` advances via fast partial refresh without redrawing static elements.
  - **Battery gauge**: `sampleBattery()` averages 4 `analogReadMilliVolts(35) * 2` readings to reduce ADC noise. `_lipoMvToPercent()` maps voltage to state-of-charge via a piecewise-linear LiPo discharge curve. When charging is detected (`packMv > 4050`), the icon renders a bolt symbol and the percentage is prefixed with `+`. At ≤ 15% and not charging, the fill bar uses a dashed alternating-column pattern and a `LOW` badge appears in the clock strip. M5Unified power abstractions are broken on this hardware revision and must not be used.
  - **Battery runtime estimate**: `_lastBattRuntimeH` is populated each render cycle from `WeatherData::batteryRuntimeH` (computed by `AppController` from `rtcBatRing[]`). The Settings page shows `"Est. Runtime: ~N h left"` when the estimate is non-zero.
  - **IP diagnostics**: `setLastKnownIP()` accepts a cached IP from AppController; shows live, offline-cached, or "No data yet" state.
  - **PIN entry**: `promptPIN()` feeds `esp_task_wdt_reset()` on every loop iteration and returns an empty string after 2 minutes of inactivity, preventing both a WDT reset and an indefinitely locked display.

- **`InputManager`**: Polls G38/G37/G39 and the GT911 capacitive touch panel in a dedicated FreeRTOS task on Core 0. `_processTouchGestures()` calls `M5.update()` exclusively — `loop()` must not call it, as `M5.update()` is not thread-safe and the `wasPressed()`/`wasReleased()` edge flags are one-shot.
  - **Swipe gestures**: delta-X tracked in real time; swipe fires mid-drag at 30 px threshold.
  - **Swipe-up gesture**: delta-Y tracked separately; an upward swipe opens the detail overlay.
  - **Tap detection**: `checkTap(x, y)` returns a pending tap if the touch was released without crossing the 30 px swipe threshold. Consumed on read.
  - **G38 double-click**: `consecutiveClicks == 2` in the interactive session fires the configured webhook URL via `HTTPClient` (5-second timeout).
  - **G38 long-press** (≥10 s): triggers re-provisioning flag.
  - **G38 short-press**: increments `_click` counter (used for page cycling in non-Settings views).

### 4. Storage & Provisioning

- **`ConfigManager`**: Interfaces with `Preferences.h` to read/write `WeatherConfig` to the NVS partition (`"wcfg"` namespace). Survives cold boots and battery deaths. Stores up to 5 WiFi SSID/password pairs via indexed keys (`w_ssid_N` / `w_pass_N` + `w_count`); retains legacy `wifi_ssid` / `wifi_pass` keys for rollback compatibility. Critical `putInt` / `putString` calls check the return value and emit `ESP_LOGE` when zero bytes are written, making NVS corruption or flash exhaustion visible in the log.
  - **AES-256-CTR NVS encryption**: Sensitive fields (`wifi_passes[]`, `api_key`, `webhook_url`) are encrypted before being written to NVS. The 32-byte AES key is derived per-device as `SHA-256(eFuse MAC ∥ "M5PaperWCfgKey-v1")` — hardware-bound, never stored. Each `save()` call generates a fresh 16-byte IV from the ESP32 hardware TRNG; the stored format is `"E1:" + Base64(IV[16] ∥ ciphertext)`. `load()` detects the `"E1:"` prefix and decrypts transparently; values without the prefix are treated as legacy plaintext and returned as-is. On first boot after a firmware upgrade, `begin()` checks the `cfg_encv` NVS flag; if false and the device is already provisioned, a one-time migration re-encrypts all existing plaintext credentials in-place and sets `cfg_encv = true`.
- **`ProvisioningManager` & `WebServer`**: On first boot or G38 hold-at-boot, spins up an `ESPAsyncWebServer` serving a mobile-optimised HTML captive portal from flash. On form submission, saves to NVS and reboots into normal mode.
  - **PIN re-provisioning guard**: when a PIN hash is stored in NVS, POST `/save` requires the existing PIN as the `current_pin` parameter. Three consecutive wrong submissions increment `_failedAttempts` and trigger a 60-second lockout (`_lockoutUntilMs`), returning HTTP 429 for the duration. Both counters are instance members on `WebServer`.

## 🔄 Execution Flow

1. **Boot**: `main.cpp` initialises M5, `DisplayManager`, `ConfigManager`, `InputManager`.
2. **Provisioning check**: if NVS is empty, G38 held at boot, or force-provisioning flag is set → SoftAP + `ProvisioningManager`. If a PIN hash is stored, the e-ink PIN keypad is shown; 3 consecutive wrong entries trigger `esp_restart()`.
3. **Normal mode** — `setup()` returns after spawning `_appTask` via `xTaskCreatePinnedToCore()` (24 KB stack, core 1, priority 5). `loopTask` stays idle (only pumps `ProvisioningManager::run()` in provisioning mode). `_appTask` calls `AppController::begin()`:
   - **EXT0 wakeup**: render from RTC cache (or show "No Data Yet") → interactive session → `enterDeepSleep()`.
   - **Timer / Power-On wakeup**: show loading screen (if no cache) → connect WiFi → optionally sync NTP → fetch weather → render → `enterDeepSleep()`.
4. **Deep sleep**: configurable timer (default 30 min, doubled on low battery) + EXT0 on `GPIO_NUM_38` (LOW = wakeup). `_configureEXT0Wakeup()` is called to move the pin into the RTC IO domain.

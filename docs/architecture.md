# System Architecture

The M5Paper Weather App is built on top of the ESP32 framework (`framework-arduinoespressif32`) using a strict Object-Oriented, task-driven architecture. The application decouples hardware interfacing, network operations, UI rendering, and state management into distinct Singleton manager classes.

## 🏗️ Core Components

### 1. `AppController` (The Orchestrator)

The brain of the application. It acts as a synchronous, event-driven state machine orchestrating network fetches, UI rendering, and ultra-low power states.

- **Deep Sleep Lifecycles**: Drifts the ESP32 into hardware hibernation between 30-minute sync windows to maximise battery longevity.
- **RTC Fast-Wake**: Stores the entire `WeatherData` struct in `RTC_DATA_ATTR` fixed-size buffers (not heap Strings), allowing the device to wake via hardware button press, render instantly from cache, and return to sleep.
- **Wakeup Dispatch**: `esp_sleep_get_wakeup_cause()` branches into two paths:
  - **EXT0 (G38 button)**: enters a 10-minute interactive session, rendering from RTC cache with no network round-trip. If no data is cached yet, displays a "No Data Yet" message.
  - **Timer / Power-On**: connects WiFi, optionally syncs NTP, fetches weather, renders a full-quality page, then immediately calls `enterDeepSleep()`.
- **Force Sync path**: `_enterDeepSleepForImmediateWakeup()` uses a 1-second timer so the device wakes almost immediately into the normal timer-fetch path, avoiding a 30-minute wait.
- **EXT0 pin initialisation**: `rtc_gpio_init()` and `rtc_gpio_set_direction()` must be called on `GPIO_NUM_38` before `esp_sleep_enable_ext0_wakeup()`. GPIO34–39 are input-only with no internal pull resistors, so `rtc_gpio_pullup_en()` must not be called on these pins.

**RTC-persisted state:**

| Variable | Type | Purpose |
|---|---|---|
| `rtcCachedWeather` | `WeatherData` | Full weather + 10-day forecast |
| `rtcForecastOffset` | `int` | Current forecast scroll position |
| `rtcWakeupCount` | `uint32_t` | Tracks 24-hour NTP sync interval (every 48 wakeups) |
| `rtcActivePage` | `Page` | Active display page across sleeps |
| `rtcSettingsCursor` | `int` | Settings icon focus across sleeps |
| `rtcLastIP` | `char[16]` | Last WiFi-assigned IP for diagnostics display |
| `rtcCachedSsid` | `char[33]` | SSID of the last successfully connected network; used by `connectBestSTA()` to prefer the fast-connect BSSID/channel cache entry |

### 2. Network Layer

Responsible for all outbound and inbound connectivity.

- **`WiFiManager`**: Toggles the ESP32 between Station (STA) mode for Internet access and SoftAP mode during initial setup. `connectBestSTA()` performs an active WiFi scan, ranks every SSID in the `WeatherConfig` list by RSSI, and connects to the strongest available network. A fast-connect cache (`rtcCachedSsid` + BSSID + channel) in `RTC_DATA_ATTR` is tried first when the cached SSID is still among the candidates, skipping a second channel scan. Falls back to config-order sequential attempts when scanning fails or returns no matches.
- **`NTPManager`**: Uses `configTzTime` / POSIX locale via `setenv("TZ", ...)` + `tzset()`. Explicitly wipes stale RTC data on cold boots. Full NTP sync runs only every 48 timer-wakeup cycles (~24 hours); the BM8563 hardware RTC keeps time between syncs.
- **`WeatherService`**: TLS HTTPS client to the Google Weather API v1. Parses `currentConditions:lookup` (ambient data) and `forecast/days:lookup` (10-day, `pageSize=10`) using ArduinoJson v7 (`JsonDocument`). Flattens results into `WeatherData` / `DailyForecast` structs that are safe to copy into RTC memory.

### 3. Hardware & Display Layer

- **`DisplayManager`**: Built on M5GFX. Manages a full-screen `M5Canvas` sprite; all drawing targets the sprite, which is then pushed to the display in one call. Never issues a global screen clear on partial updates.
  - `epd_quality` — full-page weather redraws only.
  - `epd_fastest` — clock tick, loading step advances, forecast scroll.
  - **Pages**: `Dashboard` (today), `Forecast` (10-day), `Settings`.
  - **Today page**: time/date header; hero icon + temperature; 3-row details grid (feels like, wind+direction, humidity, cloud cover, UV, visibility); AQI gauge + Sun Arc dials with flanking sunrise/sunset times; tomorrow preview card.
  - **Forecast page**: temperature band sparkline (dual Hi/Lo lines); precipitation bar chart; 3-column scrollable cards with weekday labels, icons, H/L text, temperature range bar, and rain chance.
  - **Settings page**: 3-column vector icon grid (Sync / Setup / Sleep); diagnostics row (battery voltage/%, IP, firmware version).
  - **Loading screen**: cloud+sun vector icon (fill→hollow technique), 3-step animated progress bar. `updateLoadingStep(step)` advances via fast partial refresh without redrawing static elements.
  - **Battery gauge**: `analogReadMilliVolts(35) * 2`; M5Unified power abstractions are broken on this hardware revision and must not be used.
  - **IP diagnostics**: `setLastKnownIP()` accepts a cached IP from AppController; shows live, offline-cached, or "No data yet" state.

- **`InputManager`**: Polls G38/G37/G39 and the GT911 capacitive touch panel in a dedicated FreeRTOS task on Core 0.
  - **Swipe gestures**: delta-X tracked in real time; swipe fires mid-drag at 30 px threshold.
  - **Tap detection**: `checkTap(x, y)` returns a pending tap if the touch was released without crossing the 30 px swipe threshold. Consumed on read.
  - **G38 long-press** (≥10 s): triggers re-provisioning flag.
  - **G38 short-press**: increments `_click` counter (used for page cycling in non-Settings views).

### 4. Storage & Provisioning

- **`ConfigManager`**: Interfaces with `Preferences.h` to read/write `WeatherConfig` to the NVS partition (`"wcfg"` namespace). Survives cold boots and battery deaths. Stores up to 5 WiFi SSID/password pairs via indexed keys (`w_ssid_N` / `w_pass_N` + `w_count`); retains legacy `wifi_ssid` / `wifi_pass` keys for rollback compatibility.
  - **AES-256-CTR NVS encryption**: Sensitive fields (`wifi_passes[]`, `api_key`, `webhook_url`) are encrypted before being written to NVS. The 32-byte AES key is derived per-device as `SHA-256(eFuse MAC ∥ "M5PaperWCfgKey-v1")` — hardware-bound, never stored. Each `save()` call generates a fresh 16-byte IV from the ESP32 hardware TRNG; the stored format is `"E1:" + Base64(IV[16] ∥ ciphertext)`. `load()` detects the `"E1:"` prefix and decrypts transparently; values without the prefix are treated as legacy plaintext and returned as-is. On first boot after a firmware upgrade, `begin()` checks the `cfg_encv` NVS flag; if false and the device is already provisioned, a one-time migration re-encrypts all existing plaintext credentials in-place and sets `cfg_encv = true`.
- **`ProvisioningManager` & `WebServer`**: On first boot or G38 hold-at-boot, spins up an `ESPAsyncWebServer` serving a mobile-optimised HTML captive portal from flash. On form submission, saves to NVS and reboots into normal mode.

## 🔄 Execution Flow

1. **Boot**: `main.cpp` initialises M5, `DisplayManager`, `ConfigManager`, `InputManager`.
2. **Provisioning check**: if NVS is empty, G38 held at boot, or force-provisioning flag is set → SoftAP + `ProvisioningManager`.
3. **Normal mode** → `AppController::begin()`:
   - **EXT0 wakeup**: render from RTC cache (or show "No Data Yet") → interactive session → `enterDeepSleep()`.
   - **Timer / Power-On wakeup**: show loading screen (if no cache) → connect WiFi → optionally sync NTP → fetch weather → render → `enterDeepSleep()`.
4. **Deep sleep**: 30-minute timer + EXT0 on `GPIO_NUM_38` (LOW = wakeup). `rtc_gpio_init()` called first to move pin into RTC IO domain.

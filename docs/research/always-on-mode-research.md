# Always-On Low Power Display Mode Research

## 1. Concept Objective

The goal of the "Always-On" mode is to transform the M5Paper from a static weather station into a highly functional desktop clock, without completely draining the battery over a few days.
This requires updating the time every minute (`1-Minute Tick`) while maintaining the low-power 30-minute fetch cycle for weather data.

## 2. Hardware Constraints & Feasibility

Waking the ESP32 every 60 seconds from Deep Sleep sounds expensive, but it is highly efficient **if the Wi-Fi radio remains explicitly powered off** during the clock-tick cycles.

* **Boot Time:** ESP32 wakes from Deep Sleep in ~30ms.
* **Radio:** Wi-Fi modem must NOT be initialized during a minute-tick.
* **E-ink Refresh:** Updating a localized bounding box using `epd_fastest` takes ~200-300ms.
* **Ghosting Accumulation:** Pushing 29 fast-refreshes to the same localized clock area will cause artifacting (ghosting). The firmware must execute a full `epd_quality` screen clearing refresh exactly once every 30 minutes when tying into the weather-fetching cycle.

---

## 3. Deep Sleep State Machine Logic

The application lifecycle in `setup()` must diverge based on an RTC-persisted counter track.

```cpp
// These must be stored in RTC memory to survive the 1-minute sleeps
RTC_DATA_ATTR uint8_t rtc_minutes_since_sync = 30; // Force sync on first boot
RTC_DATA_ATTR struct tm rtc_time_info;

void setup() {
    uint8_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    // Increment our minute tracker
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        rtc_minutes_since_sync++;
        // Increment the internal RTC time structurally
        // ...
    }

    if (rtc_minutes_since_sync >= 30) {
        // --- 30 MINUTE HEAVY SYNC CYCLE ---
        WiFi.begin();
        WeatherService::fetchData();
        NTPManager::syncTime(&rtc_time_info);
        
        DisplayManager::drawMinimalMode(WeatherData, rtc_time_info);
        DisplayManager::pushFullRefresh(epd_quality); 
        
        rtc_minutes_since_sync = 0;
        WiFi.disconnect(true);
    } else {
        // --- 1 MINUTE LIGHT TICK CYCLE ---
        // DO NOT START WIFI OR BACKGROUND SERVICES
        DisplayManager::updateClockOnly(rtc_time_info);
        DisplayManager::pushPartialRefresh(epd_fastest, clock_bounding_box);
    }
    
    // Sleep exactly until the top of the next minute
    uint64_t timeToSleepUs = calculateMicrosecondsUntilNextMinute(rtc_time_info);
    esp_sleep_enable_timer_wakeup(timeToSleepUs);
    esp_deep_sleep_start();
}
```

---

## 4. Minimal Mode UI Mockup

To minimize ghosting and power logic overhead, the "Always-On Minimum" screen strips out detailed arrays (hourly, pollen, charts) and focuses solely on extreme glanceability.

*ASCII scale used below: 1 character ≈ 10 px wide · 1 line ≈ 20 px tall.*
*Display is 540 × 960 px. (Vertical)*

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │   [███░░ ▌] 65%                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
 200 │                      10:42 AM                        │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
 600 │                    [Cloud Icon]                      │
     │                        72°F                          │
     │                                                      │
     │                   H: 78°  L: 65°                     │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
     │                                                      │
 960 └──────────────────────────────────────────────────────┘
```

### Visual Architecture Details

1. **The Clock Block (Y: 200 to 400)**
   * **Font:** `FreeSansBold48pt` or larger.
   * **Update Strategy:** This is the ONLY element that changes on the 1-minute tick.
   * **Refresh Mode:** `epd_fastest` applied strictly to a bounding box wrapping `x:0, y:200, w:540, h:200`.
2. **The Weather Block (Y: 600 to 800)**
   * **Scope:** Main Icon, Current Temperature, High/Low.
   * **Update Strategy:** Redrawn only once every 30 minutes when `rtc_minutes_since_sync >= 30`.
3. **Ghosting Control**
   * Keeping the space between the Clock and the Weather completely blank prevents artifact collisions when bounding boxes accidentally overlap.

---

## 5. Captive Portal Configuration

Users must be able to toggle this feature depending on their use case (Desktop setup vs Wall mounted setup).

Add to the Settings schema inside `ConfigManager.h`:

```json
{
  "display_mode": "minimal_always_on", // Enum: "standard", "minimal_always_on"
  "always_on_sync_interval": 30        // Minutes (Weather Sync interval)
}
```

* **"standard"**: Legacy mode. Screen updates once every 30 mins and displays dense information. Does not wake up on the minute.
* **"minimal_always_on"**: Switches to the minimal sparse UI and engages the 1-minute tick timer.

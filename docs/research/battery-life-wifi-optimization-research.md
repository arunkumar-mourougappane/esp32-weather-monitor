# Battery Life & WiFi-Off Clock Update Research

## 1. Executive Summary

This document is a deep-dive into battery life extension strategies for the M5Paper Weather
Monitor and specifically answers: **can WiFi be kept off while the clock is still updated?**

**Short answer: yes — and it already mostly works that way.** The BM8563 hardware RTC drives
all clock rendering on button-press wakeups. WiFi is only powered on during timer-triggered
weather fetch cycles. However, several concrete gaps exist where current is being wasted that
can meaningfully extend runtime.

---

## 2. Current Power Architecture (As-Built)

### 2.1 Wakeup Paths

There are two wakeup sources, and they follow completely different power paths:

| Wakeup | WiFi | NTP | Weather Fetch | Clock Source |
|--------|------|-----|---------------|--------------|
| **Timer** (every N min) | ✅ Connected | Only every 48 cycles (~24h) | ✅ Full fetch | System clock (NTP-synced) |
| **EXT0 / G38 button** | ❌ Never connected | ❌ Never | ❌ Never | BM8563 hardware RTC |

On a button press, the device reads `rtcCachedWeather` from `RTC_DATA_ATTR` memory, renders
the display from that frozen snapshot, and runs the 10-minute interactive loop entirely
offline. The clock ticks using `NTPManager::getLocalTime()` which falls back to the BM8563
I²C chip when the system clock is not set. This means **every clock update during an
interactive session costs zero WiFi energy**.

### 2.2 WiFi On-Time Per Cycle

During a full timer-wakeup fetch, WiFi is powered on from `connectBestSTA()` until
`esp_deep_sleep_start()`. Based on the code path, the worst-case WiFi-on window is:

| Phase | Estimated Time |
|-------|----------------|
| BSSID fast-connect (cached) | 1–3 s |
| BSSID scan + connect (cold) | 5–15 s |
| NTP sync (when due, every 24h) | up to 30 s (3 × 10 s retry budget) |
| HTTP: current conditions | 2–5 s (TLS + JSON) |
| HTTP: 10-day forecast | 2–5 s |
| HTTP: AQI + pollen (open-meteo) | 2–5 s |
| HTTP: hourly + sun (open-meteo) | 2–5 s |
| HTTP: weather alerts | 1–3 s |
| Render + sleep prep | 1–2 s |
| **Total WiFi-on (typical)** | **~15–35 s per cycle** |

At a 30-minute interval that is **0.8–2% duty cycle** for the radio. The ESP32 WiFi radio in
active transmit draws ~130–230 mA; deep sleep draws ~10 µA. This radio window is the dominant
power cost per cycle.

### 2.3 Battery Sampling

Battery voltage is read via `analogReadMilliVolts(35) * 2` (GPIO 35, resistor-divided from the
battery pack). Four samples are averaged inside `DisplayManager::sampleBattery()`. The call is
gated by a 5-second cache in `_drawBattery()` so ADC noise from multiple renders doesn't stack
up. The value is also cached in `_cachedBatVoltage` and reused by `AppController::enterDeepSleep()`
without re-sampling.

---

## 3. Identified Power Leaks

### 3.1 WiFi Radio Not Explicitly Shut Down Before Sleep

**Current code** (`AppController::enterDeepSleep()`):
```cpp
esp_sleep_enable_timer_wakeup(sleepUs);
_configureEXT0Wakeup();
delay(500);
esp_deep_sleep_start();
```

`WiFi.disconnect(true)` and `WiFi.mode(WIFI_OFF)` are **never called** before entering deep
sleep. The ESP32 deep sleep does power off the RF block, but the graceful sequence
recommended by Espressif to prevent phantom current during the transition (before the sleep
latch fully engages) is absent.

**Recommendation** — add to `enterDeepSleep()` and `_enterDeepSleepForImmediateWakeup()`:
```cpp
// Explicitly shut radio down before the deep-sleep latch engages
WiFi.disconnect(true);
WiFi.mode(WIFI_OFF);
esp_wifi_stop();
```
Typical savings: 5–20 µA of phantom draw during the sleep transition; more importantly,
this prevents a rare edge case where the modem stays partially awake if a previous
`connectBestSTA()` left the radio in `WIFI_STA` mode.

### 3.2 WiFi Modem Not Put to Sleep During HTTP Transfer Window

Once connected, the radio operates at full transmit power. Enabling **modem sleep** allows
the WiFi modem to doze between DTIM beacon intervals while the CPU is active. This reduces
the average radio current during the 15–35 s window by ~40–60%.

```cpp
// After WiFi.begin() / connect succeeds, before HTTP fetches:
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // DTIM-aligned modem sleep
```

`WIFI_PS_MAX_MODEM` would save even more but introduces latency spikes that can cause
`HTTPClient` timeout on slow APs. `WIFI_PS_MIN_MODEM` is the safer default.

### 3.3 Interactive Session Runs at Full CPU Frequency

The 10-minute interactive loop polls at 100 Hz (`delay(10)`) on a CPU running at the Arduino
default of 240 MHz. During this time the device is only:
- Reading the BM8563 over I²C (once per minute)
- Polling touch / button state
- Conditionally re-rendering the display

There is no computation that justifies 240 MHz.

**Recommendation — two options:**

**Option A: CPU frequency scaling**
```cpp
// At the start of _runInteractiveSession():
setCpuFrequencyMhz(80);  // Arduino API; drops dynamic power ~65%
// ...interactive loop...
// Restore before WiFi needs it (WiFi requires ≥80 MHz on ESP32):
// (80 MHz is the minimum; WiFi still works)
```

**Option B: Light sleep between ticks**
```cpp
// Replace delay(10) in the interactive loop with:
esp_sleep_enable_timer_wakeup(10000); // 10 ms
esp_light_sleep_start();
```
Light sleep halts the CPU and most peripherals between ticks while preserving RAM and
GPIO state. At 100 Hz this is mostly wake overhead, so the real benefit only comes if
the tick rate is reduced to 10–20 Hz (100 ms) — which is sufficient for touch events.

Combining both: drop to 80 MHz and increase the poll period to 50 ms (20 Hz). Clock
rendering fires at most once per minute so 20 Hz input polling adds imperceptible latency.

### 3.4 Sequential HTTP Requests Keep WiFi On Unnecessarily Long

All five HTTP fetches (current conditions, forecast, AQI, sun/hourly, alerts) run
**sequentially** with their own TLS handshakes. Total WiFi-on time is the sum of all five
round trips plus five separate TLS negotiation costs.

**Recommendation — parallel fetch via FreeRTOS tasks:**

The `WeatherService.cpp` already includes `freertos/event_groups.h` (imported but currently
unused). Two independent groups can be fetched in parallel because they hit different hosts:
- Group A: `googleapis.com` — current conditions + alerts (same TLS session, reuse possible)
- Group B: `open-meteo.com` — AQI and sun/hourly (same root CA, different subdomains)

The forecast fetch hits `googleapis.com` so it can share a connection with Group A.

Estimated savings: 6–12 s off the WiFi-on window per cycle, roughly a 30% reduction.

### 3.5 NTP Retry Budget Holds WiFi On for Up to 30 Extra Seconds

`NTPManager::sync()` allows 3 retries of 10 s each = 30 s worst-case just for NTP. This
only fires once per 24 hours (every 48 wakeups), but on a bad network day this is 30 s of
idle WiFi-on time on top of the weather fetches.

NTP is already only needed once every 24 hours; the BM8563 RTC drift over 24 h is
< 2 seconds. A 5-second single-attempt NTP sync (with fallback to BM8563) would be
sufficient in nearly all cases and would reduce the worst-case NTP window from 30 s to 5 s.

---

## 4. WiFi-Off Clock Updates — Deep Dive

### 4.1 How Time Is Kept Without WiFi

The M5Paper has a **BM8563** real-time clock IC connected via I²C. `NTPManager::sync()`
writes the authoritative UTC time into this chip:

```cpp
M5.Rtc.setDateTime(rtc_time);  // NTPManager.cpp:89
```

On button wakeup, `NTPManager::getLocalTime()` calls the standard `::getLocalTime()` first.
If the system clock has been wiped (e.g., fresh boot after a flash), it falls back to:

```cpp
m5::rtc_datetime_t bb_time = M5.Rtc.getDateTime();
// ... reconstructs tm, calls settimeofday() to re-seed the system clock
```

Since the system clock persists across deep sleep (ESP32 RTC domain keeps the `timeval`
via `RTC_SLOW_MEM`), the typical button-wakeup path is:

```
esp_sleep_wakeup → system clock still valid from last NTP sync → localtime_r() works fine
```

The BM8563 fallback is only needed on the very first boot after flashing.

### 4.2 Clock Display During Interactive Sessions

`_runInteractiveSession()` refreshes the clock display when `localTime.tm_min != lastMinute`:

```cpp
if (localTime.tm_min != lastMinute) {
    lastMinute = localTime.tm_min;
    if (disp.getActivePage() == Page::Dashboard && rtcCachedWeather.valid) {
        disp.updateClockOnly(localTime, NTPManager::getInstance().isNtpFailed());
        lastActivityMs = millis();
        continue;
    }
}
```

`updateClockOnly()` uses `epd_fastest` mode (partial refresh) — no full-screen flash, and
no WiFi is needed. The time displayed will drift by at most ±1 second per 24 hours (BM8563
spec: ±3 ppm at room temperature).

### 4.3 Can the Clock Be Kept Running During Deep Sleep?

The ESP32 itself cannot update the display during deep sleep — the CPU is off. The BM8563
keeps counting time, but no code runs to push a new frame to the e-ink controller.

However, the e-ink display retains its last image without power. So the screen shows
the correct time as of the last render, which is refreshed on every button press. This
is already the correct user experience: the display is a *snapshot* that updates on demand
(button press or 30-min sync), not a live clock.

If a true always-on clock is required, the only architecturally sound path is **ULP
(Ultra-Low-Power) coprocessor wake** to drive the I²C display controller — but the
M5Paper's IT8951 e-ink controller requires a full SPI initialization sequence each update,
which is too complex for the ULP coprocessor's limited instruction set. This is not feasible
on this hardware without adding an external microcontroller.

---

## 5. Night Mode — No-WiFi Sleep Extension

The single highest-impact battery life change is skipping the weather fetch entirely
during nighttime hours when the user is not looking at the display.

### 5.1 Logic

At the point where `enterDeepSleep()` calculates `sleepUs`, it already has access to:
- Current local time via `NTPManager::getLocalTime()`
- User-configured base interval from `cfg.sync_interval_m`

```cpp
// Proposed addition to AppController::enterDeepSleep():
struct tm now = {};
NTPManager::getInstance().getLocalTime(now);
int hour = now.tm_hour;

// Night mode: 22:00 → 06:00 — sleep until morning instead of syncing
constexpr int kNightStart = 22;
constexpr int kNightEnd   = 6;
bool isNight = (hour >= kNightStart || hour < kNightEnd);
if (isNight) {
    // Calculate minutes until 06:00
    int minutesUntilMorning;
    if (hour >= kNightStart) {
        minutesUntilMorning = (24 - hour + kNightEnd) * 60 - now.tm_min;
    } else {
        minutesUntilMorning = (kNightEnd - hour) * 60 - now.tm_min;
    }
    sleepUs = (uint64_t)minutesUntilMorning * 60ULL * 1000000ULL;
    ESP_LOGI(TAG, "Night mode: sleeping %d min until 06:00", minutesUntilMorning);
}
```

At 10 PM, this would skip ~8 WiFi fetch cycles (one every 30 min), saving ~8 × 35 s of
radio-on time per night. For a device that otherwise runs 24/7, night mode alone could
extend battery life by 20–30%.

The night window start/end hours should be user-configurable via the provisioning portal
and stored in NVS alongside `sync_interval_m`.

---

## 6. Battery Life Budget Model

Using the ESP32 datasheet current figures and measured/estimated timings:

| State | Current Draw | Time Per 30-min Cycle | Energy (mAh) |
|-------|-------------|----------------------|--------------|
| Deep sleep | 10 µA | ~28.5 min | 0.005 mAh |
| CPU active (no WiFi, 240 MHz) | ~50 mA | ~0.5 min | 0.417 mAh |
| WiFi TX/RX | +130–180 mA | ~0.5 min | 1.5–2.0 mAh |
| E-ink full refresh | ~50 mA | ~1.3 s | 0.018 mAh |
| **Cycle total (30 min)** | | | **~2.0–2.5 mAh** |

For the **M5Paper's ~950 mAh battery** (Li-Po on the M5Stack Paper):

| Scenario | Cycles/Day | mAh/Day | Estimated Runtime |
|----------|-----------|---------|-------------------|
| Current (30 min, no night mode) | 48 | ~110 mAh | **~8–9 days** |
| + WiFi off before sleep (3.1) | 48 | ~108 mAh | +0.2 days |
| + Modem sleep during fetch (3.2) | 48 | ~90 mAh | **~10.5 days** |
| + CPU 80 MHz interactive (3.3) | 48 | ~88 mAh | +0.2 days |
| + Parallel fetches / -10s WiFi (3.4) | 48 | ~80 mAh | **~12 days** |
| + Night mode 22:00–06:00 (5.1) | 32 | ~65 mAh | **~14–15 days** |
| + Night mode + 60-min base interval | 16 | ~40 mAh | **~23 days** |

> **Note:** These are estimates. Actual draw depends on WiFi signal quality (weak signal
> ↑ TX power), AP DTIM interval, JSON payload size, and ambient temperature. The M5Paper
> uses a PMIC that adds ~5–10 mA baseline to all states.

---

## 7. Implementation Priority

| Priority | Change | Est. Effort | Est. Gain |
|----------|--------|-------------|-----------|
| 1 (quick win) | `WiFi.mode(WIFI_OFF)` before deep sleep | 2 lines | Minor / correctness fix |
| 2 (quick win) | `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` after connect | 1 line | ~20–30% radio current reduction |
| 3 (medium) | Reduce interactive poll from 100 Hz → 20 Hz + 80 MHz CPU | ~10 lines | ~5% overall |
| 4 (medium) | Night mode sleep calculation | ~25 lines + NVS key + portal UI | ~25–35% |
| 5 (large) | Parallel HTTP fetches via FreeRTOS event groups | ~100 lines refactor | ~15% |
| 6 (large) | User-configurable night window in provisioning portal | Full UI + NVS schema | usability |

---

## 8. Key Constraints and Pitfalls

### WiFi Minimum CPU Frequency
The ESP32 WiFi stack requires the CPU to run at ≥ 80 MHz. Dropping to 40 MHz (another
common preset) **will crash the WiFi driver**. The timer-wakeup fetch path must keep the
CPU at 80 MHz or higher while WiFi is in use.

### Light Sleep + WiFi
`esp_light_sleep_start()` can coexist with WiFi in **modem-sleep** mode but **not** in
fully disconnected state. Do not call `esp_light_sleep_start()` after `WiFi.mode(WIFI_OFF)`;
use `esp_deep_sleep_start()` instead.

### Interactive Webhook and WiFi
`_runInteractiveSession()` has a double-click webhook path that calls `http.GET()`.
On EXT0 wakeup, WiFi is never connected — this call will silently fail. If webhook
functionality is required during interactive sessions, the session start should optionally
connect WiFi when a webhook URL is configured. This is a current silent failure that
should be documented as a known limitation.

### BM8563 Drift Over Extended Night Sleep
Sleeping for 8 hours without NTP will accumulate at most ±86 ms of BM8563 drift
(3 ppm × 8 h × 3600 s × 1000 ms/s ≈ 86 ms). This is imperceptible for a weather display.
The once-per-24h NTP resync (every 48 wakeups) handles correction automatically.

### Battery Voltage After Long Sleep
When the device wakes from a multi-hour night sleep, the first `sampleBattery()` call
reads a slightly elevated voltage because the cell has been at rest. Adding a brief
(100 ms) active delay before the ADC read would improve accuracy, but the effect is small
(< 50 mV) and does not affect the protective thresholds materially.

---

## 9. References

- ESP32 Technical Reference Manual §4 (Power Management), §9 (ULP Coprocessor)
- Espressif ESP-IDF Power Management API docs: `esp_pm_configure()`, `esp_wifi_set_ps()`
- BM8563 datasheet — Section 5.1: Oscillator accuracy ±3 ppm at 25 °C
- M5Paper hardware schematic: GPIO 35 = battery voltage divider (1:1 ratio)
- `lib/App/AppController.cpp` — `enterDeepSleep()`, `_runInteractiveSession()`
- `lib/Network/WiFiManager.cpp` — `connectBestSTA()`, fast-connect via RTC BSSID cache
- `lib/Network/NTPManager.cpp` — `sync()`, `getLocalTime()` BM8563 fallback path
- `lib/Network/WeatherService.cpp` — 5 sequential HTTPS fetches, `freertos/event_groups.h` import

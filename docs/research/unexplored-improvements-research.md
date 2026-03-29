# Unexplored Improvement Areas Research

This document covers improvement areas **not** addressed by any of the eight existing
research documents. Each section identifies a concrete gap found in the codebase, explains
the risk or missed opportunity, and proposes a specific implementation path.

---

## 1. TLS Certificate Verification

### Current State
All five HTTPS fetches in `WeatherService.cpp` disable TLS verification:
```cpp
WiFiClientSecure client;
client.setInsecure();
ESP_LOGW(TAG, "TLS certificate verification DISABLED — MITM injection of weather data is possible");
```
A comment already flags this. Anyone on the same network can intercept the Google Weather API
response and inject arbitrary weather data (or consume the device's quota via replayed requests).

### Risk
The API key is transmitted in the URL query string over a TLS connection whose server
certificate is never checked. A MITM attacker on the local network can intercept the
handshake, present a self-signed cert, and read the key in plaintext.

### Recommended Fix
Pin the two root CAs needed by the current endpoints:

| Endpoint | Root CA |
|----------|---------|
| `weather.googleapis.com` | GTS Root R1 (Google Trust Services) |
| `air-quality-api.open-meteo.com` | ISRG Root X1 (Let's Encrypt) |
| `api.open-meteo.com` | ISRG Root X1 (Let's Encrypt) |

```cpp
// Store in flash (PROGMEM) to avoid eating DRAM
static const char kGTSRootR1_PEM[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
... GTS Root R1 PEM ...
-----END CERTIFICATE-----
)EOF";

static const char kISRGRootX1_PEM[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
... ISRG Root X1 PEM ...
-----END CERTIFICATE-----
)EOF";

// Before googleapis.com fetches:
client.setCACert(kGTSRootR1_PEM);
// Before open-meteo.com fetches:
client.setCACert(kISRGRootX1_PEM);
```

**Cert expiry management:** Root CAs rotate on a multi-year cycle. Store the PEMs as build
flags or in a dedicated NVS key so they can be updated via OTA without a full reflash.
The device should log a warning when the cert within 90 days of expiry (derivable from the
`notAfter` field after a successful handshake).

---

## 2. OTA (Over-The-Air) Firmware Updates

### Current State
There is **zero OTA infrastructure**. No `ArduinoOTA`, no `httpUpdate`, no `Update` library
usage anywhere in the codebase. Updating firmware requires physical USB access.

### Why This Matters
The device is intended to run unattended for weeks. Bug fixes and new features (such as those
planned in the 17 open issues) cannot be deployed without physical access.

### Recommended Implementation

**Option A — Pull OTA from a URL (simplest)**

Add a "Check for Update" button to the Settings page that triggers an HTTP fetch of a
manifest, then a firmware binary download if a newer version exists.

```cpp
// In AppController, after user triggers "Check Update":
#include <HTTPUpdate.h>

WiFiClientSecure tlsClient;
tlsClient.setCACert(kGTSRootR1_PEM); // or your own CDN cert

t_httpUpdate_return ret = httpUpdate.update(
    tlsClient,
    "https://your-cdn.example.com/firmware/latest.bin"
);
switch (ret) {
    case HTTP_UPDATE_OK:        ESP_LOGI(TAG, "OTA succeeded"); break;
    case HTTP_UPDATE_FAILED:    ESP_LOGE(TAG, "OTA failed: %s", httpUpdate.getLastErrorString().c_str()); break;
    case HTTP_UPDATE_NO_UPDATES: ESP_LOGI(TAG, "Already up to date"); break;
}
```

**Option B — Provisioning portal upload**

Extend `ProvisionWebServer` with a `/update` POST endpoint that receives a multipart binary
and writes it via `Update.write()`. The user uploads the `.bin` from the same captive portal
used for WiFi setup.

```cpp
_server->on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
        req->send(Update.hasError() ? 500 : 200, "text/plain",
                  Update.hasError() ? "FAIL" : "OK");
        delay(500);
        esp_restart();
    },
    [](AsyncWebServerRequest* req, String filename,
       size_t index, uint8_t* data, size_t len, bool final) {
        if (!index) Update.begin();
        Update.write(data, len);
        if (final) Update.end(true);
    }
);
```

**Version manifest format (`version.json`):**
```json
{ "version": "3.2.0", "url": "https://cdn/v3.2.0.bin", "sha256": "abc123..." }
```
The device compares `APP_VERSION` (already a build flag) against the manifest `version` and
only downloads if newer, verifying the SHA-256 digest before committing.

---

## 3. Imperial / Metric Unit Toggle

### Current State
All temperatures are hard-coded in Celsius and wind speeds in km/h throughout
`DisplayManager.cpp`:
```cpp
snprintf(tempBuf, sizeof(tempBuf), "%.1f C", data.tempC);
snprintf(buf1, sizeof(buf1), "Feels: %.1f C", data.feelsLikeC);
snprintf(buf2, sizeof(buf2), "%s %.0f km/h", windDir, data.windKph);
```
There is no `use_imperial` flag in `WeatherConfig` and no conversion path.

### Fix
Add a single boolean to `WeatherConfig`:
```cpp
bool use_imperial = false; ///< Display °F and mph when true; °C and km/h when false.
```

Inline conversion helpers in `DisplayManager` (no new files needed):
```cpp
static float toDisplayTemp(float c, bool imp) { return imp ? c * 9.0f / 5.0f + 32.0f : c; }
static float toDisplayWind(float kph, bool imp) { return imp ? kph * 0.621371f : kph; }
static const char* tempUnit(bool imp) { return imp ? "F" : "C"; }
static const char* windUnit(bool imp) { return imp ? "mph" : "km/h"; }
```

All `snprintf` calls in `DisplayManager` would reference `cfg.use_imperial` via a display
state flag injected at `begin()` time. The raw API values remain in SI units in `WeatherData`
— only the rendered string changes.

Expose the toggle in the provisioning portal as a radio button: **°C / km/h** vs **°F / mph**.

---

## 4. Input Validation — Lat/Lon and API Key

### Current State
`WebServer.cpp` validates timezone via an allowlist and validates PIN length, but **does
not validate latitude, longitude, or API key format** before saving to NVS:
```cpp
cfg.api_key = get("api_key");  // any string accepted
cfg.lat     = get("lat");      // "banana" would be saved
cfg.lon     = get("lon");
```
An invalid lat/lon string silently passes through to `WeatherService::fetch()` which
constructs a malformed URL, gets HTTP 400 from Google, and permanently fails until
re-provisioned.

### Fix
Add server-side validation in `WebServer.cpp`:
```cpp
// Validate lat/lon are numeric and in range
float latF = get("lat").toFloat();
float lonF = get("lon").toFloat();
if (latF < -90.0f || latF > 90.0f) {
    req->send(400, "text/plain", "Latitude must be between -90 and 90");
    return;
}
if (lonF < -180.0f || lonF > 180.0f) {
    req->send(400, "text/plain", "Longitude must be between -180 and 180");
    return;
}
// Validate API key is non-empty and looks like a Google API key (39 chars, alphanumeric)
String apiKey = get("api_key");
if (apiKey.length() < 20 || apiKey.length() > 50) {
    req->send(400, "text/plain", "API key length looks invalid");
    return;
}
```

The provisioning portal HTML should also add `min`/`max` and `pattern` attributes to the
lat/lon inputs for client-side feedback before the form is even submitted.

---

## 5. PIN Security — Missing Salt

### Current State
The PIN is hashed with raw SHA-256 with no salt:
```cpp
// WebServer.cpp
String pinHash = _sha256(pin);  // SHA-256("1234") is always the same value
```
A pre-computed rainbow table for 4–8 digit PINs (a tiny space) would crack any PIN stored
this way in milliseconds.

### Fix
Add a random 16-byte salt generated at provisioning time, stored alongside the hash:
```cpp
// Generate salt once at provisioning time
uint8_t salt[16];
for (int i = 0; i < 16; i += 4) {
    uint32_t r = esp_random();
    memcpy(salt + i, &r, 4);
}
String saltHex = bytesToHex(salt, 16);
String pinHash = _sha256(saltHex + pin);  // hash(salt || pin)

cfg.pin_salt = saltHex;  // new NVS field
cfg.pin_hash = pinHash;
```

`InputManager::verifyPIN()` would read the salt from `ConfigManager` and prepend it before
hashing. This makes rainbow tables infeasible even for the tiny PIN space.

---

## 6. API Rate Limit Handling (HTTP 429)

### Current State
`WeatherService.cpp` handles HTTP errors uniformly — a 400, 403, 429, or 500 all produce
the same `ESP_LOGE` log line and fall through to `data.valid = false`. There is no
back-off or rate-limit awareness:
```cpp
} else {
    ESP_LOGE(TAG, "HTTP GET current conditions failed: %d — will still attempt supplemental fetches", code);
}
```

### Risk
If the device receives a 429 (quota exhausted) response, it will retry on the next
30-minute wake cycle, burning more quota. A sustained 429 loop wastes all API calls for
the day and keeps issuing failed HTTPS connections.

### Fix
Persist a backoff state in `RTC_DATA_ATTR`:
```cpp
RTC_DATA_ATTR uint8_t rtcApiBackoffCycles = 0; // remaining cycles to skip

// In AppController, before WiFi connect:
if (rtcApiBackoffCycles > 0) {
    rtcApiBackoffCycles--;
    ESP_LOGW(TAG, "API backoff active: %d cycles remaining", rtcApiBackoffCycles);
    // Render cached data with a "quota" badge, skip fetch
    enterDeepSleep();
    return;
}
```

In `WeatherService::fetch()`, return a distinct error code for 429:
```cpp
if (code == 429) {
    ESP_LOGW(TAG, "API quota exceeded (HTTP 429) — setting 6-cycle backoff");
    data.rateLimited = true;  // new field
    return data;
}
```
`AppController` sets `rtcApiBackoffCycles = 6` (3 hours at 30-min intervals) and renders a
"API Quota" badge on the dashboard.

---

## 7. Production Build — Verbose Logging Always On

### Current State
`platformio.ini` sets `CORE_DEBUG_LEVEL=3` unconditionally — this is `ESP_LOGI` level
(verbose) in **all** builds including production:
```ini
build_flags =
    -D CORE_DEBUG_LEVEL=3
```
Every `ESP_LOGI` call formats a string and writes it over UART at 115200 baud. This
wastes CPU time, keeps the UART peripheral clock running, and produces tens of kilobytes
of serial output per wake cycle.

### Fix
Separate debug and release build environments in `platformio.ini`:
```ini
[env:m5stack_paper]           ; Release
build_flags =
    -D CORE_DEBUG_LEVEL=1     ; Only errors
    -D APP_VERSION=\"3.1.0\"
    ...

[env:m5stack_paper_debug]     ; Debug
build_flags =
    -D CORE_DEBUG_LEVEL=3     ; Verbose
    -D APP_VERSION=\"3.1.0-dev\"
    ...
```

Alternatively, disable the UART peripheral entirely after the boot splash in release builds:
```cpp
#ifndef DEBUG_BUILD
    Serial.end();  // saves ~1 mA with UART clock gated
#endif
```

---

## 8. Webhook Enhancement — POST with Weather Payload

### Current State
The webhook fires a bare `HTTP GET` with no payload:
```cpp
http.begin(cfg.webhook_url);
int code = http.GET();
```
This is only useful for simple triggers (e.g., "ping IFTTT"). There is no way to transmit
the current weather state to an external service.

### Improvements
**Option A — POST with JSON payload**
```cpp
http.begin(cfg.webhook_url);
http.addHeader("Content-Type", "application/json");

char body[256];
snprintf(body, sizeof(body),
    "{\"temp\":%.1f,\"humidity\":%d,\"condition\":\"%s\",\"aqi\":%d,\"bat_pct\":%d}",
    rtcCachedWeather.tempC,
    rtcCachedWeather.humidity,
    rtcCachedWeather.condition,
    rtcCachedWeather.aqi,
    DisplayManager::getInstance().getBatLevel());
int code = http.POST(body);
```

**Option B — Home Assistant webhook integration**
Home Assistant's webhook automation trigger accepts a POST with arbitrary JSON. With the
payload above, automations like "turn on fan when temp > 28°C" or "send notification when
AQI > 150" become possible without any additional hardware.

**Option C — MQTT publish** (see §12)

---

## 9. RTC Memory Pressure — WeatherData Struct Size

### Current State
`WeatherData` is stored in `RTC_DATA_ATTR` (RTC slow memory, 8 KB total on ESP32):
```
WeatherData fields:
  char condition[64]           =   64 bytes
  float/int scalar fields      =   ~60 bytes
  DailyForecast forecast[10]   = 10 × ~52 bytes  = 520 bytes
  HourlyForecast hourly[24]    = 24 × ~28 bytes  = 672 bytes
  char alertHeadline[80]       =   80 bytes
  char alertSeverity[24]       =   24 bytes
  ─────────────────────────────────────────────
  TOTAL                        ≈  1,420 bytes
```

Combined with the other `RTC_DATA_ATTR` variables (pressure ring, battery ring, IP, WiFi
BSSID, etc.), total RTC usage is approximately **1.6–1.7 KB**. The ESP32's RTC slow memory
limit is **8 KB** — usage is currently safe but should be monitored as new RTC fields are
added (e.g., from planned night mode, always-on mode counters).

### Recommendation
Add a compile-time assertion to catch overflows early:
```cpp
// In AppController.cpp:
static_assert(sizeof(WeatherData) < 2048,
    "WeatherData exceeds safe RTC budget — consider trimming hourly[] array");
```

If RTC memory becomes constrained, reduce `hourly[24]` to `hourly[12]` (12 hours is
sufficient for the hourly page) saving ~336 bytes.

---

## 10. Factory Reset via UI

### Current State
`ConfigManager::clear()` exists and erases NVS, but there is **no user-accessible UI
trigger** for it. The only factory reset path is holding G38 at boot (which triggers
re-provisioning but does not erase existing credentials) or reflashing.

### Fix
Add a "Factory Reset" option to the Settings page alongside the existing Sync, Setup, and
Sleep actions. A long-press confirmation (3 seconds) should be required to prevent
accidental triggers.

```cpp
// Settings page col 3 (new 4th column, or replace an existing less-used action):
if (col == 3) {
    disp.showMessage("Hold to Reset", "Hold 3s to factory reset");
    uint32_t holdStart = millis();
    while (input.checkLongPress()) {
        if (millis() - holdStart > 3000) {
            ConfigManager::getInstance().clear();
            disp.showMessage("Reset Complete", "Rebooting...");
            delay(1500);
            esp_restart();
        }
    }
}
```

---

## 11. Configuration Backup and Restore

### Current State
There is no way to back up the device configuration. If NVS becomes corrupt (power loss
during write, flash wear), all credentials must be re-entered manually.

### Fix
Expose a `/config/export` and `/config/import` endpoint on the provisioning web server:

**Export** — returns a JSON object with all non-secret fields plus encrypted blobs:
```json
{
  "city": "Chicago",
  "state": "IL",
  "lat": "41.8781",
  "lon": "-87.6298",
  "timezone": "CST6CDT,M3.2.0,M11.1.0",
  "sync_interval_m": 30,
  "wifi_ssids": ["HomeNetwork"],
  "wifi_passes_enc": ["E1:..."],
  "api_key_enc": "E1:...",
  "webhook_url_enc": "E1:...",
  "pin_hash": "...",
  "app_version": "3.1.0"
}
```

**Import** — accepts the same JSON and calls `ConfigManager::save()`. The encrypted blobs
can only be decrypted on the same device (eFuse MAC-derived key), so the export is safe to
store in cloud backup without exposing plaintext credentials.

---

## 12. Home Automation Integration (MQTT / Home Assistant)

### Current State
The only external push mechanism is the single-endpoint webhook GET. There is no persistent
connection or structured telemetry stream.

### Proposed MQTT Integration
Add an optional MQTT publisher that runs during the timer-wakeup fetch cycle (while WiFi is
already on) to push sensor readings to a broker:

```cpp
// New lib/Network/MQTTPublisher.h
class MQTTPublisher {
public:
    bool publishWeather(const WeatherData& data, float batV);
};

// Topics published:
// weather/temp          → "21.5"
// weather/humidity      → "62"
// weather/condition     → "Partly cloudy"
// weather/aqi           → "45"
// device/battery_v      → "3.85"
// device/battery_pct    → "72"
// device/rssi           → "-67"
```

**Home Assistant auto-discovery** — publish a discovery payload to
`homeassistant/sensor/m5paper_weather/config` once at first boot so the device appears
automatically as a HA entity with no manual configuration.

**NVS config additions:** `mqtt_broker`, `mqtt_port`, `mqtt_user`, `mqtt_pass_enc`,
`mqtt_enabled` (bool).

Since WiFi is already on during the fetch cycle, the MQTT publish adds only ~100 ms of
extra WiFi-on time.

---

## 13. Timezone Auto-Detection

### Current State
The user must select their timezone from a `<select>` dropdown in the provisioning portal
containing ~40 hard-coded POSIX TZ strings. An incorrect selection causes all timestamps to
display in the wrong local time.

### Fix
The open-meteo `forecast` endpoint already returns `"timezone": "America/Chicago"` in its
response body. The device can infer the POSIX TZ string from this without any user input.

**Lookup table approach** — a compact `pgm_read`-friendly array in flash:
```cpp
struct TZEntry { const char* ianaName; const char* posixStr; };
static const TZEntry kTZTable[] PROGMEM = {
    {"America/Chicago",    "CST6CDT,M3.2.0,M11.1.0"},
    {"America/New_York",   "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles","PST8PDT,M3.2.0,M11.1.0"},
    // ... ~80 common zones ...
    {nullptr, nullptr}
};
```

On first successful fetch after provisioning, the device writes the derived POSIX string
back to NVS via `ConfigManager::save()`. The provisioning portal can still offer a manual
override dropdown.

---

## 14. Historical Data Visualisation — Pressure & Battery Charts

### Current State
The firmware accumulates a 3-sample rolling pressure ring (`rtcPressureRing[3]`) and an
8-sample battery voltage ring (`rtcBatRing[8]`), both stored in `RTC_DATA_ATTR`. These
rings are used only for scalar calculations (trend direction, runtime estimate) — the raw
values are never rendered as a chart.

### Opportunity
Both rings are already populated every cycle; displaying them requires only new
`DisplayManager` drawing code with no network or data changes.

**24-hour pressure sparkline** on the Dashboard overlay:
- x-axis: time (samples at 30-min intervals)
- y-axis: hPa range ±5 around the current value
- Arrow glyph (↑↗→↘↓) derived from `pressureTrend` already computed

**Battery discharge curve** on the Settings page:
- x-axis: cycle count
- y-axis: voltage (3.2V–4.2V)
- Annotated with the current `batteryRuntimeH` estimate

Extending both ring buffers from 3→48 (pressure) and 8→48 (battery) would enable
full 24-hour charts at 30-minute resolution. The additional RTC memory cost would be
48 × 4 bytes × 2 = **384 bytes** — well within the 8 KB RTC budget.

---

## 15. Provisioning Portal Input UX — Coordinate Auto-Fill

### Current State
Users must manually enter decimal latitude and longitude in the provisioning portal — values
that most people do not know off the top of their head.

### Fix
Add a browser-based geolocation button to the provisioning portal HTML:
```html
<button type="button" onclick="getLocation()">📍 Use My Location</button>
<script>
function getLocation() {
    navigator.geolocation.getCurrentPosition(function(pos) {
        document.getElementById('lat').value = pos.coords.latitude.toFixed(4);
        document.getElementById('lon').value = pos.coords.longitude.toFixed(4);
    });
}
</script>
```
This works on modern mobile browsers (including iOS Safari and Android Chrome) when the
user grants location permission. Since the portal is served over the device's own SoftAP
(`192.168.4.1`), HTTPS is not required for the Geolocation API in most browsers when
connecting to a local IP (not enforced on LAN origins).

---

## 16. Provisioning SoftAP — Open Network Security Risk

### Current State
`WiFiManager::startAP()` opens an **unprotected** (open) SoftAP by default when no
password is provided:
```cpp
bool WiFiManager::startAP(const String& ssid, const String& password) {
    WiFi.mode(WIFI_AP_STA);
    if (password.isEmpty()) {
        ok = WiFi.softAP(ssid.c_str());  // open network
    }
```
Any device within WiFi range can join the provisioning AP, access `192.168.4.1`, and submit
a new configuration — including overwriting a PIN-protected device if they get the right
current PIN (or brute-force it during the 60-second lockout window).

### Fix
Generate a random 8-character WPA2 password at first boot (stored in RTC memory during the
provisioning session) and display it on the e-ink screen alongside the AP name:

```cpp
// Generate a session-only AP password from RNG
char apPass[9];
snprintf(apPass, sizeof(apPass), "%08X", (unsigned)esp_random());
WiFiManager::getInstance().startAP("M5Paper-Setup", apPass);
DisplayManager::getInstance().showProvisioningScreen("M5Paper-Setup", apPass);
```

The user sees the 8-character code on the screen and enters it to join the AP. After
provisioning is complete and the device reboots, the AP is shut down and the password
is discarded.

---

## 17. Structured Crash / Error Telemetry

### Current State
Errors are tracked via `rtcLastError` (a single `uint8_t` AppError code) and the Settings
page shows a badge icon. There is no error history, no crash log, and no mechanism to
report persistent failures.

### Fix
Store a small error ring in RTC memory:
```cpp
struct ErrorEntry {
    uint8_t code;    // AppError value
    time_t  time;    // when the error occurred
};
RTC_DATA_ATTR ErrorEntry rtcErrorHistory[8];
RTC_DATA_ATTR uint8_t    rtcErrorHead = 0;
```

On the Settings page, display the last 8 errors with timestamps instead of a single
current-error badge. This makes intermittent failures (e.g., NTP timeouts that only occur
at 3 AM) visible to the user without requiring serial monitor access.

Additionally, the `/status` endpoint on the provisioning web server (currently returns
`{"status":"provisioning"}`) could be extended to return the error history as JSON —
useful for remote debugging.

---

## 18. Summary — New Improvement Areas

| # | Area | Type | Priority | Effort |
|---|------|------|----------|--------|
| 1 | TLS certificate verification | Security | Critical | M |
| 2 | OTA firmware updates | Feature | High | L |
| 3 | Imperial / metric unit toggle | Feature | High | S |
| 4 | Lat/lon + API key input validation | Bug/Security | High | XS |
| 5 | PIN salt (rainbow table hardening) | Security | High | S |
| 6 | API rate-limit (HTTP 429) backoff | Reliability | High | S |
| 7 | Disable verbose logging in production | Performance | Medium | XS |
| 8 | Webhook POST with weather JSON payload | Feature | Medium | S |
| 9 | RTC memory budget assertion | Reliability | Medium | XS |
| 10 | Factory reset via Settings UI | UX | Medium | S |
| 11 | Config backup / restore via portal | UX | Medium | M |
| 12 | MQTT / Home Assistant integration | Feature | Medium | L |
| 13 | Timezone auto-detection from API response | UX | Medium | S |
| 14 | Historical pressure + battery charts | Feature | Low | M |
| 15 | Coordinate auto-fill via browser geolocation | UX | Low | XS |
| 16 | Provisioning AP WPA2 password | Security | High | XS |
| 17 | Structured error history in RTC + portal | Reliability | Low | S |

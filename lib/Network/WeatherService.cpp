#include "WeatherService.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

static const char* TAG = "WeatherService";

// Google Weather API endpoints (v1)
static constexpr const char* kCurrentUrl =
    "https://weather.googleapis.com/v1/currentConditions:lookup";
static constexpr const char* kForecastUrl =
    "https://weather.googleapis.com/v1/forecast/days:lookup";

static constexpr EventBits_t kBitGoogle    = BIT0; ///< Set by _fetchGoogleTask on completion.
static constexpr EventBits_t kBitOpenMeteo = BIT1; ///< Set by _fetchOpenMeteoTask on completion.

/**
 * @brief Shared context passed by pointer to both FreeRTOS fetch tasks.
 *
 * fetch() blocks on fetchDone until both bits are set (or 30 s timeout).
 * Tasks write to non-overlapping WeatherData fields, so no mutex is needed.
 */
struct FetchContext {
    const String*      lat;
    const String*      lon;
    const String*      apiKey;
    WeatherData*       data;
    EventGroupHandle_t fetchDone;
    bool               googleOk; ///< Set by _fetchGoogleTask when fetch #1 succeeds.
};

WeatherService& WeatherService::getInstance() {
    static WeatherService instance;
    return instance;
}

// ── Task A: googleapis.com — fetches #1 (current), #2 (forecast), #5 (alerts) ─

static void _fetchGoogleTask(void* arg) {
    FetchContext* ctx    = static_cast<FetchContext*>(arg);
    WeatherData*  data   = ctx->data;
    const String& lat    = *ctx->lat;
    const String& lon    = *ctx->lon;
    const String& apiKey = *ctx->apiKey;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    // --- 1. Current Conditions ---
    String currentUrl = String(kCurrentUrl)
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon;
    http.begin(client, currentUrl);
    http.addHeader("Accept", "application/json");
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            const char* condText = doc["weatherCondition"]["description"]["text"] | "";
            strncpy(data->condition, condText, sizeof(data->condition) - 1);
            data->condition[sizeof(data->condition) - 1] = '\0';
            data->isDaytime    = doc["isDaytime"].as<bool>();
            data->humidity     = doc["relativeHumidity"].as<int>();
            data->uvIndex      = doc["uvIndex"].as<int>();
            data->cloudCover   = doc["cloudCover"].as<int>();
            data->visibilityKm = doc["visibility"]["distance"].as<float>();
            data->tempC        = doc["temperature"]["degrees"].as<float>();
            data->feelsLikeC   = doc["feelsLikeTemperature"]["degrees"].as<float>();
            data->windKph      = doc["wind"]["speed"]["value"].as<float>();
            data->windDirDeg   = doc["wind"]["direction"]["degrees"].as<int>();
            ctx->googleOk = true;
        } else {
            ESP_LOGE(TAG, "JSON parse error (current)");
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET current conditions failed: %d", code);
    }
    http.end();

    // --- 2. 10-Day Forecast (same host — fresh socket via http.begin) ---
    String forecastUrl = String(kForecastUrl)
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon
        + "&days=10&pageSize=10";
    http.begin(client, forecastUrl);
    http.addHeader("Accept", "application/json");
    code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument fcDoc;
        if (!deserializeJson(fcDoc, payload)) {
            JsonArray days = fcDoc["forecastDays"].as<JsonArray>();
            int i = 0;
            for (JsonVariant day : days) {
                if (i >= 10) break;
                JsonObject daytime = day["daytimeForecast"];
                const char* dayCond = daytime["weatherCondition"]["description"]["text"] | "";
                strncpy(data->forecast[i].condition, dayCond, sizeof(data->forecast[i].condition) - 1);
                data->forecast[i].condition[sizeof(data->forecast[i].condition) - 1] = '\0';
                data->forecast[i].minTempC    = day["minTemperature"]["degrees"].as<float>();
                data->forecast[i].maxTempC    = day["maxTemperature"]["degrees"].as<float>();
                data->forecast[i].precipChance = daytime["precipitation"]["probability"]["percent"].as<int>();
                // Parse dayTime from interval.startTime (RFC 3339: "2026-03-07T13:00:00Z")
                const char* startTimeStr = day["interval"]["startTime"] | "";
                if (startTimeStr[0] != '\0') {
                    struct tm dayTm = {};
                    if (strptime(startTimeStr, "%Y-%m-%dT%H:%M:%SZ", &dayTm) != nullptr) {
                        dayTm.tm_isdst = -1;
                        data->forecast[i].dayTime = mktime(&dayTm);
                    }
                }
                data->forecastDays++;
                i++;
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse forecast JSON");
        }
    } else {
        ESP_LOGW(TAG, "HTTP GET forecast failed: %d", code);
    }
    http.end();

    // --- 5. Weather Alerts (same host — fresh socket via http.begin) ---
    String alertUrl = String("https://weather.googleapis.com/v1/weatherAlerts:lookup")
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon;
    http.begin(client, alertUrl);
    http.addHeader("Accept", "application/json");
    code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument alertDoc;
        if (!deserializeJson(alertDoc, payload)) {
            JsonArray alerts = alertDoc["weatherAlerts"].as<JsonArray>();
            if (alerts.size() > 0) {
                JsonObject first    = alerts[0];
                const char* headline = first["headline"]["text"] | "";
                const char* severity = first["certainty"] | "";
                strncpy(data->alertHeadline, headline, sizeof(data->alertHeadline) - 1);
                data->alertHeadline[sizeof(data->alertHeadline) - 1] = '\0';
                strncpy(data->alertSeverity, severity, sizeof(data->alertSeverity) - 1);
                data->alertSeverity[sizeof(data->alertSeverity) - 1] = '\0';
                data->hasAlert = (data->alertHeadline[0] != '\0');
                if (data->hasAlert) {
                    ESP_LOGW(TAG, "Active alert: %s [%s]", data->alertHeadline, data->alertSeverity);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse alerts JSON");
        }
    } else {
        // 404 = no alerts endpoint for this region — acceptable
        ESP_LOGI(TAG, "Weather alerts HTTP %d (no alerts or unsupported region)", code);
    }
    http.end();

    xEventGroupSetBits(ctx->fetchDone, kBitGoogle);
    vTaskDelete(nullptr);
}

// ── Task B: open-meteo.com — fetches #3 (AQI/pollen), #4 (sun/hourly) ────────

static void _fetchOpenMeteoTask(void* arg) {
    FetchContext* ctx  = static_cast<FetchContext*>(arg);
    WeatherData*  data = ctx->data;
    const String& lat  = *ctx->lat;
    const String& lon  = *ctx->lon;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    // --- 3. AQI + Pollen (air-quality-api.open-meteo.com) ---
    // Note: open-meteo uses "ragweed_pollen" for weed pollen — "weed_pollen" returns HTTP 400.
    String aqiUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + lat
        + "&longitude=" + lon
        + "&current=us_aqi"
        + "&hourly=grass_pollen,birch_pollen,ragweed_pollen"
        + "&timeformat=unixtime&timezone=auto&forecast_days=1";
    http.begin(client, aqiUrl);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument aqiDoc;
        if (!deserializeJson(aqiDoc, payload)) {
            data->aqi = aqiDoc["current"]["us_aqi"].as<int>();
            // Take the peak pollen value over the next 8 hours from now.
            JsonArray pollenTimes    = aqiDoc["hourly"]["time"].as<JsonArray>();
            JsonArray pollenGrassArr = aqiDoc["hourly"]["grass_pollen"].as<JsonArray>();
            JsonArray pollenBirchArr = aqiDoc["hourly"]["birch_pollen"].as<JsonArray>();
            JsonArray pollenWeedArr  = aqiDoc["hourly"]["ragweed_pollen"].as<JsonArray>();
            time_t currentTime = time(nullptr);
            int peakGrass = 0, peakBirch = 0, peakWeed = 0, hoursScanned = 0;
            for (size_t pidx = 0; pidx < pollenTimes.size() && hoursScanned < 8; pidx++) {
                time_t pt = pollenTimes[pidx].as<time_t>();
                if (pt >= currentTime - 1800) {
                    peakGrass = std::max(peakGrass, pollenGrassArr[pidx].as<int>());
                    peakBirch = std::max(peakBirch, pollenBirchArr[pidx].as<int>());
                    peakWeed  = std::max(peakWeed,  pollenWeedArr[pidx].as<int>());
                    hoursScanned++;
                }
            }
            data->pollenGrass = peakGrass;
            data->pollenBirch = peakBirch;
            data->pollenWeed  = peakWeed;
        } else {
            ESP_LOGW(TAG, "Failed to parse AQI JSON");
        }
    } else {
        String errBody = http.getString();
        ESP_LOGW(TAG, "HTTP GET AQI failed: %d — %s", code, errBody.substring(0, 120).c_str());
    }
    http.end();

    // --- 4. Sun Times + Hourly Forecast (api.open-meteo.com) ---
    String sunUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + lat
        + "&longitude=" + lon
        + "&daily=sunrise,sunset"
        + "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,wind_gusts_10m,surface_pressure"
        + "&timeformat=unixtime&timezone=auto&forecast_days=2";
    client.stop(); // switch subdomain: air-quality-api → api.open-meteo.com
    http.begin(client, sunUrl);
    code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument sunDoc;
        if (!deserializeJson(sunDoc, payload)) {
            data->sunriseTime = sunDoc["daily"]["sunrise"][0].as<time_t>();
            data->sunsetTime  = sunDoc["daily"]["sunset"][0].as<time_t>();
            JsonArray hourlyTimes        = sunDoc["hourly"]["time"].as<JsonArray>();
            JsonArray hourlyTemps        = sunDoc["hourly"]["temperature_2m"].as<JsonArray>();
            JsonArray hourlyWeatherCodes = sunDoc["hourly"]["weather_code"].as<JsonArray>();
            JsonArray hourlyPrecip       = sunDoc["hourly"]["precipitation_probability"].as<JsonArray>();
            JsonArray hourlyWind         = sunDoc["hourly"]["wind_speed_10m"].as<JsonArray>();
            JsonArray hourlyGusts        = sunDoc["hourly"]["wind_gusts_10m"].as<JsonArray>();
            JsonArray hourlyPressure     = sunDoc["hourly"]["surface_pressure"].as<JsonArray>();
            time_t currentTime = time(nullptr);
            data->hourlyCount = 0;
            for (size_t i = 0; i < hourlyTimes.size() && data->hourlyCount < 24; i++) {
                time_t hourTime = hourlyTimes[i].as<time_t>();
                if (hourTime > currentTime - 1800) {
                    data->hourly[data->hourlyCount].timestamp   = hourTime;
                    data->hourly[data->hourlyCount].tempC        = hourlyTemps[i].as<float>();
                    data->hourly[data->hourlyCount].weatherCode  = hourlyWeatherCodes[i].as<int>();
                    data->hourly[data->hourlyCount].precipChance = hourlyPrecip[i].as<int>();
                    data->hourly[data->hourlyCount].windKph      = hourlyWind[i].as<float>();
                    data->hourly[data->hourlyCount].windGustKph  = hourlyGusts[i].as<float>();
                    data->hourly[data->hourlyCount].pressureHpa  = hourlyPressure[i].as<float>();
                    data->hourlyCount++;
                }
            }
            // Populate current-conditions scalars from the first valid hourly entry.
            if (data->hourlyCount > 0) {
                data->windGustKph = data->hourly[0].windGustKph;
                data->pressureHpa = data->hourly[0].pressureHpa;
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse Sun JSON");
        }
    } else {
        ESP_LOGW(TAG, "HTTP GET Sun failed: %d", code);
    }
    http.end();

    xEventGroupSetBits(ctx->fetchDone, kBitOpenMeteo);
    vTaskDelete(nullptr);
}

// ── Public API ────────────────────────────────────────────────────────────────

WeatherData WeatherService::fetch(const String& lat, const String& lon,
                                  const String& apiKey) {
    WeatherData data = {};

    if (apiKey.isEmpty() || lat.isEmpty() || lon.isEmpty()) {
        ESP_LOGW(TAG, "Missing API key or coordinates");
        return data;
    }

    ESP_LOGW(TAG, "TLS certificate verification DISABLED — MITM injection of weather data is possible");

    FetchContext ctx = { &lat, &lon, &apiKey, &data, xEventGroupCreate(), false };

    // Launch both fetch tasks concurrently. Task A handles all googleapis.com
    // requests (#1 current, #2 forecast, #5 alerts); Task B handles both
    // open-meteo.com requests (#3 AQI/pollen, #4 sun/hourly). Fields written
    // by each task are non-overlapping, so no mutex is required.
    xTaskCreate(_fetchGoogleTask,    "fetchGoogle",    8192, &ctx, 5, nullptr);
    xTaskCreate(_fetchOpenMeteoTask, "fetchOpenMeteo", 8192, &ctx, 5, nullptr);

    // Block until both tasks finish (or 30-second hard timeout).
    xEventGroupWaitBits(ctx.fetchDone, kBitGoogle | kBitOpenMeteo,
                        pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));
    vEventGroupDelete(ctx.fetchDone);

    if (ctx.googleOk) {
        data.fetchTime = time(nullptr);
        data.valid     = true;
        ESP_LOGI(TAG, "Weather: %s  %.1f°C  hum=%d%%  aqi=%d  sunrise=%ld  alert=%d",
                 data.condition, data.tempC, data.humidity, data.aqi, (long)data.sunriseTime,
                 (int)data.hasAlert);
    } else {
        ESP_LOGW(TAG, "Current conditions fetch failed; supplemental data fetched but data.valid remains false");
    }
    return data;
}

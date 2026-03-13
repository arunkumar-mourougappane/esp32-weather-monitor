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



WeatherService& WeatherService::getInstance() {
    static WeatherService instance;
    return instance;
}

WeatherData WeatherService::fetch(const String& lat, const String& lon,
                                  const String& apiKey) {
    WeatherData data = {};

    if (apiKey.isEmpty() || lat.isEmpty() || lon.isEmpty()) {
        ESP_LOGW(TAG, "Missing API key or coordinates");
        return data;
    }


    // --- 1. Fetch Current Conditions ---
    String currentUrl = String(kCurrentUrl)
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon;

    WiFiClientSecure client;
    client.setInsecure(); // NOTE: skips TLS cert verification – acceptable for IoT

    HTTPClient http;
    http.setReuse(true); // Enable Keep-Alive to reuse the TLS handshake for subsequent calls
    http.begin(client, currentUrl);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ESP_LOGE(TAG, "HTTP GET current conditions failed: %d", code);
        http.end();
        return data;
    }

    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        ESP_LOGE(TAG, "JSON parse error (current): %s", err.c_str());
        http.end();
        return data;
    }

    // ----- Map current conditions fields -----
    const char* condText = doc["weatherCondition"]["description"]["text"] | "";
    strncpy(data.condition, condText, sizeof(data.condition) - 1);
    data.condition[sizeof(data.condition) - 1] = '\0';

    data.isDaytime    = doc["isDaytime"].as<bool>();
    data.humidity     = doc["relativeHumidity"].as<int>();
    data.uvIndex      = doc["uvIndex"].as<int>();
    data.cloudCover   = doc["cloudCover"].as<int>();
    data.visibilityKm = doc["visibility"]["distance"].as<float>();

    // Temperature
    data.tempC        = doc["temperature"]["degrees"].as<float>();
    data.feelsLikeC   = doc["feelsLikeTemperature"]["degrees"].as<float>();

    // Wind
    data.windKph      = doc["wind"]["speed"]["value"].as<float>();
    data.windDirDeg   = doc["wind"]["direction"]["degrees"].as<int>();


    http.end();

    // --- 2. Fetch 10-Day Forecast ---
    String forecastUrl = String(kForecastUrl)
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon
        + "&days=10"
        + "&pageSize=10";
        
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
                
                // Usually take daytime forecast for logic
                JsonObject daytime = day["daytimeForecast"];
                
                const char* dayCond = daytime["weatherCondition"]["description"]["text"] | "";
                strncpy(data.forecast[i].condition, dayCond, sizeof(data.forecast[i].condition) - 1);
                data.forecast[i].condition[sizeof(data.forecast[i].condition) - 1] = '\0';
                
                data.forecast[i].minTempC = day["minTemperature"]["degrees"].as<float>();
                data.forecast[i].maxTempC = day["maxTemperature"]["degrees"].as<float>();
                data.forecast[i].precipChance = daytime["precipitation"]["probability"]["percent"].as<int>();
                
                // Very basic fallback to extract the day from the startTime String, 
                // e.g., "2026-03-07T13:00:00Z" (We don't actually need full time_t, just indexing 0-4 is fine)
                data.forecastDays++;
                i++;
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse forecast JSON");
        }
    } else {
        ESP_LOGW(TAG, "HTTP GET forecast failed: %d", code);
    }
    http.end();

    // --- 3. Fetch Supplemental AQI ---
    String aqiUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + lat + "&longitude=" + lon + "&current=us_aqi";
    http.begin(client, aqiUrl);
    code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument aqiDoc;
        if (!deserializeJson(aqiDoc, payload)) {
            data.aqi = aqiDoc["current"]["us_aqi"].as<int>();
        } else {
            ESP_LOGW(TAG, "Failed to parse AQI JSON");
        }
    } else {
        ESP_LOGW(TAG, "HTTP GET AQI failed: %d", code);
    }
    http.end();

    // --- 4. Fetch Supplemental Sun Times & Hourly Forecast ---
    String sunUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon 
        + "&daily=sunrise,sunset&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m"
        + "&timeformat=unixtime&timezone=auto&forecast_days=2";
    http.begin(client, sunUrl);
    code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument sunDoc;
        if (!deserializeJson(sunDoc, payload)) {
            data.sunriseTime = sunDoc["daily"]["sunrise"][0].as<time_t>();
            data.sunsetTime  = sunDoc["daily"]["sunset"][0].as<time_t>();

            JsonArray hourlyTimes = sunDoc["hourly"]["time"].as<JsonArray>();
            JsonArray hourlyTemps = sunDoc["hourly"]["temperature_2m"].as<JsonArray>();
            JsonArray hourlyWeatherCodes = sunDoc["hourly"]["weather_code"].as<JsonArray>();
            JsonArray hourlyPrecip = sunDoc["hourly"]["precipitation_probability"].as<JsonArray>();
            JsonArray hourlyWind = sunDoc["hourly"]["wind_speed_10m"].as<JsonArray>();
            
            time_t currentTime = time(nullptr);
            data.hourlyCount = 0;
            
            for (size_t i = 0; i < hourlyTimes.size() && data.hourlyCount < 24; i++) {
                time_t hourTime = hourlyTimes[i].as<time_t>();
                if (hourTime > currentTime - 1800) { // allow up to 30 mins in past for current hour
                    data.hourly[data.hourlyCount].timestamp = hourTime;
                    data.hourly[data.hourlyCount].tempC = hourlyTemps[i].as<float>();
                    data.hourly[data.hourlyCount].weatherCode = hourlyWeatherCodes[i].as<int>();
                    data.hourly[data.hourlyCount].precipChance = hourlyPrecip[i].as<int>();
                    data.hourly[data.hourlyCount].windKph = hourlyWind[i].as<float>();
                    data.hourlyCount++;
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse Sun JSON");
        }
    } else {
        ESP_LOGW(TAG, "HTTP GET Sun failed: %d", code);
    }
    http.end();

    // --- 5. Fetch Active Weather Alerts ---
    // Uses the Google Weather API v1 alerts endpoint. Non-fatal: failure leaves hasAlert=false.
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
                // Pick the first (highest-priority) alert
                JsonObject first = alerts[0];
                const char* headline = first["headline"]["text"] | "";
                const char* severity = first["certainty"] | "";
                strncpy(data.alertHeadline, headline, sizeof(data.alertHeadline) - 1);
                data.alertHeadline[sizeof(data.alertHeadline) - 1] = '\0';
                strncpy(data.alertSeverity, severity, sizeof(data.alertSeverity) - 1);
                data.alertSeverity[sizeof(data.alertSeverity) - 1] = '\0';
                data.hasAlert = (data.alertHeadline[0] != '\0');
                if (data.hasAlert) {
                    ESP_LOGW(TAG, "Active alert: %s [%s]", data.alertHeadline, data.alertSeverity);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse alerts JSON");
        }
    } else {
        // 404 means no alerts endpoint for this region — that is acceptable
        ESP_LOGI(TAG, "Weather alerts HTTP %d (no alerts or unsupported region)", code);
    }
    http.end();

    data.fetchTime = time(nullptr);
    data.valid     = true;
    ESP_LOGI(TAG, "Weather: %s  %.1f°C  hum=%d%%  aqi=%d  sunrise=%ld  alert=%d",
             data.condition, data.tempC, data.humidity, data.aqi, (long)data.sunriseTime,
             (int)data.hasAlert);
    return data;
}

#include "WeatherService.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_log.h>

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
    WeatherData data;

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
    http.begin(client, currentUrl);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ESP_LOGE(TAG, "HTTP GET current conditions failed: %d – %s",
                 code, http.errorToString(code).c_str());
        http.end();
        return data;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        ESP_LOGE(TAG, "JSON parse error (current): %s", err.c_str());
        return data;
    }

    // ----- Map current conditions fields -----
    data.condition    = doc["weatherCondition"]["description"]["text"].as<String>();
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
        String fcPayload = http.getString();
        // ESP_LOGI(TAG, "Forecast payload len: %d", fcPayload.length());
        JsonDocument fcDoc;
        if (!deserializeJson(fcDoc, fcPayload)) {
            JsonArray days = fcDoc["forecastDays"].as<JsonArray>();
            int i = 0;
            for (JsonVariant day : days) {
                if (i >= 10) break;
                
                // Usually take daytime forecast for logic
                JsonObject daytime = day["daytimeForecast"];
                
                data.forecast[i].condition = daytime["weatherCondition"]["description"]["text"].as<String>();
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


    data.fetchTime = time(nullptr);
    data.valid     = true;
    ESP_LOGI(TAG, "Weather: %s  %.1f°C  hum=%d%%  forecastDays=%d",
             data.condition.c_str(), data.tempC, data.humidity, data.forecastDays);
    return data;
}

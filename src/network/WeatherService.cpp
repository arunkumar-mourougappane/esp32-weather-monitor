#include "WeatherService.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "WeatherService";

// Google Weather API endpoint (v1 – currentConditions)
// Full URL: https://weather.googleapis.com/v1/currentConditions:lookup
//           ?key=<KEY>&location.latitude=<LAT>&location.longitude=<LON>
static constexpr const char* kBaseUrl =
    "https://weather.googleapis.com/v1/currentConditions:lookup";

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

    String url = String(kBaseUrl)
        + "?key=" + apiKey
        + "&location.latitude=" + lat
        + "&location.longitude=" + lon;

    WiFiClientSecure client;
    client.setInsecure(); // NOTE: skips TLS cert verification – acceptable for IoT

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %d – %s",
                 code, http.errorToString(code).c_str());
        http.end();
        return data;
    }

    String payload = http.getString();
    ESP_LOGI(TAG, "Raw payload: %s", payload.c_str());

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    http.end();

    if (err) {
        ESP_LOGE(TAG, "JSON parse error: %s", err.c_str());
        return data;
    }

    // ----- Map Google Weather API response fields -----
    data.condition   = doc["weatherCondition"]["description"]["text"]
                         .as<String>();
    data.isDaytime   = doc["isDaytime"].as<bool>();
    data.humidity    = doc["relativeHumidity"].as<int>();
    data.uvIndex     = doc["uvIndex"].as<int>();

    // Temperature (API returns in the unit configured per project; assume °C)
    data.tempC       = doc["temperature"]["degrees"].as<float>();
    data.feelsLikeC  = doc["feelsLikeTemperature"]["degrees"].as<float>();

    // Wind
    data.windKph     = doc["wind"]["speed"]["value"].as<float>();
    data.windDirDeg  = doc["wind"]["direction"]["degrees"].as<int>();

    data.fetchTime   = time(nullptr);
    data.valid       = true;
    ESP_LOGI(TAG, "Weather: %s  %.1f°C  hum=%d%%",
             data.condition.c_str(), data.tempC, data.humidity);
    return data;
}

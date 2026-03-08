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

static EventGroupHandle_t fetchEvents;
#define BIT_AQI_DONE BIT0
#define BIT_SUN_DONE BIT1

struct OpenMeteoArgs {
    String lat;
    String lon;
    WeatherData* data;
};

void fetchAqiTask(void* pvParameters) {
    OpenMeteoArgs* args = (OpenMeteoArgs*)pvParameters;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String aqiUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + args->lat + "&longitude=" + args->lon + "&current=us_aqi";
    http.begin(client, aqiUrl);
    if (http.GET() == HTTP_CODE_OK) {
        JsonDocument aqiDoc;
        if (!deserializeJson(aqiDoc, http.getStream())) {
            args->data->aqi = aqiDoc["current"]["us_aqi"].as<int>();
        } else {
            ESP_LOGW("AQI_TASK", "Failed to parse AQI JSON");
        }
    } else {
        ESP_LOGW("AQI_TASK", "HTTP GET AQI failed");
    }
    http.end();
    xEventGroupSetBits(fetchEvents, BIT_AQI_DONE);
    vTaskDelete(NULL);
}

void fetchSunTask(void* pvParameters) {
    OpenMeteoArgs* args = (OpenMeteoArgs*)pvParameters;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String sunUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + args->lat + "&longitude=" + args->lon + "&daily=sunrise,sunset&timezone=auto&forecast_days=1";
    http.begin(client, sunUrl);
    if (http.GET() == HTTP_CODE_OK) {
        JsonDocument sunDoc;
        if (!deserializeJson(sunDoc, http.getStream())) {
            String sunriseStr = sunDoc["daily"]["sunrise"][0].as<String>();
            String sunsetStr = sunDoc["daily"]["sunset"][0].as<String>();
            struct tm tms = {0};
            if (strptime(sunriseStr.c_str(), "%Y-%m-%dT%H:%M", &tms) != nullptr) {
                tms.tm_isdst = -1;
                args->data->sunriseTime = mktime(&tms);
            }
            if (strptime(sunsetStr.c_str(), "%Y-%m-%dT%H:%M", &tms) != nullptr) {
                tms.tm_isdst = -1;
                args->data->sunsetTime = mktime(&tms);
            }
        } else {
            ESP_LOGW("SUN_TASK", "Failed to parse Sun JSON");
        }
    } else {
        ESP_LOGW("SUN_TASK", "HTTP GET Sun failed");
    }
    http.end();
    xEventGroupSetBits(fetchEvents, BIT_SUN_DONE);
    vTaskDelete(NULL);
}

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

    // Prepare Thread Synchonization & Memory
    fetchEvents = xEventGroupCreate();
    OpenMeteoArgs* omArgs = new OpenMeteoArgs{lat, lon, &data};
    
    // Spawn Asynchronous Fetch Threads pinning them away from APP_Core_1 so they compute in raw parallel
    xTaskCreatePinnedToCore(fetchAqiTask, "AQI_FETCHER", 10240, omArgs, 5, NULL, 0);
    xTaskCreatePinnedToCore(fetchSunTask, "SUN_FETCHER", 10240, omArgs, 5, NULL, 0);

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

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream()); // Parse directly off the TCP stream!

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
        JsonDocument fcDoc;
        if (!deserializeJson(fcDoc, http.getStream())) {
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

    // Block the master APP_Core until the 2 PRO_Core Open-Meteo threads complete their TLS payloads
    // Sets a harsh 5-second failure timeout to prevent infinite sync hanging if Open-Meteo dies.
    EventBits_t uxBits = xEventGroupWaitBits(fetchEvents, BIT_AQI_DONE | BIT_SUN_DONE, pdTRUE, pdTRUE, 5000 / portTICK_PERIOD_MS);
    
    if ((uxBits & (BIT_AQI_DONE | BIT_SUN_DONE)) != (BIT_AQI_DONE | BIT_SUN_DONE)) {
        ESP_LOGE(TAG, "FreeRTOS thread synchronization timeout! Background APIs hung.");
    }
    
    vEventGroupDelete(fetchEvents);
    delete omArgs;

    data.fetchTime = time(nullptr);
    data.valid     = true;
    ESP_LOGI(TAG, "Weather: %s  %.1f°C  hum=%d%%  aqi=%d  sunrise=%ld",
             data.condition, data.tempC, data.humidity, data.aqi, (long)data.sunriseTime);
    return data;
}

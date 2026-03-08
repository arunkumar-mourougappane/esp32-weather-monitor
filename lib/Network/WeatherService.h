/**
 * @file WeatherService.h
 * @brief Google Weather API v1 client for current conditions and 10-day forecast.
 *
 * WeatherService performs two HTTPS GET requests per refresh cycle:
 *  1. `currentConditions:lookup` – real-time ambient conditions.
 *  2. `forecast/days:lookup`     – 10-day daily forecast (pageSize=10 bypasses
 *     the default 5-day pagination limit).
 *
 * The response JSON is parsed with ArduinoJson v7 and flattened into a
 * WeatherData struct that the rest of the application can consume safely.
 */
#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H
#include <Arduino.h>

/**
 * @struct DailyForecast
 * @brief Compact summary of weather conditions for a single forecast day.
 */
struct DailyForecast {
    char   condition[32]; ///< Human-readable sky condition (e.g. "Partly cloudy").
    float  minTempC;     ///< Daytime low temperature in degrees Celsius.
    float  maxTempC;     ///< Daytime high temperature in degrees Celsius.
    int    precipChance; ///< Precipitation probability in percent (0–100).
    time_t dayTime;      ///< Unix timestamp for midnight (UTC) of this forecast day.
};

/**
 * @struct WeatherData
 * @brief Aggregate result returned by WeatherService::fetch().
 *
 * Contains both real-time current conditions and the full 10-day daily
 * forecast array.  Always check @c valid before consuming any fields.
 */
struct WeatherData {
    char   condition[64];       ///< Current sky condition description.
    float  tempC         = 0.0f; ///< Ambient temperature in degrees Celsius.
    float  feelsLikeC    = 0.0f; ///< "Feels like" temperature in degrees Celsius.
    int    humidity      = 0;    ///< Relative humidity (%).
    float  windKph       = 0.0f; ///< Wind speed in km/h.
    int    windDirDeg    = 0;    ///< Wind direction in degrees (0° = north).
    int    uvIndex       = 0;    ///< UV index (0–11+).
    int    cloudCover    = 0;    ///< Cloud cover percentage.
    float  visibilityKm  = 0.0f; ///< Horizontal visibility in kilometres.
    bool   isDaytime     = true;  ///< @c true between sunrise and sunset.
    bool   valid         = false; ///< @c true only when the last fetch succeeded.
    time_t fetchTime     = 0;    ///< Unix timestamp of the last successful fetch.

    DailyForecast forecast[10];  ///< Up to 10 days of daily forecast data.
    int forecastDays = 0;        ///< Number of valid entries in @c forecast[].
};

/**
 * @class WeatherService
 * @brief Singleton HTTP client that fetches weather from Google Weather API v1.
 *
 * Connections are made over TLS using NetworkClientSecure (root CA bundle
 * bundled with the ESP32 Arduino framework).
 *
 * Example:
 * @code
 *   WeatherData wd = WeatherService::getInstance().fetch(lat, lon, apiKey);
 *   if (wd.valid) {
 *       Serial.println(wd.tempC);
 *   }
 * @endcode
 */
class WeatherService {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single WeatherService object.
     */
    static WeatherService& getInstance();

    /**
     * @brief Fetch current conditions and 10-day forecast from the Google Weather API.
     *
     * Makes two sequential HTTPS requests:
     *  - `currentConditions:lookup` for real-time data.
     *  - `forecast/days:lookup?days=10&pageSize=10` for the 10-day forecast.
     *
     * @param lat     Decimal latitude string (e.g. @c "41.6611").
     * @param lon     Decimal longitude string (e.g. @c "-89.5000").
     * @param apiKey  Google Cloud API key with the Weather API enabled.
     * @return        Populated WeatherData on success (@c valid==true),
     *                or a zeroed struct with @c valid==false on any HTTP or
     *                JSON parsing error.
     */
    WeatherData fetch(const String& lat, const String& lon,
                      const String& apiKey);

private:
    WeatherService() = default;
};

#endif // WEATHER_SERVICE_H

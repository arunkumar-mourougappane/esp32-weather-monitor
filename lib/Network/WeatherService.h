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
    float  tempC;               ///< Ambient temperature in degrees Celsius.
    float  feelsLikeC;          ///< "Feels like" temperature in degrees Celsius.
    int    humidity;            ///< Relative humidity (%).
    float  windKph;             ///< Wind speed in km/h.
    int    windDirDeg;          ///< Wind direction in degrees (0° = north).
    int    uvIndex;             ///< UV index (0–11+).
    int    cloudCover;          ///< Cloud cover percentage.
    float  visibilityKm;        ///< Horizontal visibility in kilometres.
    bool   isDaytime;           ///< @c true between sunrise and sunset.
    int    aqi;                 ///< Air Quality Index (US EPA).
    time_t sunriseTime;         ///< Unix timestamp for today's sunrise.
    time_t sunsetTime;          ///< Unix timestamp for today's sunset.
    bool   valid;               ///< @c true only when the last fetch succeeded.
    time_t fetchTime;           ///< Unix timestamp of the last successful fetch.

    DailyForecast forecast[10]; ///< Up to 10 days of daily forecast data.
    int forecastDays;           ///< Number of valid entries in @c forecast[].
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

#pragma once
#include <Arduino.h>

struct DailyForecast {
    String condition;
    float  minTempC;
    float  maxTempC;
    int    precipChance;
    time_t dayTime;
};

/// Represents a single weather observation returned by the Google Weather API.
struct WeatherData {
    String condition;       ///< Human-readable description (e.g. "Partly cloudy")
    float  tempC         = 0.0f;
    float  feelsLikeC    = 0.0f;
    int    humidity      = 0;   ///< Relative humidity %
    float  windKph       = 0.0f;
    int    windDirDeg    = 0;
    int    uvIndex       = 0;
    int    cloudCover    = 0;   ///< Cloud cover %
    float  visibilityKm  = 0.0f;///< Visibility in Km
    bool   isDaytime     = true;
    bool   valid         = false;
    time_t fetchTime     = 0;

    DailyForecast forecast[5];
    int forecastDays = 0;
};

/// Fetches current weather from the Google Weather API.
class WeatherService {
public:
    static WeatherService& getInstance();

    /// Fetch current conditions.
    ///
    /// Google Weather API endpoint (v1):
    ///   GET https://weather.googleapis.com/v1/currentConditions:lookup
    ///   Query: key=<API_KEY>&location.latitude=<LAT>&location.longitude=<LON>
    ///
    /// NOTE: Verify the exact response schema against Google's live docs;
    ///       field paths below match the early-2025 public preview spec.
    ///
    /// @return WeatherData with valid=true on success, valid=false on error.
    WeatherData fetch(const String& lat, const String& lon,
                      const String& apiKey);

private:
    WeatherService() = default;
};

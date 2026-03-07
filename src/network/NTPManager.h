#pragma once
#include <Arduino.h>
#include <time.h>

/// Manages NTP synchronisation and timezone-aware local time.
class NTPManager {
public:
    static NTPManager& getInstance();

    /// Configure SNTP and block until synced or timeout.
    /// @param ntpServer  URL or IP of the NTP server.
    /// @param timezone   POSIX TZ string (e.g. "CST6CDT,M3.2.0,M11.1.0").
    /// @return true if time was successfully obtained.
    bool sync(const String& ntpServer, const String& timezone,
              uint32_t timeoutMs = 15000);

    /// Fill @p timeinfo with the current local time.
    /// @return true if the RTC has been synced at least once.
    bool getLocalTime(struct tm& timeinfo) const;

    /// @return true after a successful sync().
    bool isSynced() const { return _synced; }

private:
    NTPManager() = default;
    bool _synced = false;
};

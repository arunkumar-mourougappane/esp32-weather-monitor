/**
 * @file NTPManager.h
 * @brief SNTP-based time synchronisation manager with POSIX timezone support.
 *
 * NTPManager wraps the ESP-IDF configTzTime / ESP SNTP subsystem.  In
 * addition to performing a standard NTP sync, it explicitly zeroes the system
 * clock before each sync attempt to prevent stale RTC data (left over from
 * prior deep-sleep cycles) from passing the "time already reasonable" check.
 *
 * DST transitions are handled natively by the C standard library's localtime_r
 * using the POSIX timezone string (e.g. @c "CST6CDT,M3.2.0,M11.1.0").
 */
#pragma once
#include <Arduino.h>
#include <time.h>

/**
 * @class NTPManager
 * @brief Singleton that synchronises the ESP32 system clock over NTP.
 *
 * Example:
 * @code
 *   bool ok = NTPManager::getInstance().sync("pool.ntp.org",
 *                                            "CST6CDT,M3.2.0,M11.1.0");
 *   if (ok) {
 *       struct tm t;
 *       NTPManager::getInstance().getLocalTime(t);
 *   }
 * @endcode
 */
class NTPManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single NTPManager object.
     */
    static NTPManager& getInstance();

    /**
     * @brief Configure SNTP and block until the clock is synchronised.
     *
     * Steps performed internally:
     *  1. Set system clock to the Unix epoch (0) to invalidate any stale RTC.
     *  2. Call `configTzTime(timezone, ntpServer)` to configure the SNTP
     *     daemon and register the POSIX timezone.
     *  3. Poll `esp_sntp_get_sync_status()` until
     *     `SNTP_SYNC_STATUS_COMPLETED` or @p timeoutMs elapses.
     *
     * @param ntpServer  Hostname or IP of the NTP server (e.g. @c "pool.ntp.org").
     * @param timezone   POSIX TZ string (e.g. @c "EST5EDT,M3.2.0,M11.1.0").
     * @param timeoutMs  Maximum time to wait for synchronisation (default 15 000 ms).
     * @return           @c true if synchronisation completed within the timeout.
     */
    bool sync(const String& ntpServer, const String& timezone,
              uint32_t timeoutMs = 15000);

    /**
     * @brief Populate a @c tm struct with the current local time.
     *
     * The struct is filled using `localtime_r`, honouring the POSIX timezone
     * set during sync().
     *
     * @param[out] timeinfo  Destination struct to fill.
     * @return               @c true if the clock has been synced at least once.
     */
    bool getLocalTime(struct tm& timeinfo) const;

    /**
     * @brief Quick check whether a successful sync has occurred.
     * @return @c true after at least one successful sync() call.
     */
    bool isSynced() const { return _synced; }

private:
    NTPManager() = default;

    bool _synced = false;      ///< Set to true after the first successful sync.
    long _utcOffsetSec = 0;    ///< UTC offset in seconds (informational, not used for TZ math).
};

/**
 * @file SystemEvents.h
 * @brief Typed event identifiers and payload structs for the EventBus.
 *
 * Every event that crosses a module boundary must be listed here.  Payload
 * ownership rules: the *publisher* owns the payload and it must remain valid
 * for the entire (synchronous) duration of EventBus::publish().
 */
#ifndef CORE_SYSTEM_EVENTS_H
#define CORE_SYSTEM_EVENTS_H
#include <Arduino.h>

// Forward declarations — include the relevant header before dereferencing.
struct WeatherData;

/**
 * @enum SystemEvent
 * @brief Enumeration of all application-wide events.
 *
 * _COUNT is a sentinel used to size the EventBus dispatch table.
 * It must always be the last entry.
 */
enum class SystemEvent : uint8_t {
    EV_WIFI_CONNECTED,        ///< WiFi association succeeded.      Payload: nullptr
    EV_WIFI_DISCONNECTED,     ///< WiFi link dropped.               Payload: nullptr
    EV_WEATHER_UPDATED,       ///< Fresh WeatherData available.      Payload: const WeatherData*
    EV_BATTERY_LOW,           ///< Battery below warning threshold.  Payload: nullptr
    EV_TOUCH_SWIPE_LEFT,      ///< Left-swipe gesture detected.      Payload: nullptr
    EV_TOUCH_SWIPE_RIGHT,     ///< Right-swipe gesture detected.     Payload: nullptr
    EV_TOUCH_SWIPE_UP,        ///< Up-swipe gesture detected.        Payload: nullptr
    EV_TOUCH_SWIPE_DOWN,      ///< Down-swipe gesture detected.      Payload: nullptr
    EV_PIN_PROMPT_REQUESTED,  ///< Module needs PIN from the user.   Payload: PinPromptPayload*
    _COUNT                    ///< Sentinel — always keep last.
};

/**
 * @struct PinPromptPayload
 * @brief Payload for EV_PIN_PROMPT_REQUESTED.
 *
 * The publisher fills @c message and leaves @c result empty.  The subscriber
 * (DisplayManager) calls promptPIN() synchronously and writes the entered PIN
 * string into @c result before returning.  Because EventBus dispatch is
 * synchronous, the publisher can read @c result immediately after publish().
 */
struct PinPromptPayload {
    const char* message; ///< Prompt text; must be valid during the publish() call.
    String      result;  ///< Filled by the DisplayManager subscriber.
};

#endif // CORE_SYSTEM_EVENTS_H

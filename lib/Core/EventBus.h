/**
 * @file EventBus.h
 * @brief Minimal synchronous publish/subscribe event bus.
 *
 * EventBus decouples modules by letting publishers fire events without
 * knowing which (if any) modules are listening.  All handlers execute
 * synchronously in registration order inside publish(), so no RTOS
 * primitives or extra stacks are required.
 *
 * **Thread safety:** subscribe() and reset() must be called from the
 * initialisation context before any tasks are running.  publish() may be
 * called from any single task, but concurrent publish() calls from multiple
 * tasks are not safe without external locking.
 *
 * Example:
 * @code
 *   // Subscriber (called once at startup):
 *   EventBus::subscribe(SystemEvent::EV_WEATHER_UPDATED, [](void* p) {
 *       auto* data = static_cast<const WeatherData*>(p);
 *       DisplayManager::getInstance().refresh(*data);
 *   });
 *
 *   // Publisher:
 *   EventBus::publish(SystemEvent::EV_WEATHER_UPDATED, &weatherData);
 * @endcode
 */
#ifndef CORE_EVENT_BUS_H
#define CORE_EVENT_BUS_H
#include <functional>
#include <array>
#include <vector>
#include "SystemEvents.h"

class EventBus {
public:
    /** @brief Callback type: receives the raw payload pointer. */
    using Handler = std::function<void(void*)>;

    /**
     * @brief Register a handler for a specific event.
     *
     * @param ev       Event type to subscribe to.
     * @param handler  Callable invoked with the payload pointer on each publish.
     *                 The handler is stored by value (captures are allowed).
     */
    static void subscribe(SystemEvent ev, Handler handler);

    /**
     * @brief Dispatch an event to all registered handlers synchronously.
     *
     * All handlers registered for @p ev are called in registration order.
     * publish() returns only after every handler has completed.
     *
     * @param ev       Event to dispatch.
     * @param payload  Optional pointer to event-specific data.  Cast to the
     *                 appropriate payload type documented in SystemEvents.h.
     *                 The payload must remain valid for the duration of the call.
     */
    static void publish(SystemEvent ev, void* payload = nullptr);

    /**
     * @brief Clear all subscriptions.
     *
     * Intended for unit tests that need a fresh EventBus state between runs.
     */
    static void reset();

private:
    using Table = std::array<std::vector<Handler>,
                             static_cast<size_t>(SystemEvent::_COUNT)>;

    /** @brief Returns the singleton dispatch table. */
    static Table& _table();
};

#endif // CORE_EVENT_BUS_H

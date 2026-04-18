#ifndef I_PAGE_H
#define I_PAGE_H

#include "PageState.h"
#include <stdint.h>

/**
 * @enum PageAction
 * @brief System-level actions a page can request via handleTouch().
 *
 * Pages set _lastAction before returning true from handleTouch() when a tap
 * triggers an operation requiring AppController-level logic.
 * PageRouter::consumePendingAction() drains this value after each touch cycle.
 */
enum class PageAction : uint8_t {
    None = 0,                  ///< Touch not consumed / no system action.
    Consumed,                  ///< Touch absorbed; no further system action needed.
    ForceSync,                 ///< User requested immediate weather sync.
    StartProvisioning,         ///< User requested portal / web-setup reboot.
    EnterSleep,                ///< User requested deep sleep.
    IncrementForecastOffset,   ///< Scroll forecast view forward one column.
    DecrementForecastOffset,   ///< Scroll forecast view back one column.
    IncrementSettingsCursor,   ///< Move settings highlight down.
    DecrementSettingsCursor,   ///< Move settings highlight up.
};

/**
 * @file IPage.h
 * @brief Abstract interface for all full-screen application pages.
 *
 * Lifecycle sequence when navigating TO a page:
 *   createPage() → init() → updateData() → onFocus() → render()
 *
 * Lifecycle sequence when navigating AWAY:
 *   onBlur() → delete
 *
 * Ownership: PageRouter creates and deletes IPage instances. Only one page
 * lives in RAM at a time (lazy loading) to minimise heap pressure on ESP32.
 */
class IPage {
protected:
    PageAction _lastAction = PageAction::None;

public:
    virtual ~IPage() = default;

    /** Called once after construction; allocate child widgets here. */
    virtual void init() = 0;

    /** Called when navigating TO this page; triggers an epd_quality clear. */
    virtual void onFocus() = 0;

    /** Called when navigating AWAY from this page; release any transient state. */
    virtual void onBlur() = 0;

    /** Inject the latest PageState; propagate to child widgets. */
    virtual void updateData(const PageState& state) = 0;

    /** Redraw dirty (or all) widgets and flush to e-ink. */
    virtual void render() = 0;

    /**
     * @brief Route touch coordinates to interactive widgets (hit-test loop).
     * @param x  Touch X coordinate.
     * @param y  Touch Y coordinate.
     * @return   true if the touch was consumed by this page.
     */
    virtual bool handleTouch(int16_t x, int16_t y) = 0;

    /**
     * @brief Consume and return the last action requested by handleTouch().
     *
     * Resets _lastAction to PageAction::None after reading.
     * Call this on the page after handleTouch() returns true.
     */
    PageAction consumeLastAction() {
        PageAction a = _lastAction;
        _lastAction  = PageAction::None;
        return a;
    }
};

#endif // I_PAGE_H

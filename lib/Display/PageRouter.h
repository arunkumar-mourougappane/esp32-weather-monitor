#ifndef PAGE_ROUTER_H
#define PAGE_ROUTER_H

#include "IPage.h"
#include "SystemState.h"
#include <stdint.h>

/**
 * @file PageRouter.h
 * @brief Central page lifecycle manager and navigation controller.
 *
 * PageRouter owns exactly one IPage* at a time (lazy loading).
 *
 * Full navigation lifecycle (navigateTo):
 *   onBlur() → delete → PageFactory::create() → init() → updateData() → onFocus() → render()
 *
 * Restore lifecycle (restore — used on EXT0 wakeup when the screen is already rendered):
 *   delete → PageFactory::create() → init() → updateData()
 *   (onFocus/render are skipped because the display already shows the correct content.)
 *
 * RTC memory persists the active page id so the correct page is reloaded after deep sleep.
 */
class PageRouter {
public:
    PageRouter() = default;
    ~PageRouter();

    /**
     * @brief Restore the page for @p pageId without triggering onFocus()/render().
     *
     * Used at session startup when the e-ink display already shows the correct
     * page image (EXT0 wakeup fast-render was done before entering the session).
     *
     * @param pageId  Active page to restore.
     * @param state   Current SystemState to inject via updateData().
     */
    void restore(uint8_t pageId, const SystemState& state);

    /**
     * @brief Navigate to a page, running the full teardown → setup lifecycle.
     *
     * @param pageId  Target page identifier.
     * @param state   Current SystemState passed to updateData().
     */
    void navigateTo(uint8_t pageId, const SystemState& state);

    /** @brief Cycle to the next page (wraps around kMaxPages). */
    void navigateNext(const SystemState& state);

    /** @brief Cycle to the previous page (wraps around kMaxPages). */
    void navigatePrev(const SystemState& state);

    /** @brief Push updated state to the current page without navigating. */
    void updateData(const SystemState& state);

    /** @brief Re-render the current page. */
    void render();

    /**
     * @brief Route touch coordinates through the active page's hit-test loop.
     *
     * After calling this, check consumePendingAction() for any system-level
     * operation the page requested (force sync, sleep, etc.).
     *
     * @return true if the touch was consumed by the page.
     */
    bool handleTouch(int16_t x, int16_t y);

    /** @brief Return the id of the currently active page. */
    uint8_t getActivePageId() const { return _activePageId; }

    /**
     * @brief Consume and return any system action requested by the last handleTouch().
     *
     * Resets the pending action to PageAction::None.
     * AppController must call this after handleTouch() returns true.
     */
    PageAction consumePendingAction();

private:
    static constexpr uint8_t kMaxPages = 4;

    IPage*   _activePage   = nullptr;
    uint8_t  _activePageId = 0;

    void _teardown();
};

#endif // PAGE_ROUTER_H

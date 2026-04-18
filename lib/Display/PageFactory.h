#ifndef PAGE_FACTORY_H
#define PAGE_FACTORY_H

#include "IPage.h"

/**
 * @brief Numeric identifiers for all application pages.
 *
 * Values match the legacy Page enum in DisplayManager.h to ensure RTC-persisted
 * page IDs remain valid across firmware updates.
 */
enum class PageId : uint8_t {
    Dashboard = 0,
    Hourly    = 1,
    Forecast  = 2,
    Settings  = 3,
    _Count    = 4,
};

/**
 * @file PageFactory.h
 * @brief Factory that maps a PageId to a heap-allocated IPage instance.
 *
 * Centralising construction here means PageRouter never includes concrete
 * page headers — adding a new page only requires updating PageFactory.cpp.
 */
class PageFactory {
public:
    /**
     * @brief Allocate the IPage implementation for @p id.
     * @param id  Page identifier (cast PageId to uint8_t, or use raw enum value).
     * @return    Heap-allocated IPage*; caller takes ownership.
     *            Returns a DashboardPage for unknown ids.
     */
    static IPage* create(uint8_t id);
};

#endif // PAGE_FACTORY_H

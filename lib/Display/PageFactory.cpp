#include "PageFactory.h"
#include "Pages/DashboardPage.h"
#include "Pages/HourlyPage.h"
#include "Pages/ForecastPage.h"
#include "Pages/SettingsPage.h"

IPage* PageFactory::create(uint8_t id) {
    switch (id) {
        case static_cast<uint8_t>(PageId::Hourly):    return new HourlyPage();
        case static_cast<uint8_t>(PageId::Forecast):  return new ForecastPage();
        case static_cast<uint8_t>(PageId::Settings):  return new SettingsPage();
        case static_cast<uint8_t>(PageId::Dashboard): // fall-through
        default:                                       return new DashboardPage();
    }
}

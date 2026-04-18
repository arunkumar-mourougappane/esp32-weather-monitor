#include "PageRouter.h"
#include "PageFactory.h"
#include <esp_log.h>

static const char* TAG = "PageRouter";

PageRouter::~PageRouter() {
    _teardown();
}

void PageRouter::_teardown() {
    if (_activePage) {
        _activePage->onBlur();
        delete _activePage;
        _activePage = nullptr;
    }
}

void PageRouter::restore(uint8_t pageId, const SystemState& state) {
    _teardown();
    _activePageId = pageId;
    _activePage   = PageFactory::create(pageId);
    _activePage->init();
    _activePage->updateData(state);
    ESP_LOGI(TAG, "Page %u restored (no redraw)", pageId);
}

void PageRouter::navigateTo(uint8_t pageId, const SystemState& state) {
    ESP_LOGI(TAG, "Navigating %u → %u", _activePageId, pageId);
    _teardown();
    _activePageId = pageId;
    _activePage   = PageFactory::create(pageId);
    _activePage->init();
    _activePage->updateData(state);
    _activePage->onFocus();
    _activePage->render();
}

void PageRouter::navigateNext(const SystemState& state) {
    navigateTo((_activePageId + 1) % kMaxPages, state);
}

void PageRouter::navigatePrev(const SystemState& state) {
    navigateTo((_activePageId + kMaxPages - 1) % kMaxPages, state);
}

void PageRouter::updateData(const SystemState& state) {
    if (_activePage) _activePage->updateData(state);
}

void PageRouter::render() {
    if (_activePage) _activePage->render();
}

bool PageRouter::handleTouch(int16_t x, int16_t y) {
    if (!_activePage) return false;
    return _activePage->handleTouch(x, y);
}

PageAction PageRouter::consumePendingAction() {
    if (!_activePage) return PageAction::None;
    return _activePage->consumeLastAction();
}

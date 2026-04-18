#include "EventBus.h"

EventBus::Table& EventBus::_table() {
    static Table t;
    return t;
}

void EventBus::subscribe(SystemEvent ev, Handler handler) {
    _table()[static_cast<size_t>(ev)].push_back(std::move(handler));
}

void EventBus::publish(SystemEvent ev, void* payload) {
    for (auto& h : _table()[static_cast<size_t>(ev)]) {
        h(payload);
    }
}

void EventBus::reset() {
    for (auto& v : _table()) {
        v.clear();
    }
}

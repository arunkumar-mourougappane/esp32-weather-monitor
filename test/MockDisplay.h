/**
 * @file MockDisplay.h
 * @brief MockDisplay — IDisplayController implementation for unit testing.
 *
 * MockDisplay records every draw call as a string entry in an internal log so
 * that unit tests can assert on what was rendered without requiring physical
 * e-ink hardware.
 *
 * Usage:
 * @code
 *   MockDisplay display(540, 960);
 *   ClockWidget clock(&display, 0, 0, 540, 95);
 *   clock.update(tm_struct);
 *   clock.draw();
 *
 *   ASSERT_TRUE(display.hasCall("fillRect"));
 *   ASSERT_TRUE(display.hasCall("drawString"));
 *   display.clearLog();
 * @endcode
 */
#pragma once

#include "../lib/Display/IDisplayController.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef MOCK_DISPLAY_MAX_LOG
/** @brief Maximum number of draw-call log entries stored by MockDisplay. */
#define MOCK_DISPLAY_MAX_LOG 256
#endif

/**
 * @class MockDisplay
 * @brief Recording stub of IDisplayController for use in PlatformIO unit tests.
 *
 * All drawing operations are no-ops that append a descriptive string to the
 * internal call log.  Tests can inspect the log with hasCall() or iterate it
 * with getLog() / getLogSize() to verify widget behaviour without hardware.
 *
 * ThemeColor → native colour mapping mimics EInkDriver (monochrome).
 */
class MockDisplay : public IDisplayController {
public:
    MockDisplay(int16_t w = 540, int16_t h = 960) : _w(w), _h(h) {}

    // ── Log inspection ────────────────────────────────────────────────────────

    /** @brief Return true if any log entry contains @p substr. */
    bool hasCall(const char* substr) const {
        for (int i = 0; i < _logSize; i++) {
            if (strstr(_log[i], substr) != nullptr) return true;
        }
        return false;
    }

    /** @brief Number of draw calls recorded since the last clearLog(). */
    int  getLogSize()              const { return _logSize; }

    /** @brief Access a specific log entry by zero-based index. */
    const char* getLog(int i)      const { return (i < _logSize) ? _log[i] : ""; }

    /** @brief Discard all recorded draw calls. */
    void clearLog() { _logSize = 0; }

    // ── IDisplayController: Lifecycle ─────────────────────────────────────────
    void     init()                                  override { _append("init"); }
    void     clear(ThemeColor c = ThemeColor::Background) override {
        char buf[48];
        snprintf(buf, sizeof(buf), "clear(%d)", (int)c);
        _append(buf);
    }
    void     flushFull()                             override { _append("flushFull"); }
    void     sleep()                                 override { _append("sleep"); }

    uint32_t resolveColor(ThemeColor c) const override {
        return (c == ThemeColor::Background) ? 0xFFFFFFu : 0x000000u;
    }

    // ── IDisplayController: Shapes ────────────────────────────────────────────
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t) override {
        char buf[64];
        snprintf(buf, sizeof(buf), "fillRect(%d,%d,%d,%d)", x, y, w, h);
        _append(buf);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t) override {
        char buf[64];
        snprintf(buf, sizeof(buf), "drawRect(%d,%d,%d,%d)", x, y, w, h);
        _append(buf);
    }
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint32_t) override {
        char buf[72];
        snprintf(buf, sizeof(buf), "drawRoundRect(%d,%d,%d,%d,r=%d)", x, y, w, h, r);
        _append(buf);
    }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint32_t) override {
        char buf[48];
        snprintf(buf, sizeof(buf), "fillCircle(%d,%d,r=%d)", x, y, r);
        _append(buf);
    }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint32_t) override {
        char buf[48];
        snprintf(buf, sizeof(buf), "drawCircle(%d,%d,r=%d)", x, y, r);
        _append(buf);
    }
    void drawArc(int16_t x, int16_t y, int16_t r0, int16_t r1,
                 float a0, float a1, uint32_t) override {
        char buf[72];
        snprintf(buf, sizeof(buf), "drawArc(%d,%d,r0=%d,r1=%d)", x, y, r0, r1);
        _append(buf);
    }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint32_t) override {
        char buf[72];
        snprintf(buf, sizeof(buf), "fillTriangle(%d,%d,%d,%d,%d,%d)", x0, y0, x1, y1, x2, y2);
        _append(buf);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint32_t) override {
        char buf[48];
        snprintf(buf, sizeof(buf), "drawFastHLine(%d,%d,w=%d)", x, y, w);
        _append(buf);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint32_t) override {
        char buf[48];
        snprintf(buf, sizeof(buf), "drawFastVLine(%d,%d,h=%d)", x, y, h);
        _append(buf);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t) override {
        char buf[64];
        snprintf(buf, sizeof(buf), "drawLine(%d,%d,%d,%d)", x0, y0, x1, y1);
        _append(buf);
    }
    void drawPixel(int16_t x, int16_t y, uint32_t) override {
        char buf[40];
        snprintf(buf, sizeof(buf), "drawPixel(%d,%d)", x, y);
        _append(buf);
    }

    // ── IDisplayController: Text ──────────────────────────────────────────────
    void setFont(WidgetFont f)       override {
        char buf[32];
        snprintf(buf, sizeof(buf), "setFont(%d)", (int)f);
        _append(buf);
    }
    void setTextSize(float s)        override {
        char buf[32];
        snprintf(buf, sizeof(buf), "setTextSize(%.1f)", (double)s);
        _append(buf);
    }
    void setTextColor(uint32_t c)    override {
        char buf[32];
        snprintf(buf, sizeof(buf), "setTextColor(0x%06X)", c);
        _append(buf);
    }
    void setTextDatum(WidgetDatum d) override {
        char buf[32];
        snprintf(buf, sizeof(buf), "setTextDatum(%d)", (int)d);
        _append(buf);
    }
    void drawString(const char* t, int16_t x, int16_t y) override {
        char buf[128];
        snprintf(buf, sizeof(buf), "drawString(\"%s\",%d,%d)", t, x, y);
        _append(buf);
    }
    void drawCentreString(const char* t, int16_t x, int16_t y) override {
        char buf[128];
        snprintf(buf, sizeof(buf), "drawCentreString(\"%s\",%d,%d)", t, x, y);
        _append(buf);
    }

    // ── IDisplayController: Weather icon ──────────────────────────────────────
    void drawWeatherIcon(const char* cond, int16_t x, int16_t y, int16_t sz) override {
        char buf[96];
        snprintf(buf, sizeof(buf), "drawWeatherIcon(\"%s\",%d,%d,sz=%d)", cond, x, y, sz);
        _append(buf);
    }

    // ── IDisplayController: Flush ─────────────────────────────────────────────
    void flushBoundingBox(int16_t x, int16_t y, int16_t w, int16_t h) override {
        char buf[64];
        snprintf(buf, sizeof(buf), "flushBoundingBox(%d,%d,%d,%d)", x, y, w, h);
        _append(buf);
    }

    // ── IDisplayController: Dimensions ───────────────────────────────────────
    int16_t getWidth()  const override { return _w; }
    int16_t getHeight() const override { return _h; }

private:
    int16_t _w, _h;

    char _log[MOCK_DISPLAY_MAX_LOG][128] = {};
    int  _logSize = 0;

    void _append(const char* entry) {
        if (_logSize < MOCK_DISPLAY_MAX_LOG) {
            snprintf(_log[_logSize], sizeof(_log[0]), "%s", entry);
            _logSize++;
        }
    }
};

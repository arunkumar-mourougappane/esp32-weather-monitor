/**
 * @file Widget.h
 * @brief Abstract base class for all component-based UI elements.
 *
 * Every visual element on the display inherits from Widget.  The base class
 * encapsulates:
 *   - bounding-box geometry (_x, _y, _w, _h) used by PageBase::render() to
 *     target e-ink partial refreshes at exactly the dirty region.
 *   - dirty flag (_isDirty) so the Page engine only redraws elements whose
 *     data actually changed since the last render cycle.
 *   - a pointer to IDisplayController (_gfx) for hardware-agnostic drawing.
 *
 * Subclasses implement draw() using only IDisplayController primitives so
 * that widget logic is fully decoupled from M5GFX / LGFX internals.
 */
#ifndef WIDGET_H
#define WIDGET_H

#include "IDisplayController.h"

class Widget {
protected:
    int16_t  _x, _y;
    uint16_t _w, _h;
    bool     _isDirty = true;
    IDisplayController* _gfx;

public:
    Widget(IDisplayController* gfx, int16_t x, int16_t y, uint16_t w, uint16_t h)
        : _x(x), _y(y), _w(w), _h(h), _isDirty(true), _gfx(gfx) {}

    virtual ~Widget() = default;

    // Subclasses implement this to (re)draw their content via _gfx primitives.
    // Implementations must call clearDirty() before returning.
    virtual void draw() = 0;

    bool isDirty()    const { return _isDirty; }
    void markDirty()        { _isDirty = true; }
    void clearDirty()       { _isDirty = false; }

    // Bounding-box accessors used by PageBase for targeted partial refresh.
    int16_t  getX()      const { return _x; }
    int16_t  getY()      const { return _y; }
    uint16_t getWidth()  const { return _w; }
    uint16_t getHeight() const { return _h; }
};

#endif // WIDGET_H

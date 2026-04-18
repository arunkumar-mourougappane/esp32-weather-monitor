/**
 * @file PageBase.h
 * @brief Page engine base class for component-based screen layouts.
 *
 * PageBase owns a list of Widget pointers and drives the render loop:
 *   1. Iterate all widgets.
 *   2. Call draw() only on dirty entries.
 *   3. After each dirty draw, call IDisplayController::flushBoundingBox() so
 *      the e-ink panel performs a targeted epd_fastest partial refresh limited
 *      to that widget's bounding rectangle.
 *
 * On a forced full refresh (forceFullRefresh = true) every widget is redrawn
 * unconditionally and a single full-screen flush is issued at the end.
 *
 * Named PageBase to avoid a name collision with the existing Page enum in
 * DisplayManager.h which selects the active interactive view.
 *
 * Ownership: PageBase owns its widget pointers and deletes them in its
 * destructor.  Widgets should be allocated with new and added via addWidget().
 */
#ifndef PAGE_BASE_H
#define PAGE_BASE_H

#include "Widget.h"
#include "IDisplayController.h"
#include <vector>

class PageBase {
protected:
    std::vector<Widget*> _widgets;
    IDisplayController*  _gfx;

public:
    explicit PageBase(IDisplayController* gfx) : _gfx(gfx) {}

    virtual ~PageBase() {
        for (auto* w : _widgets) delete w;
    }

    void addWidget(Widget* w) {
        _widgets.push_back(w);
    }

    /**
     * @brief Render all dirty (or all, on force) widgets and flush to display.
     *
     * @param forceFullRefresh  When true every widget is redrawn regardless of
     *                          its dirty flag, and a single full-canvas flush is
     *                          issued at the end instead of per-widget flushes.
     */
    void render(bool forceFullRefresh = false) {
        for (auto* w : _widgets) {
            if (w->isDirty() || forceFullRefresh) {
                w->draw();
                if (!forceFullRefresh) {
                    _gfx->flushBoundingBox(w->getX(), w->getY(),
                                           w->getWidth(), w->getHeight());
                }
            }
        }
        if (forceFullRefresh) {
            _gfx->flushFull();
        }
    }

    // Mark every widget dirty — call before returning to this page from another.
    void invalidateAll() {
        for (auto* w : _widgets) w->markDirty();
    }
};

#endif // PAGE_BASE_H

#ifndef DISPLAY_SYSTEM_STATE_H
#define DISPLAY_SYSTEM_STATE_H

/**
 * @file lib/Display/SystemState.h
 * @brief Backward-compatibility shim.
 *
 * The display-layer state struct was renamed from SystemState to PageState
 * and moved to lib/Display/PageState.h.
 *
 * The RTC-persistent application state is in lib/Models/SystemState.h as
 * struct SystemState / extern g_state.  Including this shim pulls in both
 * so that any translation unit that previously included <SystemState.h>
 * still compiles without modification.
 */
#include "PageState.h"
#include "../Models/SystemState.h"

#endif // DISPLAY_SYSTEM_STATE_H

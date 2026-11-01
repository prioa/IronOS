#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"

#ifdef TS101_DOG_UI
// State 1 = info page, state 2 = 1 + debug value while the raw values are showing (behind the last page)
OperatingMode showInfoPages(const ButtonState buttons, guiContext *cxt) {
  uint16_t &page  = cxt->scratch_state.state1;
  uint16_t &debug = cxt->scratch_state.state2;
  if (debug) {
    ui_draw_debug_menu(debug - 1);
    if (buttons == BUTTON_F_SHORT) {
      debug = debug % 16 + 1;
    } else if (buttons == BUTTON_B_SHORT || buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
      dogPressConsumed();
      debug = 0;
    }
    return OperatingMode::DebugMenuReadout;
  }
  dogDrawInfoPage(page);
  if (buttons == BUTTON_B_SHORT || buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Left;
    return cxt->previousMode;
  }
  if (buttons == BUTTON_F_SHORT) {
    page                = (page + 1) % DOG_INFO_PAGES;
    cxt->transitionMode = TransitionAnimation::Down;
  } else if (page == DOG_INFO_PAGES - 1 && dogHoldDone(true, DOG_HOLD_FULL)) {
    dogPressConsumed();
    debug = 1;
  }
  return OperatingMode::DebugMenuReadout;
}
#endif

OperatingMode showDebugMenu(const ButtonState buttons, guiContext *cxt) {
#ifdef TS101_DOG_UI
  return showInfoPages(buttons, cxt);
#endif

  ui_draw_debug_menu(cxt->scratch_state.state1);

  if (buttons == BUTTON_B_SHORT) {
    cxt->transitionMode = TransitionAnimation::Up;
    return OperatingMode::HomeScreen;
  } else if (buttons == BUTTON_F_SHORT) {
    cxt->scratch_state.state1++;
#if defined(MCU_TEMP_CUTOFF_C) || defined(DUAL_TIP_SOCKETS)
    // Skip the items this build doesn't have
#ifndef HALL_SENSOR
    if (cxt->scratch_state.state1 == 16) {
      cxt->scratch_state.state1++;
    }
#endif
#ifndef MCU_TEMP_CUTOFF_C
    if (cxt->scratch_state.state1 == 17) {
      cxt->scratch_state.state1++;
    }
#endif
#ifdef DUAL_TIP_SOCKETS
    if (cxt->scratch_state.state1 > 19) {
#else
    if (cxt->scratch_state.state1 > 17) {
#endif
      cxt->scratch_state.state1 = 0;
    }
#else
    // Keep these compile time constants: a variable modulo pulls in the division helper
#if defined(HALL_SENSOR)
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 17;
#else
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 16;
#endif
#endif
  }
  return OperatingMode::DebugMenuReadout; // Stay in debug menu
}

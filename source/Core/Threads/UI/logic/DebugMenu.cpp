#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"

#ifdef TS101_DOG_UI
// One more page after the stock ones: the sensor's three axes and the lie of the iron worked out from them, to check the axes on the iron
#define RAW_PAGES 17
static void drawLie() {
  char text[24];
  auto put = [&](int16_t x, uint8_t base, const char *label, int16_t value) {
    char *end = text;
    strcpy(end, label);
    end += strlen(end);
    if (value < 0) {
      *end++ = '-';
      value  = -value;
    }
    dogFormat(end, value);
    dogSmall(x, text, base);
  };
  // X -16384  Y -16384 / Z -16384  LAGE / LÄNGS -40 DREH -180
  put(0, 7, "X ", dogAccelRaw[0]);
  put(60, 7, "Y ", dogAccelRaw[1]);
  put(0, 16, "Z ", dogAccelRaw[2]);
  dogSmallRight(OLED_WIDTH - 1, DOG_T("LAGE", "LIE"), 16);
  int16_t along = 0, turn = 0;
  dogIronLie(along, turn);
  put(0, 25, DOG_T("LÄNGS ", "ALONG "), along);
  put(66, 25, DOG_T("DREH ", "TURN "), turn);
}

// The raw values behind the last info page. State 1 = which one
static OperatingMode showRawValues(const ButtonState buttons, guiContext *cxt) {
  uint16_t &value = cxt->scratch_state.state1;
  if (value == RAW_PAGES - 1) {
    drawLie();
  } else {
    ui_draw_debug_menu(value);
  }
  if (buttons == BUTTON_F_SHORT) {
    value = (value + 1) % RAW_PAGES;
  } else if (buttons == BUTTON_B_SHORT) {
    value = (value + RAW_PAGES - 1) % RAW_PAGES;
  } else if (buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Left;
    return cxt->previousMode;
  }
  return OperatingMode::DebugMenuReadout;
}
#endif

OperatingMode showDebugMenu(const ButtonState buttons, guiContext *cxt) {
#ifdef TS101_DOG_UI
  return showRawValues(buttons, cxt);
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

#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"

#ifdef TS101_DOG_UI
#define DOG_ADJUST_STEP_C 10
#define DOG_ADJUST_STEP_F 20

static void selectNextPreset() {
  static const uint16_t presetsC[] = TS101_PRESETS_C;
  static const uint16_t presetsF[] = TS101_PRESETS_F;
  const uint16_t       *presets    = getSettingValue(SettingsOptions::TemperatureInF) ? presetsF : presetsC;
  const uint16_t        current    = getSettingValue(SettingsOptions::SolderingTemp);
  // The first preset above the current set point, wrapping round to the lowest: one step, whatever it is now
  uint16_t next = presets[0];
  for (uint8_t i = 0; i < sizeof(presetsC) / sizeof(presetsC[0]); i++) {
    if (presets[i] > current) {
      next = presets[i];
      break;
    }
  }
  setSettingValue(SettingsOptions::SolderingTemp, next);
}

static void stepSetPoint(bool up) {
  const bool    inF  = getSettingValue(SettingsOptions::TemperatureInF);
  const int16_t step = inF ? DOG_ADJUST_STEP_F : DOG_ADJUST_STEP_C;
  const int16_t now  = getSettingValue(SettingsOptions::SolderingTemp);
  // To the next multiple of the step in that direction
  int16_t next = up ? (now / step + 1) * step : ((now - 1) / step) * step;
  const int16_t lo = inF ? MIN_TEMP_F : MIN_TEMP_C, hi = inF ? MAX_TEMP_F : MAX_TEMP_C;
  next = next < lo ? lo : (next > hi ? hi : next);
  setSettingValue(SettingsOptions::SolderingTemp, (uint16_t)next);
}

void dogAdjustOpened() { lastButtonTime = xTaskGetTickCount(); }

bool dogAdjustButtons(ButtonState buttons) {
  if (dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    return true;
  }
  if (dogHoldDone(true, DOG_HOLD_FULL)) {
    dogPressConsumed(); // exactly one preset per hold, however long it lasts
    selectNextPreset();
    return false;
  }
  const bool swapped = getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled);
  switch (buttons) {
  case BUTTON_F_SHORT:
    stepSetPoint(!swapped);
    break;
  case BUTTON_B_SHORT:
    stepSetPoint(swapped);
    break;
  case BUTTON_BOTH:
    return true;
  default:
    break;
  }
  return (xTaskGetTickCount() - lastButtonTime) > DOG_ADJUST_TIMEOUT;
}

// Only used from the home screen and the menu, with the heater off; while soldering the view lives in Soldering.cpp
OperatingMode gui_solderingTempAdjust(const ButtonState buttons, guiContext *cxt) {
  currentTempTargetDegC = 0;
  dogDrawTemperatureView(false);
  if (dogAdjustButtons(buttons)) {
    saveSettings();
    cxt->transitionMode = TransitionAnimation::Right;
    return cxt->previousMode;
  }
  return OperatingMode::TemperatureAdjust;
}
#else
OperatingMode gui_solderingTempAdjust(const ButtonState buttonIn, guiContext *cxt) {

  currentTempTargetDegC              = 0; // Turn off heater while adjusting temp
  uint16_t   *waitForRelease         = &(cxt->scratch_state.state1);
  uint32_t   *autoRepeatTimer        = &(cxt->scratch_state.state3);
  uint16_t   *autoRepeatAcceleration = &(cxt->scratch_state.state2);
  ButtonState buttons                = buttonIn;
  if (*waitForRelease == 0) {
    // When we first enter we wait for the user to release buttons before enabling changes
    if (buttons != BUTTON_NONE) {
      buttons = BUTTON_NONE;
    } else {
      (*waitForRelease)++;
    }
  }

  int16_t delta = 0;
  switch (buttons) {
  case BUTTON_NONE:
    // stay
    (*autoRepeatAcceleration) = 0;
    break;
  case BUTTON_BOTH:
    // exit
    saveSettings();
    cxt->transitionMode = TransitionAnimation::Right;
    return cxt->previousMode;
  case BUTTON_B_LONG:
    if (xTaskGetTickCount() - (*autoRepeatTimer) + (*autoRepeatAcceleration) > PRESS_ACCEL_INTERVAL_MAX) {
      delta              = -getSettingValue(SettingsOptions::TempChangeLongStep);
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
    }
    break;
  case BUTTON_B_SHORT:
    delta = -getSettingValue(SettingsOptions::TempChangeShortStep);
    break;
  case BUTTON_F_LONG:
    if (xTaskGetTickCount() - (*autoRepeatTimer) + (*autoRepeatAcceleration) > PRESS_ACCEL_INTERVAL_MAX) {
      delta              = getSettingValue(SettingsOptions::TempChangeLongStep);
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
    }
    break;
  case BUTTON_F_SHORT:
    delta = getSettingValue(SettingsOptions::TempChangeShortStep);
    break;
  default:
    break;
  }
  if ((PRESS_ACCEL_INTERVAL_MAX - (*autoRepeatAcceleration)) < PRESS_ACCEL_INTERVAL_MIN) {
    (*autoRepeatAcceleration) = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
  }
  // If buttons are flipped; flip the delta
  if (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled)) {
    delta = -delta;
  }
  if (delta != 0) {
    // constrain between the set temp limits, i.e. 10-450 C
    int16_t newTemp = getSettingValue(SettingsOptions::SolderingTemp);
    newTemp += delta;
    // Round to nearest increment of delta
    delta   = abs(delta);
    newTemp = (newTemp / delta) * delta;

    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      if (newTemp > MAX_TEMP_F) {
        newTemp = MAX_TEMP_F;
      } else if (newTemp < MIN_TEMP_F) {
        newTemp = MIN_TEMP_F;
      }
    } else {
      if (newTemp > MAX_TEMP_C) {
        newTemp = MAX_TEMP_C;
      } else if (newTemp < MIN_TEMP_C) {
        newTemp = MIN_TEMP_C;
      }
    }
    setSettingValue(SettingsOptions::SolderingTemp, (uint16_t)newTemp);
  }
  ui_draw_temperature_change();

  if (xTaskGetTickCount() - lastButtonTime > (TICKS_SECOND * 3)) {
    saveSettings();
    cxt->transitionMode = TransitionAnimation::Right;
    return cxt->previousMode; // exit if user just doesn't press anything for a bit
  }
  return OperatingMode::TemperatureAdjust; // Stay in temp adjust
}
#endif

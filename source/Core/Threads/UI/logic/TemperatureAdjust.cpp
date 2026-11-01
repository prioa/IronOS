#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"

#ifdef TS101_DOG_UI
#define PRESET_COUNT (sizeof(presetsC) / sizeof(presetsC[0]))
static const uint16_t presetsC[] = TS101_PRESETS_C;
static const uint16_t presetsF[] = TS101_PRESETS_F;

static const uint16_t *presets() { return getSettingValue(SettingsOptions::TemperatureInF) ? presetsF : presetsC; }

int8_t dogPresetIndex() {
  const uint16_t set = getSettingValue(SettingsOptions::SolderingTemp);
  for (uint8_t i = 0; i < PRESET_COUNT; i++) {
    if (presets()[i] == set) {
      return i;
    }
  }
  return -1;
}

// A click while soldering: the next solder preset in that direction, staying at the ends so a click can't jump from one end to the other
void dogJumpPreset(bool up) {
  const uint16_t current = getSettingValue(SettingsOptions::SolderingTemp);
  for (uint8_t i = 0; i < PRESET_COUNT; i++) {
    const uint16_t candidate = presets()[up ? i : PRESET_COUNT - 1 - i];
    if (up ? candidate > current : candidate < current) {
      setSettingValue(SettingsOptions::SolderingTemp, candidate);
      return;
    }
  }
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

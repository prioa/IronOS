
#include "Buttons.hpp"
#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"

bool showExitMenuTransition = false;

#ifdef TS101_DOG_UI
static TickType_t lockNoticeUntil = 0; // says what the lock gesture just did

static OperatingMode handleDogHomeButtons(const ButtonState buttons, guiContext *cxt) {
  if (buttons != BUTTON_NONE && cxt->scratch_state.state1 == 0) {
    return OperatingMode::HomeScreen; // whatever was pressed on the way here: wait until the buttons are let go once
  }
  cxt->scratch_state.state1 = 1;
  if (dogAlert != DogAlert::None) {
    // An alert stays until it is safe again and a button says it was read; the iron can't start before that
    if (buttons != BUTTON_NONE && (dogAlert != DogAlert::Handle || getHandleTemperature(0) < DOG_HANDLE_LIMIT_X10C)) {
      dogAlert = DogAlert::None;
    }
    return OperatingMode::HomeScreen;
  }
  if (buttons == BUTTON_BOTH_LONG && getSettingValue(SettingsOptions::LockingMode)) {
    dogPressConsumed(); // once per press; while soldering both buttons stop instead, so the lock is switched here
    dogLockArmed    = !dogLockArmed;
    lockNoticeUntil = xTaskGetTickCount() + TICKS_SECOND * 3 / 2;
    return OperatingMode::HomeScreen;
  }
  if (dogHoldDone(true, DOG_HOLD_FULL)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::TemperatureAdjust;
  }
  if (dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::DebugMenuReadout;
  }
  switch (buttons) {
  case BUTTON_F_SHORT:
    if (!isTipDisconnected()) {
      if (getHandleTemperature(0) >= DOG_HANDLE_LIMIT_X10C) {
        dogAlert = DogAlert::Handle;
        break;
      }
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::Soldering;
    }
    break;
  case BUTTON_B_SHORT:
    cxt->transitionMode = TransitionAnimation::Right;
    return OperatingMode::SettingsMenu;
  default:
    break;
  }
  return OperatingMode::HomeScreen;
}
#endif

OperatingMode handleHomeButtons(const ButtonState buttons, guiContext *cxt) {
  if (buttons != BUTTON_NONE && cxt->scratch_state.state1 == 0) {
    return OperatingMode::HomeScreen; // Ignore button press
  } else {
    cxt->scratch_state.state1 = 1;
  }
  switch (buttons) {
  case BUTTON_NONE:
    // Do nothing
    break;
  case BUTTON_BOTH:
    break;

  case BUTTON_B_LONG:
    cxt->transitionMode = TransitionAnimation::Down;
    return OperatingMode::DebugMenuReadout;
    break;
  case BUTTON_F_LONG:
#ifdef PROFILE_SUPPORT
    if (!isTipDisconnected()) {
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::SolderingProfile;
    } else {
      return OperatingMode::HomeScreen;
    }
#else
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::TemperatureAdjust;
#endif
    break;
  case BUTTON_F_SHORT:
    if (!isTipDisconnected()) {
      bool detailedView   = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
      cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Left;
      return OperatingMode::Soldering;
    }
    break;
  case BUTTON_B_SHORT:
    cxt->transitionMode = TransitionAnimation::Right;
    return OperatingMode::SettingsMenu;
    break;
  default:
    break;
  }
  return OperatingMode::HomeScreen;
}

OperatingMode drawHomeScreen(const ButtonState buttons, guiContext *cxt) {

  currentTempTargetDegC = 0; // ensure tip is off
#ifdef TS101_DOG_UI
  dogSessionEnded(); // remember the last session and persist the lifetime soldering minutes
#endif
  getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  uint32_t tipTemp = TipThermoModel::getTipInC();

  // Setup LCD Cursor location
  if (OLED::getRotation()) {
    OLED::setCursor(50, 0);
  } else {
    OLED::setCursor(-1, 0);
  }
#ifdef TS101_DOG_UI
  dogSetLocked(false);
  if (xTaskGetTickCount() < lockNoticeUntil) {
    dogWarnLocked(dogLockArmed, BUTTON_NONE);
  } else {
    ui_draw_homescreen_simplified(getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : tipTemp);
  }
  return handleDogHomeButtons(buttons, cxt);
#else
  if (getSettingValue(SettingsOptions::DetailedIDLE)) {
    ui_draw_homescreen_detailed(tipTemp);
  } else {
    ui_draw_homescreen_simplified(tipTemp);
  }
  return handleHomeButtons(buttons, cxt);
#endif
}

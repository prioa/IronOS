#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
#define DOG_WAKE_FRAME_TICKS (TICKS_100MS * 12 / 10) // each of the three frames of the dog looking up

OperatingMode gui_SolderingSleepingMode(const ButtonState buttons, guiContext *cxt) {
#ifdef NO_SLEEP_MODE
  return OperatingMode::Soldering;
#endif
  // Drop to sleep temperature and display until movement or button press
#ifdef TS101_DOG_UI
  // State 3 = the tick the wake frames started, 0 while asleep
  if (!cxt->scratch_state.state3) {
    dogSleepWakeFrame = 0;
  }
  if (dogBothDown() || dogHoldDone(false, DOG_HOLD_STOP)) {
    dogPressConsumed();
    currentTempTargetDegC = 0;
    cxt->transitionMode   = TransitionAnimation::Right;
    return OperatingMode::HomeScreen; // heater off, to the start
  }
  if (dogPressed(true)) {
    dogPressConsumed(); // the press that wakes the iron does nothing else: it must not start a boost
  }
  if (dogHandleTooHot()) {
    currentTempTargetDegC = 0; // the handle limit holds in sleep too, the sleep temperature still heats
    dogAlert              = DogAlert::Handle;
    dogPressConsumed();
    return OperatingMode::HomeScreen;
  }
#endif

  // user moved or pressed a button, go back to soldering
  // If in the first two seconds we disable this to let accelerometer warm up

#ifdef POW_DC
  if (checkForUnderVoltage()) {
    return OperatingMode::HomeScreen; // return non-zero on error
  }
#endif

  if (cxt->scratch_state.state4) {
    // Hibernating mode
    currentTempTargetDegC = 0;
  } else {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(min(getSettingValue(SettingsOptions::SleepTemp), getSettingValue(SettingsOptions::SolderingTemp)));
    } else {
      currentTempTargetDegC = min(getSettingValue(SettingsOptions::SleepTemp), getSettingValue(SettingsOptions::SolderingTemp));
    }
  }
#ifdef TS101_DOG_UI
  // Moved, or a button let go: three frames of the dog looking up, with the heater already on its way back to the solder
  // (the rear button may be on its way to a stop: only letting go wakes the iron, even if it moves meanwhile)
  if (!cxt->scratch_state.state3 && !dogPressed(false) && !cxt->scratch_state.state4 && !shouldBeSleeping()) {
    dogPressConsumed(); // a click that woke the iron must not reach soldering, where it would change the solder
    cxt->scratch_state.state3 = xTaskGetTickCount() | 1;
  }
  if (cxt->scratch_state.state3) {
    const TickType_t since = (xTaskGetTickCount() | 1) - cxt->scratch_state.state3;
    if (since >= 3 * DOG_WAKE_FRAME_TICKS) {
      dogSleepWakeFrame = 0;
      return cxt->previousMode;
    }
    dogSleepWakeFrame     = 1 + since / DOG_WAKE_FRAME_TICKS;
    currentTempTargetDegC = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::SolderingTemp)) : getSettingValue(SettingsOptions::SolderingTemp);
  }
#endif
  // draw the lcd
  uint16_t tipTemp = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC();

  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    ui_draw_soldering_detailed_sleep(tipTemp);
  } else {
    ui_draw_soldering_basic_sleep(tipTemp);
  }

#ifdef TS101_DOG_UI
  if (cxt->scratch_state.state3 || dogPressed(false)) {
    return OperatingMode::Sleeping; // waking, or the rear button may be on its way to a stop
  }
  if (cxt->scratch_state.state4 && !shouldBeSleeping()) {
    dogPressConsumed(); // hibernating: GUIThread goes on to soldering by itself
  }
#else
  if (!shouldBeSleeping()) {
    return cxt->previousMode;
  }
#endif

  if (shouldShutdown()) {
    // shutdown
    currentTempTargetDegC = 0;
    return OperatingMode::HomeScreen;
  }
  if (cxt->scratch_state.state4) {
    return OperatingMode::Hibernating;
  } else {
    return OperatingMode::Sleeping;
  }
}

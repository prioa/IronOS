#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
OperatingMode gui_SolderingSleepingMode(const ButtonState buttons, guiContext *cxt) {
#ifdef NO_SLEEP_MODE
  return OperatingMode::Soldering;
#endif
  // Drop to sleep temperature and display until movement or button press
#ifdef TS101_DOG_UI
  if (!cxt->scratch_state.state5) {
    cxt->scratch_state.state5 = 1;
    dogSleepEnterTime         = xTaskGetTickCount(); // the dog lies down, then falls asleep
  }
  if (dogBothDown() || dogHoldDone(false, DOG_HOLD_STOP)) {
    dogPressConsumed();
    currentTempTargetDegC = 0;
    cxt->transitionMode   = TransitionAnimation::Right;
    return OperatingMode::HomeScreen; // heater off, to the start
  }
  if (dogPressed(true)) {
    dogPressConsumed(); // the press that wakes the iron does nothing else: it must not start a boost or open a view
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
  // draw the lcd
  uint16_t tipTemp = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC();

  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    ui_draw_soldering_detailed_sleep(tipTemp);
  } else {
    ui_draw_soldering_basic_sleep(tipTemp);
  }

#ifdef TS101_DOG_UI
  // The rear button may be on its way to a stop: only letting go wakes the iron, even if it moves meanwhile
  if (dogPressed(false)) {
    return cxt->scratch_state.state4 ? OperatingMode::Hibernating : OperatingMode::Sleeping;
  }
#endif
  if (!shouldBeSleeping()) {
    return cxt->previousMode;
  }

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

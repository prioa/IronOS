
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
// State 1 = button locking  (0:unlocked+released, 1:unlocked, 2:locked, 3:locked+released)
// State 2 = boost mode
// State 3 = buzzer timer
// State 6 = TS101_DOG_UI session bookkeeping done for this entry

#ifdef TS101_DOG_UI
static void selectNextPreset() {
  static const uint16_t presetsC[] = TS101_PRESETS_C;
  static const uint16_t presetsF[] = TS101_PRESETS_F;
  const bool            inF        = getSettingValue(SettingsOptions::TemperatureInF);
  const uint16_t       *presets    = inF ? presetsF : presetsC;
  const uint8_t         count      = sizeof(presetsC) / sizeof(presetsC[0]);
  const uint16_t        current    = getSettingValue(SettingsOptions::SolderingTemp);
  // Pick the first preset above the current set point, wrapping round to the lowest
  uint16_t next = presets[0];
  for (uint8_t i = 0; i < count; i++) {
    if (presets[i] > current) {
      next = presets[i];
      break;
    }
  }
  setSettingValue(SettingsOptions::SolderingTemp, next);
  dogSettingsChanged(); // saved on the home screen, not on every click
  dogPresetShownUntil = xTaskGetTickCount() + TICKS_SECOND;
}
#endif

OperatingMode handleSolderingButtons(const ButtonState buttons, guiContext *cxt) {
  if (cxt->scratch_state.state1 >= 2) {
    // Buttons are currently locked
    if (buttons == BUTTON_BOTH_LONG) {
      if (cxt->scratch_state.state1 == 3) {
        // Unlocking
        if (warnUser(translatedString(Tr->UnlockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 1;
          cxt->scratch_state.state7 = 0;
        }
      } else {
        warnUser(translatedString(Tr->LockingKeysString), buttons);
      }
      return OperatingMode::Soldering;
    }
    if (cxt->scratch_state.state7 != 0) {
      // show locked until timer is up
      if (xTaskGetTickCount() >= cxt->scratch_state.state7) {
        cxt->scratch_state.state7 = 0;
      } else {
        warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
        return OperatingMode::Soldering;
      }
    }
    switch (buttons) {
    case BUTTON_NONE:
      cxt->scratch_state.state1 = 3;
      cxt->scratch_state.state2 = 0;
      break;
    case BUTTON_F_LONG:
      if (getSettingValue(SettingsOptions::BoostTemp) && (getSettingValue(SettingsOptions::LockingMode) == lockingMode_t::BOOST)) {
        cxt->scratch_state.state2 = 1;
        break;
      }
    /*Fall through*/
    default: // Set timer for and display a lock warning
      cxt->scratch_state.state7 = xTaskGetTickCount() + TICKS_SECOND;
      warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
      break;
    }
    return OperatingMode::Soldering;
  }

  bool detailedView = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
  // otherwise we are unlocked
  switch (buttons) {
  case BUTTON_NONE:
    cxt->scratch_state.state2 = 0;
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_BOTH:
  /*Fall through*/
  case BUTTON_B_LONG:
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  case BUTTON_F_LONG:
    // if boost mode is enabled turn it on
    if (getSettingValue(SettingsOptions::BoostTemp)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_F_SHORT:
#ifdef TS101_DOG_UI
    // The front button steps through the temperature presets
    selectNextPreset();
    break;
#endif
  case BUTTON_B_SHORT:
#ifdef TS101_DOG_UI
    // The back button steps through the soldering views
    dogNextSolderView();
    break;
#endif
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::TemperatureAdjust;
  case BUTTON_BOTH_LONG:
    if (getSettingValue(SettingsOptions::LockingMode)) {
      // Lock buttons
      if (cxt->scratch_state.state1 == 0) {
        if (warnUser(translatedString(Tr->LockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 2;
        }
      } else {
        // FIXME should be WarningKeysUnlockedString
        warnUser(translatedString(Tr->UnlockingKeysString), buttons);
      }
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
}

OperatingMode gui_solderingMode(const ButtonState buttons, guiContext *cxt) {
  /*
   * * Soldering (gui_solderingMode)
   * -> Main loop where we draw temp, and animations
   * --> User presses buttons and they goto the temperature adjust screen
   * ---> Display the current setpoint temperature
   * ---> Use buttons to change forward and back on temperature
   * ---> Both buttons or timeout for exiting
   * --> Long hold front button to enter boost mode
   * ---> Just temporarily sets the system into the alternate temperature for
   * PID control
   * --> Long hold back button to exit
   * --> Double button to exit
   * --> Long hold double button to toggle key lock
   */
#ifdef TS101_DOG_UI
  if (!cxt->scratch_state.state6) {
    cxt->scratch_state.state6 = 1;
    if (cxt->previousMode == OperatingMode::Sleeping || cxt->previousMode == OperatingMode::Hibernating) {
      dogWakeUntil = xTaskGetTickCount() + TICKS_SECOND;
    } else if (cxt->previousMode != OperatingMode::TemperatureAdjust) {
      // A new session starts unless we are only back from sleeping or adjusting the set point
      dogSolderTimerStart = 0;
      dogHeatupStartTempC = TipThermoModel::getTipInC();
      dogCurveReset();
    }
  }
#endif

  // Update the setpoints for the temperature
  if (cxt->scratch_state.state2) {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::BoostTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::BoostTemp));
    }
  } else {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::SolderingTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::SolderingTemp));
    }
  }

  // Update status
  int error = currentTempTargetDegC - TipThermoModel::getTipInC();
  if (error >= -10 && error <= 10) {
    // converged; beep once per soldering session, the first time the set point is reached
    if (!cxt->scratch_state.state5) {
      setBuzzer(true);
      cxt->scratch_state.state3 = xTaskGetTickCount() + TICKS_SECOND / 3;
      cxt->scratch_state.state5 = true;
    }
#ifdef TS101_DOG_UI
    if (!dogSolderTimerStart) {
      dogSolderTimerStart = xTaskGetTickCount() | 1; // never 0, that means "not started"
      dogBarkUntil        = xTaskGetTickCount() + TICKS_SECOND * 2;
    }
#endif
    setStatusLED(LED_HOT);
  } else {
    setStatusLED(LED_HEATING);
  }
  if (cxt->scratch_state.state3 != 0 && xTaskGetTickCount() >= cxt->scratch_state.state3) {
    setBuzzer(false);
  }

#ifdef TS101_DOG_UI
  dogCurveSample(TipThermoModel::getTipInC());
  dogCountSolderTime();
#endif
  // Draw in the screen details
#ifdef TS101_DOG_UI
  if (getSettingValue(SettingsOptions::DogSolderView) == 1) {
    ui_draw_soldering_power_status(cxt->scratch_state.state2);
  } else {
    ui_draw_soldering_basic_status(cxt->scratch_state.state2);
  }
#else
  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    ui_draw_soldering_power_status(cxt->scratch_state.state2);
  } else {
    ui_draw_soldering_basic_status(cxt->scratch_state.state2);
  }
#endif

  bool detailedView = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
  // Check if we should bail due to undervoltage for example
  if (checkExitSoldering()) {
    setBuzzer(false);
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  }
#ifdef NO_SLEEP_MODE

  if (shouldShutdown()) {
    // shutdown
    currentTempTargetDegC = 0;
    cxt->transitionMode   = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  }
#endif
  if (shouldBeSleeping()) {
    return OperatingMode::Sleeping;
  }

  if (heaterThermalRunawayCounter > 8) {
    currentTempTargetDegC       = 0; // heater control off
    heaterThermalRunawayCounter = 0;
    cxt->transitionMode         = TransitionAnimation::Right;
    return OperatingMode::ThermalRunaway;
  }
  return handleSolderingButtons(buttons, cxt);
}


#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
// State 1 = button locking  (0:unlocked+released, 1:unlocked, 2:locked, 3:locked+released)
// State 2 = boost mode
// State 3 = buzzer timer
// State 6 = TS101_DOG_UI bit flags, see below

#ifdef TS101_DOG_UI
#define SCRATCH_SESSION_DONE 1 // state 6 bit: session bookkeeping done for this entry
#define SCRATCH_ADJUST_OPEN  2 // state 6 bit: the temperature view is showing; the heater stays on
static TickType_t boostEndedAt = 0;

static TickType_t boostStartedAt = 0; // 0 = no boost running
static bool       boostUsedUp    = false; // ran for the longest it may: stays off until the button is let go

uint8_t dogBoostSecondsLeft() {
  const TickType_t ran = boostStartedAt ? xTaskGetTickCount() - boostStartedAt : 0;
  return ran >= DOG_BOOST_MAX ? 0 : (DOG_BOOST_MAX - ran + TICKS_SECOND - 1) / TICKS_SECOND;
}

// Boost has a time limit; wherever the button handling turned it on, locked or not, it goes through here
static void limitBoost(const ButtonState buttons, guiContext *cxt) {
  if (cxt->scratch_state.state2) {
    const TickType_t now = xTaskGetTickCount() | 1;
    if (!boostStartedAt && !boostUsedUp) {
      boostStartedAt = now;
    }
    if (boostUsedUp || now - boostStartedAt >= DOG_BOOST_MAX) {
      boostUsedUp               = true;
      boostStartedAt            = 0;
      cxt->scratch_state.state2 = 0;
    }
  } else {
    boostStartedAt = 0;
    if (buttons != BUTTON_F_LONG) {
      boostUsedUp = false;
    }
  }
}

static bool boostJustEnded() { return boostEndedAt && (xTaskGetTickCount() - boostEndedAt) < DOG_BOOST_CLICK_GUARD; }

// Front handles, rear leads: a click only ever opens something or changes the view, never the heater
static OperatingMode handleDogButtons(const ButtonState buttons, guiContext *cxt) {
  if (cxt->scratch_state.state6 & SCRATCH_ADJUST_OPEN) {
    if (dogAdjustButtons(buttons, true)) {
      cxt->scratch_state.state6 &= ~SCRATCH_ADJUST_OPEN;
      dogSettingsChanged(); // saved on the home screen: no flash write with the heater on, and it spares the flash
    }
    return OperatingMode::Soldering;
  }
  if (dogHoldDone(false, DOG_HOLD_STOP)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  }
  switch (buttons) {
  case BUTTON_NONE:
    if (cxt->scratch_state.state2) {
      boostEndedAt = xTaskGetTickCount() | 1;
    }
    cxt->scratch_state.state2 = 0;
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_F_LONG:
    if (getSettingValue(SettingsOptions::BoostTemp) && dogPressed(true)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_F_SHORT:
  case BUTTON_B_SHORT:
    if (!boostJustEnded()) {
      cxt->scratch_state.state6 |= SCRATCH_ADJUST_OPEN;
      dogAdjustOpened();
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
}
#endif

#ifdef TS101_DOG_UI
#define DOG_WARN_UNLOCKING(buttons) dogWarnLocked(false, buttons)
#define DOG_WARN_LOCKING(buttons)   dogWarnLocked(true, buttons)
#define DOG_WARN_LOCKED(buttons)    dogWarnLocked(true, buttons)
#else
#define DOG_WARN_UNLOCKING(buttons) warnUser(translatedString(Tr->UnlockingKeysString), buttons)
#define DOG_WARN_LOCKING(buttons)   warnUser(translatedString(Tr->LockingKeysString), buttons)
#define DOG_WARN_LOCKED(buttons)    warnUser(translatedString(Tr->WarningKeysLockedString), buttons)
#endif

OperatingMode handleSolderingButtons(const ButtonState buttons, guiContext *cxt) {
#ifdef TS101_DOG_UI
  // Both buttons always stop at once: while locked, during boost, with the temperature view open
  if (dogBothDown()) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  }
#endif
  if (cxt->scratch_state.state1 >= 2) {
    // Buttons are currently locked
    if (buttons == BUTTON_BOTH_LONG) {
      if (cxt->scratch_state.state1 == 3) {
        // Unlocking
        if (DOG_WARN_UNLOCKING(buttons)) {
          cxt->scratch_state.state1 = 1;
          cxt->scratch_state.state7 = 0;
        }
      } else {
        DOG_WARN_LOCKING(buttons);
      }
      return OperatingMode::Soldering;
    }
    if (cxt->scratch_state.state7 != 0) {
      // show locked until timer is up
      if (xTaskGetTickCount() >= cxt->scratch_state.state7) {
        cxt->scratch_state.state7 = 0;
      } else {
        DOG_WARN_LOCKED(buttons);
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
      DOG_WARN_LOCKED(buttons);
      break;
    }
    return OperatingMode::Soldering;
  }

#ifdef TS101_DOG_UI
  return handleDogButtons(buttons, cxt);
#else
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
  case BUTTON_B_SHORT:
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
#endif
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
  if (!(cxt->scratch_state.state6 & SCRATCH_SESSION_DONE)) {
    cxt->scratch_state.state6 |= SCRATCH_SESSION_DONE;
    if (cxt->previousMode != OperatingMode::Sleeping && cxt->previousMode != OperatingMode::Hibernating) {
      // A new session starts unless we are only back from sleeping
      dogSolderTimerStart = 0;
      dogHeatupStartTempC = TipThermoModel::getTipInC();
      dogCurveReset();
    }
  }
  if (dogLockArmed && cxt->scratch_state.state1 < 2) {
    cxt->scratch_state.state1 = 2; // switched on at the start, it holds for the whole session, also after sleep
  }
  dogSetLocked(cxt->scratch_state.state1 >= 2);
  limitBoost(buttons, cxt);
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
      dogReachedUntil     = xTaskGetTickCount() + TICKS_100MS * 2;
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
  if (cxt->scratch_state.state6 & SCRATCH_ADJUST_OPEN) {
    dogDrawTemperatureView(true);
  } else if (getSettingValue(SettingsOptions::DogSolderView) == 1) {
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
#ifdef TS101_DOG_UI
  if (dogHandleTooHot()) {
    currentTempTargetDegC = 0; // heater control off; the home screen says why
    dogAlert              = DogAlert::Handle;
    dogPressConsumed(); // a press still under way must not read as an answer to the alert
    cxt->transitionMode   = TransitionAnimation::Right;
    return OperatingMode::HomeScreen;
  }
#endif
  if (shouldBeSleeping()) {
    return OperatingMode::Sleeping;
  }

  if (heaterThermalRunawayCounter > 8) {
    currentTempTargetDegC       = 0; // heater control off
    heaterThermalRunawayCounter = 0;
#ifdef TS101_DOG_UI
    dogAlert = DogAlert::Thermal;
    dogPressConsumed();
#endif
    cxt->transitionMode         = TransitionAnimation::Right;
    return OperatingMode::ThermalRunaway;
  }
  return handleSolderingButtons(buttons, cxt);
}

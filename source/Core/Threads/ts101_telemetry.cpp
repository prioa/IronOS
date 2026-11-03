/*
 * TS101: the telemetry block at 0x20000000, see ts101_telemetry.hpp.
 *
 * The PID thread writes it at 8 Hz under the seqlock, copying values that are worked out anyway. What only the GUI knows (the
 * screen's state, the solder, the view, the warnings, the tilt) the GUI thread leaves in a few bytes once a frame; the PID thread
 * copies them. Commands are handled in the GUI thread, through the same paths the buttons take, so every limit stays as it is:
 * the PID thread caps the set point, soldering mode keeps its sleep, shutdown, handle and runaway checks.
 */
#include "ts101_telemetry.hpp"
#ifdef TS101_TELEMETRY
#include "BSP.h"
#include "OLED.hpp"
#include "Settings.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ts101_dog_ui.hpp"
#include "ts101_v2_solder.hpp"

__attribute__((section(".telemetry"), used)) volatile Ts101Telemetry ts101Telemetry;

extern volatile uint8_t pendingPWM;              // BSP.cpp: the pulse of powerPWM the PID thread asked for
extern int32_t          powerSupplyWattageLimit; // the PD contract's watts, 0 without one
extern uint16_t         requested_voltage_mv;    // USBPD.cpp: the voltage asked for in the contract
extern TickType_t       lastMovementTime;
extern TickType_t       lastButtonTime;

#define TELEMETRY_PERIOD (TICKS_SECOND / 8)

// What the GUI thread knows, for the PID thread to copy
static volatile uint8_t guiState = (uint8_t)TelemetryState::Off, guiLot = 255, guiView = 0, guiWarn = 0;
static volatile int8_t  guiTilt  = 0;

static inline void barrier() { __asm volatile("dmb" ::: "memory"); }

void telemetryInit() {
  // The section is not zeroed by the startup: a reset leaves whatever was there, and the ESP32's last command must not run again
  volatile uint8_t *raw = (volatile uint8_t *)&ts101Telemetry;
  for (uint8_t i = 0; i < sizeof(Ts101Telemetry); i++) {
    raw[i] = 0;
  }
  ts101Telemetry.version   = TS101_TELEMETRY_VERSION;
  ts101Telemetry.size      = sizeof(Ts101Telemetry);
  ts101Telemetry.fb_addr   = (uint32_t)OLED::framebuffer();
  ts101Telemetry.fb_w      = OLED_WIDTH;
  ts101Telemetry.fb_h      = OLED_HEIGHT;
  ts101Telemetry.fb_layout = 0;
  barrier();
  ts101Telemetry.magic = TS101_TELEMETRY_MAGIC; // last: a reader that finds the magic finds the rest set up
}

void telemetryFrameSent() { ts101Telemetry.fb_seq = ts101Telemetry.fb_seq + 1; }

static inline uint32_t toMs(TickType_t ticks) {
#if TICKS_SECOND == 1000
  return ticks; // the TS101 ticks in milliseconds
#else
  return ticks * (1000 / TICKS_SECOND);
#endif
}

void telemetryUpdate(int16_t tipC, int32_t x10Watts) {
  (void)x10Watts;
  static TickType_t last = 0;
  const TickType_t  now  = xTaskGetTickCount();
  if (now - last < TELEMETRY_PERIOD) {
    return;
  }
  last = now;
  volatile Ts101Telemetry &t = ts101Telemetry;
  t.seq                      = t.seq + 1; // odd: being written
  barrier();
  t.uptime_ms      = toMs(now);
  t.tip_c          = tipC;
  t.target_c       = currentTempTargetDegC;
  t.handle_cx10    = getHandleTemperature(0);
  t.vin_mv         = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0) * 100;
  t.power_dw       = x10WattHistory.average();
  t.pwm_pct        = (uint32_t)pendingPWM * 100 / powerPWM;
  t.state          = guiState;
  t.lot            = guiLot;
  t.view           = guiView;
  t.tilt_deg       = guiTilt;
  t.warn           = guiWarn;
  t.last_move_ms   = toMs(lastMovementTime);
  t.last_button_ms = toMs(lastButtonTime);
  t.pd_mv          = powerSupplyWattageLimit > 0 ? requested_voltage_mv : 0;
  t.pd_max_w       = powerSupplyWattageLimit > 0 ? powerSupplyWattageLimit : 0;
  t.tip_raw_uv     = TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0));
  barrier();
  t.seq = t.seq + 1; // even: valid
}

// ---- GUI thread ----
static TelemetryState stateOf(OperatingMode mode, guiContext *cxt) {
  switch (mode) {
  case OperatingMode::Soldering:
    switch (dogSolderState(cxt->scratch_state.state2)) {
    case DogSolderState::Heating:
      return TelemetryState::Heating;
    case DogSolderState::Ready:
      return TelemetryState::Ready;
    case DogSolderState::Cooling:
      return TelemetryState::Cooling;
    case DogSolderState::Load:
      return TelemetryState::Load;
    case DogSolderState::Boost:
      return TelemetryState::Boost;
    case DogSolderState::Locked:
      return TelemetryState::Locked;
    }
    return TelemetryState::Heating;
  case OperatingMode::Sleeping:
  case OperatingMode::Hibernating:
    return TelemetryState::Sleep;
  case OperatingMode::HomeScreen:
    return dogAlert != DogAlert::None || isTipDisconnected() ? TelemetryState::Warning : TelemetryState::Off;
  default:
    return TelemetryState::Off; // the menu and the rest: the heater is off
  }
}

static void publish(OperatingMode mode, guiContext *cxt) {
  guiState           = (uint8_t)stateOf(mode, cxt);
  const int8_t index = dogPresetIndex();
  guiLot             = index < 0 ? 255 : index;
  guiView            = mode == OperatingMode::Soldering && dogSolderSensorsShown() ? 1 : 0;
  uint8_t warn       = 0;
  if (mode == OperatingMode::Soldering && dogSupplyWeak()) {
    warn |= TELEMETRY_WARN_SUPPLY;
  }
  if (dogAlert == DogAlert::Handle) {
    warn |= TELEMETRY_WARN_HANDLE;
  }
  if (isTipDisconnected()) {
    warn |= TELEMETRY_WARN_NO_TIP;
  }
  guiWarn       = warn;
  int16_t along = 0, turn = 0;
  if (dogIronLie(along, turn)) {
    guiTilt = along > 127 ? 127 : (along < -127 ? -127 : along);
  }
}

OperatingMode telemetryGui(OperatingMode current, OperatingMode next, guiContext *cxt) {
  publish(current, cxt);
  volatile Ts101Telemetry &t   = ts101Telemetry;
  const uint8_t            seq = t.cmd_seq;
  if (seq == t.cmd_ack) {
    return next;
  }
  barrier(); // cmd and cmd_arg were written before cmd_seq
  const uint8_t cmd = t.cmd, arg = t.cmd_arg;
  const bool    soldering = current == OperatingMode::Soldering || current == OperatingMode::Sleeping || current == OperatingMode::Hibernating;
  switch (cmd) {
  case TELEMETRY_CMD_LOT:
    if (arg < 5) {
      const uint16_t before = getSettingValue(SettingsOptions::SolderingTemp);
      dogSetPreset(arg); // one of the five presets, all below the maximum
      if (getSettingValue(SettingsOptions::SolderingTemp) != before) {
        if (current == OperatingMode::Soldering) {
          dogLotChangedAt = xTaskGetTickCount() | 1; // the screen shows the alloy, as after a click
        }
        dogSettingsChanged(); // saved on the home screen, as a click's change is
      }
    }
    break;
  case TELEMETRY_CMD_STOP:
    if (soldering || current == OperatingMode::SolderingProfile) {
      currentTempTargetDegC = 0;
      cxt->transitionMode   = TransitionAnimation::Right;
      next                  = OperatingMode::HomeScreen;
    }
    break;
  case TELEMETRY_CMD_START:
    // The same checks as the front button on the home screen: a tip, no alert, a handle below its limit
    if (current == OperatingMode::HomeScreen && !isTipDisconnected() && dogAlert == DogAlert::None && getHandleTemperature(0) < DOG_HANDLE_LIMIT_X10C) {
      lastButtonTime      = xTaskGetTickCount(); // counts as a press: soldering must not fall asleep at once
      cxt->transitionMode = TransitionAnimation::Left;
      next                = OperatingMode::Soldering;
    }
    break;
  default:
    break; // unknown: acknowledged, nothing done
  }
  t.cmd_ack = seq;
  return next;
}
#endif

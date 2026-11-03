/*
 * TS101_DOG_UI interface v2: the soldering screen, one for heating, ready, cooling, load and boost
 *
 *   Platine bleifrei            352°     A name of the solder              D the reading
 *   [HEIZT] 65W 4S             ↑ 350°    B state label and values          E direction and target
 *   ......curve.............   [-] [+]   C curve with the set line        F keys
 *
 * A shake swaps the left side (A-C) for a table of the sensors, with a wipe from left to right; the right side stays.
 */
#include "ts101_v2_solder.hpp"
#ifdef TS101_DOG_UI
#include "BSP.h"
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "power.hpp"
#include "ts101_v2_shake.hpp"
#include "ui_drawing.hpp"
#include <string.h>

extern volatile uint8_t pendingPWM; // BSP.cpp: the pulse the PID thread asked for, of powerPWM
extern int32_t          powerSupplyWattageLimit; // the PD contract in watts, 0 without one

#define READY_WITHIN    3                      // |reading - target| below this is at the set point
#define LOAD_ON_PCT     40                     // the set point was reached and the heater works this hard: under load
#define LOAD_OFF_PCT    30
#define LOT_SHOW_TICKS  (TICKS_SECOND * 2)     // after a change of solder its alloy shows in row 2
#define WIPE_STEP_TICKS (TICKS_SECOND / 8)     // 4 steps: 0.5 s
#define SWITCH_NORMAL   (TICKS_SECOND * 4)     // WECHSELN: this long the normal view, then
#define SWITCH_SENSORS  (TICKS_SECOND * 3)     // this long the sensors
#define ESTIMATE_MAX_S  300

struct DogSolder {
  const char *de, *en, *alloy;
};
static const DogSolder SOLDERS[] = {
    {"Empfindlich", "Sensitive", "SNBI"}, {"Platine mit Blei", "Leaded PCB", "SNPB"}, {"Platine bleifrei", "Lead-free PCB", "SAC305"},
    {"Dicke Pads", "Thick pads", "SNCU"},  {"Kabel + Masse", "Wires + mass", "MAX"},
};
#define SOLDER_COUNT (sizeof(SOLDERS) / sizeof(SOLDERS[0]))

// ---- Readings ----
static uint32_t voltsX10() { return getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0); }
static uint32_t wattsNow() { return (x10WattHistory.average() + 5) / 10; }
static uint32_t ampsX10() {
  const uint32_t v = voltsX10();
  return v ? (x10WattHistory.average() * 10 + v / 2) / v : 0;
}
uint8_t dogPwmPercent() { return (uint32_t)pendingPWM * 100 / powerPWM; }

static bool inF() { return getSettingValue(SettingsOptions::TemperatureInF); }

char *dogFormatX10(char *out, uint32_t x10, const char *unit) {
  out    = dogFormat(out, x10 / 10);
  *out++ = '.';
  *out++ = '0' + x10 % 10;
  strcpy(out, unit);
  return out + strlen(unit);
}

char *dogFormatUnit(char *out, int32_t value, const char *unit) {
  if (value < 0) {
    *out++ = '-';
    value  = -value;
  }
  out = dogFormat(out, value);
  strcpy(out, unit);
  return out + strlen(unit);
}

const char *dogSolderName() {
  const int8_t solder = dogPresetIndex();
  return solder < 0 ? DOG_T("Eigenes Ziel", "Own target") : (dogLangDE() ? SOLDERS[solder].de : SOLDERS[solder].en);
}

uint16_t dogBoostOffset() { return inF() ? 90 : 50; } // 50 C either way
uint16_t dogSolderTarget(bool boost) { return getSettingValue(SettingsOptions::SolderingTemp) + (boost ? dogBoostOffset() : 0); }

// Seconds until the set point at this rate, at least 1; 0 when the tip is not on its way there
static uint16_t secondsLeft(int32_t errorC, int32_t rateC) {
  if (rateC == 0 || (errorC > 0) != (rateC > 0)) {
    return 0;
  }
  const int32_t distance = errorC > 0 ? errorC : -errorC, speed = rateC > 0 ? rateC : -rateC;
  const int32_t seconds  = (distance + speed / 2) / speed;
  return seconds > ESTIMATE_MAX_S ? 0 : (seconds ? seconds : 1);
}

// ---- The state in row 2 ----
static bool loadOn = false;

DogSolderState dogSolderState(bool boost) {
  const int32_t error = (int32_t)TipThermoModel::getTipInC() - (int32_t)currentTempTargetDegC;
  const uint8_t pwm   = dogPwmPercent();
  if (!dogSolderTimerStart || boost) {
    loadOn = false;
  } else if (pwm > LOAD_ON_PCT) {
    loadOn = true;
  } else if (pwm < LOAD_OFF_PCT) {
    loadOn = false;
  }
  if (dogLocked()) {
    return DogSolderState::Locked;
  }
  if (boost) {
    return DogSolderState::Boost;
  }
  if (loadOn) {
    return DogSolderState::Load;
  }
  if (error > -READY_WITHIN && error < READY_WITHIN) {
    return DogSolderState::Ready;
  }
  return error > 0 ? DogSolderState::Cooling : DogSolderState::Heating;
}

static void drawStateRow(DogSolderState state) {
  char        values[16];
  const char *label = "";
  char       *p     = values;
  *p                = 0;
  switch (state) {
  case DogSolderState::Locked:
    label = DOG_T("GESPERRT", "LOCKED");
    break;
  case DogSolderState::Boost: {
    label = "BOOST";
    *p++  = '+';
    p     = dogFormatUnit(p, dogBoostOffset(), "° ");
    dogFormatUnit(p, dogBoostSecondsLeft(), "S");
    break;
  }
  case DogSolderState::Heating: {
    label                = DOG_T("HEIZT", "HEATING");
    p                    = dogFormatUnit(p, wattsNow(), "W");
    const uint16_t ready = secondsLeft((int32_t)currentTempTargetDegC - (int32_t)TipThermoModel::getTipInC(), dogCurveRateC());
    if (ready) {
      *p++ = ' ';
      dogFormatUnit(p, ready, "S");
    }
    break;
  }
  case DogSolderState::Ready:
    label = DOG_T("BEREIT", "READY");
    dogFormatUnit(p, wattsNow(), "W");
    break;
  case DogSolderState::Cooling:
    label = DOG_T("KÜHLT", "COOLING");
    dogFormatUnit(p, wattsNow(), "W");
    break;
  case DogSolderState::Load:
    label = DOG_T("LAST", "LOAD");
    dogFormatUnit(p, wattsNow(), "W");
    break;
  }
  gridState(label, values);
}

// Right after a change of solder: its alloy and five boxes, the current one filled
static void drawLotRow(int8_t solder) {
  const char *alloy = SOLDERS[solder].alloy;
  gridValues(0, alloy);
  gridClip(0, GRID_LEFT_END);
  const int16_t x0 = dogTextWidth(dogFontName, alloy) + 4;
  for (uint8_t i = 0; i < SOLDER_COUNT; i++) {
    const int16_t x = x0 + i * 7;
    if ((int8_t)i == solder) {
      gridFill(x, 14, x + 4, 18);
    } else {
      gridFill(x, 14, x + 4, 14);
      gridFill(x, 18, x + 4, 18);
      gridFill(x, 14, x, 18);
      gridFill(x + 4, 14, x + 4, 18);
    }
  }
  gridClipAll();
}

static bool lotJustChanged() { return dogLotChangedAt && xTaskGetTickCount() - dogLotChangedAt < LOT_SHOW_TICKS; }

// ---- The two left sides ----
static const char *warning = nullptr; // a warning for row 2 this frame (dogSolderWarning)
void               dogSolderWarning(const char *text) { warning = text; }

static void drawNormalSide(bool boost) {
  gridName(dogSolderName());
  const int8_t solder = dogPresetIndex();
  if (warning) {
    gridWarn(warning);
  } else if (lotJustChanged() && solder >= 0 && !boost) {
    drawLotRow(solder);
  } else {
    drawStateRow(dogSolderState(boost));
  }
  gridCurve(true, currentTempTargetDegC);
}

static void cell(uint8_t col, uint8_t row, const char *label, const char *value) {
  const int16_t x0 = col ? 49 : 0, x1 = col ? GRID_LEFT_END : 44, y = row * 11;
  gridClip(x0, x1);
  dogText(dogFont3x5, x0, y + 8, label);
  gridTextRight(dogFontName, x1, y + 8, value);
  gridClipAll();
}

void dogDrawSensorSide() {
  gridClip(0, GRID_LEFT_END);
  gridDottedRow(0, GRID_LEFT_END, 9, 2);
  gridDottedRow(0, GRID_LEFT_END, 20, 2);
  gridDottedColumn(46, 0, OLED_HEIGHT - 1, 2);
  gridClipAll();
  char text[10];
  dogFormatX10(text, voltsX10(), "V");
  cell(0, 0, DOG_T("EIN", "IN"), text);
  dogFormatX10(text, ampsX10(), "A");
  cell(1, 0, DOG_T("STROM", "AMPS"), text);
  dogFormatUnit(text, wattsNow(), "W");
  cell(0, 1, DOG_T("LEIST", "POWER"), text);
  dogFormatUnit(text, dogPwmPercent(), "%");
  cell(1, 1, "PWM", text);
  int32_t handle = getHandleTemperature(0) / 10;
  if (inF()) {
    handle = handle * 9 / 5 + 32;
  }
  dogFormatUnit(text, handle, "°");
  cell(0, 2, DOG_T("GRIFF", "GRIP"), text);
  int16_t along = 0, turn = 0;
  if (dogIronLie(along, turn)) {
    dogFormatUnit(text, along, "°");
  } else {
    strcpy(text, "-");
  }
  cell(1, 2, DOG_T("LAGE", "TILT"), text);
}

// ---- Which side shows, and the wipe between them ----
static bool       sensorsShown = false; // the side shown once a wipe is over
static bool       wipeFrom     = false; // the side the wipe leaves
static TickType_t wipeStart    = 0;
static bool       wiping       = false;
static TickType_t switchEpoch  = 0;

bool dogSolderSensorsShown() { return sensorsShown; }

void dogSolderViewReset() {
  sensorsShown = getSettingValue(SettingsOptions::DogSolderView) && !getSettingValue(SettingsOptions::DogSensorMode);
  wiping       = false;
  switchEpoch  = xTaskGetTickCount();
  dogShakeTaken(); // a shake from before does not count here
}

static bool wantedSensors(bool boost, TickType_t now) {
  const bool shook = dogShakeTaken(); // read every frame, so a shake during boost is dropped and not kept for later
  if (getSettingValue(SettingsOptions::DogSensorMode)) {
    if (boost || lotJustChanged() || warning) {
      switchEpoch = now; // the cycle starts again with the normal view afterwards
      return false;
    }
    return (now - switchEpoch) % (SWITCH_NORMAL + SWITCH_SENSORS) >= SWITCH_NORMAL;
  }
  if (shook && !boost) {
    setSettingValue(SettingsOptions::DogSolderView, !getSettingValue(SettingsOptions::DogSolderView));
    dogSettingsChanged(); // saved on the home screen
  }
  if (boost || lotJustChanged() || warning) {
    return false; // shown briefly, then back to the picked view
  }
  return getSettingValue(SettingsOptions::DogSolderView);
}

static void drawSide(bool sensors, bool boost) {
  if (sensors) {
    dogDrawSensorSide();
  } else {
    drawNormalSide(boost);
  }
}

// Not critical, it heats on: the PD contract gives less than the tip would draw at the voltage there is (P = V^2 / R)
bool dogSupplyWeak() { return powerSupplyWattageLimit > 0 && powerSupplyWattageLimit * 10 < (int32_t)availableW10(0); }

static void checkSupply() {
  static char text[16];
  if (!warning && dogSupplyWeak()) {
    dogFormatUnit(text, powerSupplyWattageLimit, "W");
    char row[24];
    strcpy(row, DOG_T("! NETZTEIL ", "! SUPPLY "));
    strcat(row, text);
    strcpy(text, row);
    warning = text;
  }
}

void dogDrawSolderScreen(bool boost) {
  const TickType_t now = xTaskGetTickCount();
  checkSupply();
  dogFastSensorUntil   = now + TICKS_SECOND / 2; // the sensor every 40 ms, for the shake
  const bool wanted    = wantedSensors(boost, now);
  if (wanted != sensorsShown) {
    wipeFrom     = sensorsShown;
    sensorsShown = wanted;
    wipeStart    = now;
    wiping       = true;
  }
  const uint8_t step = wiping ? (now - wipeStart) / WIPE_STEP_TICKS : 4;
  if (step < 4) {
    static const uint8_t CUT[4] = {0, 23, 46, 69};
    const int16_t        cut    = CUT[step];
    gridOuter(cut + 1, GRID_NAME_END);
    drawSide(wipeFrom, boost);
    if (cut > 0) {
      gridOuter(0, cut - 1);
      drawSide(sensorsShown, boost);
    }
    gridOuter(0, OLED_WIDTH - 1);
    gridFill(cut, 0, cut, OLED_HEIGHT - 1);
  } else {
    wiping = false;
    drawSide(sensorsShown, boost);
  }
  warning = nullptr;

  // The right side
  gridTemp(getTipTemp());
  const int32_t error = (int32_t)TipThermoModel::getTipInC() - (int32_t)currentTempTargetDegC;
  gridTarget(error > -READY_WITHIN && error < READY_WITHIN ? 0 : (error < 0 ? 1 : -1), dogSolderTarget(boost));
  if (dogLocked()) {
    gridKeys(&iconV2Lock, &iconV2Lock);
  } else if (boost) {
    gridKeys(&iconV2Stop, &iconV2Boost, false, true);
  } else {
    gridKeys(&iconV2Minus, &iconV2Plus);
  }
}

void ui_draw_soldering_basic_status(bool boostModeOn) { dogDrawSolderScreen(boostModeOn); }
void ui_draw_soldering_power_status(bool boostModeOn) { dogDrawSolderScreen(boostModeOn); }
#endif

// Fake sensor/settings state for the host preview, plus the list of scenes to render
#define private public // the preview flips the orientation directly
#include "OLED.hpp"
#undef private
#include "preview_state.hpp"
#include "BSP.h"
#include "Buttons.hpp"
#include "OperatingModes.h"
#include "Settings.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ts101_dog_ui.hpp"
#include "ts101_v2_solder.hpp"
#include "ui_drawing.hpp"

static uint16_t settings[SettingsOptionsLength];
static uint16_t tipC = 320, vX10 = 201;
static bool     tipMissing = false;
static bool     pinFront = false, pinRear = false;
static int16_t  handleX10 = 310;
static void     orient(bool frontRight);

volatile TemperatureType_t                    currentTempTargetDegC = 320;
expMovingAverage<uint32_t, wattHistoryFilter> x10WattHistory;
#ifndef BUTTON_TEST // from Buttons.cpp / SolderingCommon.cpp there
TickType_t                                    lastButtonTime   = 0;
#endif
TickType_t                                    lastMovementTime = 0;

uint16_t          getSettingValue(const enum SettingsOptions option) { return settings[option]; }
void              setSettingValue(const enum SettingsOptions option, const uint16_t v) { settings[option] = v; }
void              saveSettings() {}
// Wraps like the real one for the two-state settings the menu shows as switches; others only count up
void nextSettingValue(const enum SettingsOptions o) {
  if (o == SettingsOptions::LockingMode) { // the same for the lock's three values
    settings[o] = (settings[o] + 1) % 3;
    return;
  }
  const bool twoStates = o == SettingsOptions::OLEDInversion || o == SettingsOptions::TemperatureInF || o == SettingsOptions::ReverseButtonTempChangeEnabled || o == SettingsOptions::CalibrateCJC || o == SettingsOptions::DogSolderView || o == SettingsOptions::DogSensorMode;
  settings[o]          = (twoStates && settings[o] >= 1) ? 0 : settings[o] + 1;
}
void              prevSettingValue(const enum SettingsOptions o) { settings[o]--; }
bool              isLastSettingValue(const enum SettingsOptions) { return false; }
void              dogSetCalibrateCJC(void) {}
void              dogSetCalibrateVIN(void) {}
void              dogSetResetSettings(void) {}
TemperatureType_t TipThermoModel::getTipInC(bool) { return tipC; }
TemperatureType_t TipThermoModel::getTipInF(bool) { return tipC * 9 / 5 + 32; }
TemperatureType_t TipThermoModel::convertFtoC(TemperatureType_t f) { return (f - 32) * 5 / 9; }
#ifndef BUTTON_TEST // from Buttons.cpp / SolderingCommon.cpp there
TemperatureType_t getTipTemp(void) { return settings[SettingsOptions::TemperatureInF] ? tipC * 9 / 5 + 32 : tipC; }
#endif
uint16_t          getInputVoltageX10(uint16_t, uint8_t) { return vX10; }
int16_t           getHandleTemperature(uint8_t) { return handleX10; }
uint8_t           getTipResistanceX10() { return 62; }
bool              isTipDisconnected() { return tipMissing; }
bool              getIsPoweredByDCIN() { return false; }
#ifndef BUTTON_TEST // from Soldering.cpp there
static uint8_t testBoostSeconds = 30;
uint8_t        dogBoostSecondsLeft() { return testBoostSeconds; }
#endif
#ifndef BUTTON_TEST // from Buttons.cpp / SolderingCommon.cpp there
int8_t            getPowerSourceNumber(void) { return 3; }
#endif
uint32_t          getSleepTimeout(void) { return 60 * TICKS_SECOND; }
static int32_t    limitX10 = 1000;
int32_t           getX10WattageLimits() { return limitX10; }
uint8_t           X10WattsToPWM(int32_t w, uint8_t) { return w > 255 ? 255 : w; }
#ifdef BUTTON_TEST
// test_buttons.cpp drives the real button code through the pins
void testSetPins(bool front, bool rear) { pinFront = front, pinRear = rear; }
void testSetHandle(int16_t x10C) { handleX10 = x10C; }
void testSetTip(uint16_t c) { tipC = c; }
#else
ButtonState       getButtonState() { return BUTTON_NONE; }
#endif
uint8_t           getButtonA() { return pinFront; }
uint8_t           getButtonB() { return pinRear; }
bool              I2CBB1::I2C_RegisterWrite(uint8_t, uint8_t, uint8_t) { return true; }
bool              I2CBB1::Mem_Write(uint16_t, uint16_t, const uint8_t *, uint16_t) { return true; }

void previewSetup() {
  settings[SettingsOptions::VoltageDiv]         = 700;
  settings[SettingsOptions::BoostTemp]          = 420;
  settings[SettingsOptions::TotalSolderMinutes] = 42 * 60 + 17;
  ui_pre_render_assets();
}

// The named button has been down for ms milliseconds, as dogButtonsPoll saw it
static void pressed(bool front, uint32_t ms) {
  pinFront = pinRear = false;
  dogButtonsPoll(BUTTON_NONE);
  previewTicks -= ms;
  (front ? pinFront : pinRear) = true;
  dogButtonsPoll(BUTTON_NONE);
  previewTicks += ms;
  dogButtonsPoll(BUTTON_NONE);
}

// The TS101 has OLED_FLIP set, so getRotation() is the opposite of this flag; true is the mock's normal layout
static void orient(bool frontRight) { OLED::inLeftHandedMode = !frontRight; }

void previewApply(const PreviewScene &s) {
  pinFront = pinRear = false;
  dogButtonsPoll(BUTTON_NONE);
  orient(true);
  limitX10 = 1000;
  lastMovementTime = lastButtonTime = 0;
  settings[SettingsOptions::Sensitivity] = 6;
  settings[SettingsOptions::SleepTime]   = 1;
  settings[SettingsOptions::SolderingTipType] = 0;
  settings[SettingsOptions::SleepTemp]          = 150;
  settings[SettingsOptions::TempChangeLongStep] = 10;
  settings[SettingsOptions::ShutdownTime]       = 10;
  settings[SettingsOptions::OLEDBrightness]     = 26;
  tipC                                         = s.tipC;
  settings[SettingsOptions::SolderingTemp]     = s.setC;
  currentTempTargetDegC                        = s.setC;
  x10WattHistory.sum                           = s.x10W;
  vX10                                         = s.vX10;
  settings[SettingsOptions::DetailedSoldering] = s.detailed;
  settings[SettingsOptions::DetailedIDLE]      = s.detailed;
  tipMissing                                   = false;
  dogSolderTimerStart = dogReachedUntil = 0;
  dogWokeHeating                                 = false;
  dogLotChangedAt                                = 0;
  dogSetLocked(false);
  settings[SettingsOptions::DogSensorMode]       = 0;
  settings[SettingsOptions::DogSolderView]       = 0;
  handleX10                                      = 310;
  settings[SettingsOptions::TotalSolderMinutes]  = 42 * 60 + 17;
  settings[SettingsOptions::TotalSolderSessions] = 214;
  settings[SettingsOptions::TotalBoosts]         = 17;
  dogSessionMinutes                              = 38;
  dogHeatupStartTempC                                                     = 25;
  dogLastSessionSeconds                                                   = 27 * 60 + 12;
}

// The set point was reached this many seconds ago
static void atTemp(uint32_t seconds) { dogSolderTimerStart = (previewTicks - seconds * 1000 - 1) | 1; }

#ifndef BUTTON_TEST
static void setBoostPreview(uint8_t s) { testBoostSeconds = s; }
#else
static void setBoostPreview(uint8_t) {}
#endif

// ---- Interface v2 ----
volatile uint8_t pendingPWM              = 0;
const uint16_t   powerPWM                = 255;
int32_t          powerSupplyWattageLimit = 65;

static void setAngle(int degrees) { // the iron's long axis against the horizon, tip down positive; 1 g = 8000
  const double a = degrees * 3.14159265 / 180;
  dogAccelRaw[0] = (int16_t)(8000 * sin(a));
  dogAccelRaw[1] = (int16_t)(-8000 * cos(a));
  dogAccelRaw[2] = 0;
}

static void pwm(uint8_t percent) { pendingPWM = percent * 255 / 100; }

// The curve at 8 samples a second up to the scene's time: from start through each step's temperature over its samples
struct CurveStep {
  int temp, samples;
};
static void curve(int start, std::initializer_list<CurveStep> steps) {
  int total = 0;
  for (const CurveStep &s : steps) {
    total += s.samples;
  }
  const uint32_t end = previewTicks;
  int            at = 0, from = start;
  dogCurveReset();
  previewTicks = end - total * 125;
  dogCurveSample(start);
  for (const CurveStep &s : steps) {
    for (int i = 1; i <= s.samples; i++) {
      at++;
      previewTicks = end - (total - at) * 125;
      // a little ripple, as the real reading has
      dogCurveSample(from + (s.temp - from) * i / s.samples + ((at * 7) % 5 == 0 ? 1 : 0));
    }
    from = s.temp;
  }
  previewTicks = end;
}

// The soldering screen at the scene's time; the view as the setting says, no wipe
static void solder(bool boost = false) {
  dogSolderViewReset();
  ui_draw_soldering_basic_status(boost);
}

// One of the wipe's four frames from the normal view to the sensors
static void wipeFrame(uint8_t step) {
  settings[SettingsOptions::DogSolderView] = 0;
  dogSolderViewReset();
  ui_draw_soldering_basic_status(false);
  settings[SettingsOptions::DogSolderView] = 1;
  const uint32_t start = previewTicks;
  ui_draw_soldering_basic_status(false); // the wipe starts here
  previewTicks = start + step * 125;
  OLED::clearScreen();
  ui_draw_soldering_basic_status(false);
  previewTicks = start;
  settings[SettingsOptions::DogSolderView] = 0;
}

// The screen saver after this many frames since falling asleep, played frame by frame as the iron does
static void sleepAt(uint32_t frames) {
  const uint32_t end = previewTicks;
  previewTicks       = end - frames * 125;
  lastMovementTime = lastButtonTime = end - 60 * 1000 - 23 * 1000; // asleep after a minute, the iron switches off after 10
  for (uint32_t i = 0; i <= frames; i++) {
    previewTicks = end - (frames - i) * 125;
    OLED::clearScreen();
    ui_draw_soldering_basic_sleep(150);
  }
}

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 600000; // 10 min after power on
  return {
      // name, ticks, tip, set, x10 W, x10 V, -, draw
      {"s01_hereinlaufen", T, 150, 150, 0, 201, false, [] { sleepAt(14); }},
      {"s02_abbremsen", T, 150, 150, 0, 201, false, [] { sleepAt(33); }},
      {"s03_sitzen", T, 150, 150, 0, 201, false, [] { sleepAt(40); }},
      {"s04_zzz1", T, 150, 150, 0, 201, false, [] { sleepAt(44); }},
      {"s05_zzz3", T, 150, 150, 0, 201, false, [] { sleepAt(51); }},
      {"s06_hocken", T, 150, 150, 0, 201, false, [] { sleepAt(79); }},
      {"s07_haeufchen4", T, 150, 150, 0, 201, false, [] { sleepAt(85); }},
      {"s08_weiterlaufen", T, 150, 150, 0, 201, false, [] { sleepAt(97); }},
      {"s09_hinaus", T, 150, 150, 0, 201, false, [] { sleepAt(118); }},
      {"s10_runde2_herein", T, 150, 150, 0, 201, false, [] { sleepAt(170); }},
      {"s11_runde2_doesen", T, 150, 150, 0, 201, false, [] { sleepAt(205); }},
      {"s12_runde2_haeufchen", T, 150, 150, 0, 201, false, [] { sleepAt(236); }},
      {"s13_runde2_hinaus", T, 150, 150, 0, 201, false, [] { sleepAt(265); }},
      {"h01_aus", T, 24, 350, 0, 201, false,
       [] {
         curve(24, {{24, 93}});
         ui_draw_homescreen_simplified(24);
       }},
      {"h02_heiss", T, 183, 350, 0, 201, false,
       [] {
         curve(350, {{350, 30}, {183, 63}});
         ui_draw_homescreen_simplified(183);
       }},
      {"h04_menue_gedrueckt", T, 24, 350, 0, 201, false,
       [] {
         curve(24, {{24, 93}});
         pressed(false, 100);
         ui_draw_homescreen_simplified(24);
       }},
      {"h03_sperre_an", T, 24, 350, 0, 201, false,
       [] {
         settings[SettingsOptions::LockingMode] = 1;
         curve(24, {{24, 93}});
         dogHomeLockNotice(true);
         ui_draw_homescreen_simplified(24);
       }},
      {"l01_heizt", T, 212, 350, 650, 201, false,
       [] {
         curve(30, {{212, 60}});
         pwm(100);
         solder();
       }},
      {"l02_bereit", T, 349, 350, 120, 201, false,
       [] {
         curve(30, {{350, 40}, {350, 53}});
         atTemp(8);
         pwm(18);
         solder();
       }},
      {"l03_kuehlt", T, 368, 350, 0, 201, false,
       [] {
         settings[SettingsOptions::SolderingTemp] = 350;
         curve(372, {{372, 40}, {368, 53}});
         atTemp(30);
         pwm(0);
         solder();
       }},
      {"l04_last", T, 338, 350, 480, 201, false,
       [] {
         curve(350, {{350, 50}, {336, 12}, {340, 31}});
         atTemp(60);
         pwm(74);
         solder();
       }},
      {"l05_boost", T, 381, 400, 650, 201, false,
       [] {
         settings[SettingsOptions::SolderingTemp] = 350;
         currentTempTargetDegC                    = 400;
         curve(350, {{350, 60}, {381, 33}});
         atTemp(60);
         pwm(100);
         setBoostPreview(23);
         pressed(true, 2000);
         solder(true);
       }},
      {"l06_gesperrt", T, 350, 350, 110, 201, false,
       [] {
         curve(350, {{350, 93}});
         atTemp(60);
         pwm(17);
         dogSetLocked(true);
         solder();
         dogSetLocked(false);
       }},
      {"l07_lotwechsel", T, 352, 370, 650, 201, false,
       [] {
         curve(350, {{350, 85}, {352, 8}});
         atTemp(60);
         pwm(100);
         dogLotChangedAt = (previewTicks - 600) | 1;
         solder();
       }},
      {"l08_eigenes_ziel", T, 300, 300, 130, 201, false,
       [] {
         curve(300, {{300, 93}});
         atTemp(60);
         pwm(20);
         solder();
       }},
      {"l09_sensoren", T, 350, 350, 650, 201, false,
       [] {
         curve(350, {{350, 93}});
         atTemp(60);
         pwm(100);
         handleX10 = 342;
         setAngle(12);
         settings[SettingsOptions::DogSolderView] = 1;
         solder();
       }},
      {"l10_wisch1", T, 350, 350, 650, 201, false, [] { curve(350, {{350, 93}}); atTemp(60); pwm(100); handleX10 = 342; setAngle(12); wipeFrame(0); }},
      {"l11_wisch2", T, 350, 350, 650, 201, false, [] { curve(350, {{350, 93}}); atTemp(60); pwm(100); handleX10 = 342; setAngle(12); wipeFrame(1); }},
      {"l12_wisch3", T, 350, 350, 650, 201, false, [] { curve(350, {{350, 93}}); atTemp(60); pwm(100); handleX10 = 342; setAngle(12); wipeFrame(2); }},
      {"l13_wisch4", T, 350, 350, 650, 201, false, [] { curve(350, {{350, 93}}); atTemp(60); pwm(100); handleX10 = 342; setAngle(12); wipeFrame(3); }},
      {"l14_minus_gedrueckt", T, 350, 350, 110, 201, false,
       [] {
         curve(350, {{350, 93}});
         atTemp(60);
         pwm(17);
         pressed(false, 100);
         solder();
       }},
  };
}

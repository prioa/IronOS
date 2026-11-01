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
  const bool twoStates = o == SettingsOptions::OLEDInversion || o == SettingsOptions::TemperatureInF || o == SettingsOptions::ReverseButtonTempChangeEnabled || o == SettingsOptions::CalibrateCJC || o == SettingsOptions::DogSolderView;
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
TemperatureType_t getTipTemp(void) { return tipC; }
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
  settings[SettingsOptions::TotalSolderMinutes]  = 42 * 60 + 17;
  settings[SettingsOptions::TotalSolderSessions] = 214;
  settings[SettingsOptions::TotalBoosts]         = 17;
  dogSessionMinutes                              = 38;
  dogHeatupStartTempC                                                     = 25;
  dogLastSessionSeconds                                                   = 27 * 60 + 12;
}

// The same simulated tip temperatures as the mock
enum class Series { Steady, Dip, Drop, Heat, Boost, Set350, Load, Limited, Set240, Set335, Set370, Set400 };
static void fillSeries(Series kind, uint32_t spacing = 250) {
  dogCurveReset();
  const uint32_t now = previewTicks;
  for (int i = 0; i < 93; i++) {
    int t = 320;
    switch (kind) {
    case Series::Set350: t = 350 + (i % 23 == 5 ? -1 : 0); break;
    case Series::Set240: t = 240; break;
    case Series::Set335: t = 335; break;
    case Series::Set370: t = 370 + (i % 23 == 5 ? -1 : 0); break;
    case Series::Set400: t = i < 60 ? 400 : 400 - (i - 60) % 8; break;
    case Series::Load: { // the dip on the board: rows below the set line, by column
      const int x = i * 78 / 93;
      const int y = x < 57 ? 0 : x < 58 ? 1 : x < 59 ? 2 : x < 60 ? 3 : x < 61 ? 4 : x < 65 ? 5 : x < 67 ? 4 : x < 69 ? 3 : x < 73 ? 2 : 1;
      t = 350 - y * 45 / 7;
      break;
    }
    case Series::Limited: t = 49 + i * 119 / 92; break;
    case Series::Steady: t = 320 + (i % 17 == 0 ? 1 : 0); break;
    case Series::Dip: t = (i < 40 || i >= 60) ? 320 : (i < 46 ? 320 - (i - 40) * 5 : 290 + (i - 46) * 2); break;
    case Series::Drop: t = i < 52 ? 320 : (320 - (i - 52) * 4 > 288 ? 320 - (i - 52) * 4 : 288); break;
    case Series::Heat: t = 25 + i * 155 / 92; break;
    case Series::Boost: t = i < 40 ? 320 : (320 + (i - 40) * 4 < 410 ? 320 + (i - 40) * 4 : 410); break;
    }
    previewTicks = now - (92 - i) * spacing;
    dogCurveSample(t, spacing);
  }
  previewTicks = now;
}

// The set point was reached this many seconds ago
static void atTemp(uint32_t seconds) { dogSolderTimerStart = (previewTicks - seconds * 1000 - 1) | 1; }

#ifndef BUTTON_TEST
static void setBoostPreview(uint8_t s) { testBoostSeconds = s; }
#else
static void setBoostPreview(uint8_t) {}
#endif

// ---- Home screen ----
enum class HomeCurve { Cold, Heating, Cooling };

// The last 10 minutes through dogHomeTick, ending at the scene's time and tip temperature
static void feedHistory(HomeCurve kind, uint16_t endC) {
  const uint32_t end = previewTicks;
  dogHomeHistoryReset();
  for (int age = 610; age >= 0; age--) {
    int t = 26;
    if (age < 560 && age >= 540) {
      t = 26 + (560 - age) * 15; // heating up
    } else if (age < 540 && age >= 300) {
      t = 326 + (age / 40 % 3 == 0 ? -25 : 0); // soldering, with dips under load
    } else if (age < 300) {
      t = 26 + (int)(300 * exp(-(300 - age) / 45.0)); // cooling down
    }
    if (kind == HomeCurve::Cooling && age < 40) {
      t = endC + age * 2;
    }
    if (kind == HomeCurve::Heating && age < 40) {
      t = endC > (uint32_t)age * 8 ? endC - age * 8 : 26;
    }
    tipC         = t;
    previewTicks = end - age * 1000;
    dogHomeTick();
  }
  previewTicks = end;
  tipC         = endC;
}

static void setAccel(int16_t x, int16_t y, int16_t z) {
  dogAccelRaw[0] = x;
  dogAccelRaw[1] = y;
  dogAccelRaw[2] = z;
}

// The iron has lain still for a moment: the dog at rest, before its first blink. gravity 1 g = 8000
static void calm() {
  const uint32_t end = previewTicks;
  dogHomeMotionReset();
  setAccel(0, -8000, 0);
  for (int i = 8; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
  }
}

// Still long enough for the first blink to be under way
static void aboutToBlink() {
  const uint32_t end = previewTicks;
  previewTicks       = end - 3100;
  dogHomeMotionReset();
  setAccel(0, -8000, 0);
  for (int i = 0; previewTicks < end; i++) {
    dogHomeMotionUpdate();
    previewTicks += 125;
  }
  previewTicks = end;
  dogHomeMotionUpdate();
}

// Calm, then picked up: a clear change of the reading, far from a shake
static void pickedUp() {
  const uint32_t end = previewTicks;
  previewTicks       = end - 125;
  calm();
  previewTicks = end;
  setAccel(2800, -7500, 0);
  dogHomeMotionUpdate();
}

// Calm, then one strong reading; frame 1-4 of the hop
static void shaken(int frame) {
  const uint32_t end = previewTicks;
  previewTicks       = end - frame * 125;
  calm();
  setAccel(0, -16000, 0);
  for (int i = frame - 1; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
    setAccel(0, -8000, 0);
  }
}

static void home() {
  feedHistory(HomeCurve::Cold, tipC);
  calm();
  ui_draw_homescreen_simplified(tipC);
}

static void homeStatistics(uint16_t minutesTotal, uint16_t counts, uint16_t session) {
  settings[SettingsOptions::TotalSolderMinutes] = minutesTotal;
  settings[SettingsOptions::TotalSolderSessions] = settings[SettingsOptions::TotalBoosts] = counts;
  dogSessionMinutes = session;
  feedHistory(HomeCurve::Heating, 350); // the widest temperature and the arrow: the tightest the top row gets
  calm();
  ui_draw_homescreen_simplified(350);
}


// Heating from 25 C at 4 samples a second up to this temperature; frame picks the legs' phase
static void heatingScene(uint16_t now, int32_t x10W, int frame) {
  limitX10 = 650;
  dogCurveReset();
  const uint32_t end   = previewTicks;
  const int      count = (now - 25) / 7 + 1; // ~28 C a second at 65 W
  for (int i = 0; i < count; i++) {
    previewTicks = end - (count - 1 - i) * 250;
    dogCurveSample(25 + (now - 25) * i / (count > 1 ? count - 1 : 1), 250);
  }
  previewTicks = end;
  x10WattHistory.sum = x10W;
  // the legs run on the time: step through the frames at the scene's power
  const uint32_t fps = x10W * 20 / 650 ? x10W * 20 / 650 : 1;
  for (int i = 0; i <= frame; i++) {
    previewTicks = end - (frame - i) * (1000 / fps);
    ui_draw_soldering_basic_status(false);
    OLED::clearScreen();
  }
  previewTicks = end;
  ui_draw_soldering_basic_status(false);
}

// At temperature; changedMs ago a click changed the solder (0: none)
static void solderScene(uint16_t tip, uint16_t set, Series kind, uint32_t changedMs) {
  limitX10 = 650;
  const uint32_t end = previewTicks;
  previewTicks       = end - changedMs;
  fillSeries(kind);
  for (uint32_t t = 250; t <= changedMs; t += 250) { // after the change the tip is on its way
    previewTicks = end - changedMs + t;
    dogCurveSample(tip, 250);
  }
  previewTicks = end;
  atTemp(199);
  dogLotChangedAt = changedMs ? (previewTicks - changedMs) | 1 : 0;
  ui_draw_soldering_basic_status(false);
}

// A weak supply: the curve is full long before the set point. Rises from 25 C to this temperature over 93 samples
static void slowHeatScene(uint16_t now, int frame) {
  limitX10 = 300;
  dogCurveReset();
  const uint32_t end = previewTicks;
  for (int i = 0; i < 93; i++) {
    previewTicks = end - (92 - i) * 250;
    dogCurveSample(25 + (now - 25) * i / 92, 250);
  }
  x10WattHistory.sum = 300;
  for (int i = 0; i <= frame; i++) { // the legs run on the time
    previewTicks = end - (frame - i) * 110;
    ui_draw_soldering_basic_status(false);
    OLED::clearScreen();
  }
  previewTicks = end;
  ui_draw_soldering_basic_status(false);
}

static void boostScene(uint8_t secondsLeft) {
  limitX10 = 650;
  settings[SettingsOptions::BoostTemp] = 420;
  setBoostPreview(secondsLeft);
  atTemp(199);
  ui_draw_soldering_basic_status(true);
}

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  return {
      // The home screen: H0 is on the statistics' first phase, each phase lasts 2 s
#define H0 6000000
      {"h1_ruhig", H0, 26, 350, 0, 201, false, [] { home(); }},
      {"h2_blinzelt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); aboutToBlink(); ui_draw_homescreen_simplified(26); }},
      {"h3_aufmerksam", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); pickedUp(); ui_draw_homescreen_simplified(26); }},
      {"h3_aufmerksam_gedreht", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); pickedUp(); ui_draw_homescreen_simplified(26); }},
      {"h4_hopser_1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(1); ui_draw_homescreen_simplified(26); }},
      {"h4_hopser_2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(2); ui_draw_homescreen_simplified(26); }},
      {"h4_hopser_3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(3); ui_draw_homescreen_simplified(26); }},
      {"h4_hopser_4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(4); ui_draw_homescreen_simplified(26); }},
      {"h10_hinten_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(false, 100); ui_draw_homescreen_simplified(26); }},
      {"h10_vorne_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      // Heating up, the first time to the set point: the dog runs up the curve, its legs as fast as the power allows
      {"k1_heizt_65w", T, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 0); }},
      {"k1_heizt_65w_bild1", T + 50, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 1); }},
      {"k1_heizt_65w_bild2", T + 100, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 2); }},
      {"k1_heizt_65w_bild3", T + 150, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 3); }},
      {"k2_heizt_start", T, 41, 350, 650, 201, false, [] { heatingScene(41, 650, 0); }},
      {"k3_heizt_fast_da", T, 338, 350, 210, 201, false, [] { heatingScene(338, 210, 0); }},
      {"k4_heizt_gedreht", T, 205, 350, 650, 201, false, [] { orient(false); heatingScene(205, 650, 0); }},
      {"k6_lang_mitte", T, 200, 350, 300, 201, false, [] { slowHeatScene(200, 0); }},
      {"k6_lang_mitte_bild1", T, 200, 350, 300, 201, false, [] { slowHeatScene(200, 1); }},
      {"k7_lang_fast_da", T, 338, 350, 300, 201, false, [] { slowHeatScene(338, 0); }},
      {"k7_lang_fast_da_bild1", T, 338, 350, 300, 201, false, [] { slowHeatScene(338, 1); }},
      {"k8_lang_gedreht", T, 338, 350, 300, 201, false, [] { orient(false); slowHeatScene(338, 0); }},
      {"k9_kurz_am_ziel", T, 340, 350, 650, 201, false, [] { heatingScene(340, 650, 0); }},
      {"k9_kurz_am_ziel_100w", T, 340, 350, 1000, 201, false, [] { heatingScene(340, 1000, 0); }},
      {"k5_heizt_plus_gedrueckt", T, 205, 350, 650, 201, false, [] { pressed(true, 100); heatingScene(205, 650, 0); }},
      // Soldering: no dog, the solder's name, clicks change it at once
      {"l1_bereit_350", T, 350, 350, 90, 201, false, [] { solderScene(350, 350, Series::Set350, 0); }},
      {"l2_plus_370_heizt", T, 352, 370, 650, 201, false, [] { solderScene(352, 370, Series::Set350, 300); }},
      {"l3_bereit_370", T, 370, 370, 120, 201, false, [] { solderScene(370, 370, Series::Set370, 0); }},
      {"l4_minus_350_kuehlt", T, 368, 350, 0, 201, false, [] { solderScene(368, 350, Series::Set370, 400); }},
      {"l5_platine_bleifrei_heizt", T, 341, 350, 650, 201, false, [] { solderScene(341, 350, Series::Load, 0); }},
      {"l6_platine_bleifrei_wechsel", T, 341, 350, 650, 201, false, [] { solderScene(341, 350, Series::Load, 500); }},
      {"l7_empfindlich_wechsel", T, 240, 240, 60, 201, false, [] { solderScene(240, 240, Series::Set240, 900); }},
      {"l8_kabel_masse", T, 400, 400, 200, 201, false, [] { solderScene(400, 400, Series::Set400, 0); }},
      {"l9_manuell_335", T, 335, 335, 80, 201, false, [] { solderScene(335, 335, Series::Set335, 0); }},
      {"l10_gedreht", T, 350, 350, 90, 201, false, [] { orient(false); solderScene(350, 350, Series::Set350, 0); }},
      {"l11_minus_gedrueckt", T, 350, 350, 90, 201, false, [] { pressed(false, 100); solderScene(350, 350, Series::Set350, 0); }},
      // Boost: the dog sprints, the ground runs to the left
      {"b1_boost", 209000, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild1", 209025, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild2", 209050, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild3", 209075, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b2_boost_gedreht", 209000, 400, 350, 650, 201, false, [] { orient(false); boostScene(23); }},
      {"h6_umgedreht", H0, 26, 350, 0, 201, false, [] { orient(false); home(); }},
      {"h8_gesamt_max", H0, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_aktuell_38", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 38); }},
      {"h8_aktuell_999", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_aktuell_max", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 59940); }},
      {"h8_vorgaenge_9999", H0 + 4000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 9999, 999); }},
      {"h8_vorgaenge_max", H0 + 4000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_boosts_max", H0 + 6000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_umgedreht_vorgaenge_9999", H0 + 4000, 350, 350, 0, 201, false, [] { orient(false); homeStatistics(65534, 9999, 999); }},
      {"h9_runter", H0, 180, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cooling, 180); calm(); ui_draw_homescreen_simplified(180); }},
      {"h9_runter_wippt", H0 + 500, 180, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cooling, 180); calm(); ui_draw_homescreen_simplified(180); }},
      {"h9_hoch", H0, 200, 350, 0, 201, false, [] { feedHistory(HomeCurve::Heating, 200); calm(); ui_draw_homescreen_simplified(200); }},
      {"h9_umgedreht_hoch", H0, 200, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Heating, 200); calm(); ui_draw_homescreen_simplified(200); }},
      {"g_detail", T, 318, 350, 580, 201, false, [] { limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"g_stopp", T, 350, 350, 90, 201, false, [] { limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      // Other states, no board
      {"s4_druck", 6000000 + 3000, 26, 335, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      {"s5_halten", 6000000 + 3000, 26, 335, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(true, 150 + 270); ui_draw_homescreen_simplified(26); }},
      {"r4_detail_gedreht", T, 318, 350, 580, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"r6_stopp_gedreht", T, 350, 350, 90, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      // The menu: one list. Rows: 0 target, 1 lock, 2 boost, 3 sleep after, 4 sleep temp, 5 off after, 6 motion, 7 brightness, 8 rotation,
      // 9 unit, 10 invert, 11 detail, 12 swap, 13 animation, 14 logo, 15 autostart, 16 tip, 17 power, 18 dc, 19 pd mode, 20 pd timeout,
      // 21 keep awake, 22 calibrate temp, 23 calibrate volts, 24 reset, 25-34 the readings. Coming down the list, as on the iron
#define MENU_AT(row) [] { for (int i = 0; i < row; i++) dogDrawMenuList(i), OLED::clearScreen(); dogDrawMenuList(row); }
      {"m00_oben", T, 26, 335, 0, 201, false, [] { for (int i = 34; i >= 0; i--) { dogDrawMenuList(i); if (i) OLED::clearScreen(); } }},
      {"m01_runter", T, 26, 335, 0, 201, false, MENU_AT(3)},
      {"m02_ziel_aendern", T, 26, 335, 0, 201, false, [] { for (int i = 34; i >= 0; i--) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuValue(0); }},
      {"m03_schlaf_aendern", T, 26, 320, 0, 201, false, [] { settings[SettingsOptions::SleepTime] = 10; for (int i = 34; i >= 0; i--) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuList(3); OLED::clearScreen(); dogDrawMenuValue(3); }},
      {"m04_blinkt", T - 5000, 26, 320, 0, 201, false, [] { settings[SettingsOptions::LockingMode] = 2; for (int i = 34; i >= 0; i--) { dogDrawMenuList(i); OLED::clearScreen(); } dogMenuBlink(); dogDrawMenuList(1); settings[SettingsOptions::LockingMode] = 0; }},
      {"m05_anzeige", T, 26, 320, 0, 201, false, [] { settings[SettingsOptions::OLEDBrightness] = 51; settings[SettingsOptions::OrientationMode] = 2; for (int i = 0; i < 10; i++) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuList(10); }},
      {"m06_laengste", T, 26, 320, 0, 201, false, [] { settings[SettingsOptions::AnimationSpeed] = 1; settings[SettingsOptions::LOGOTime] = 5; for (int i = 0; i < 15; i++) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuList(15); settings[SettingsOptions::AnimationSpeed] = 0; }},
      {"m07_strom", T, 26, 320, 0, 201, false, [] { settings[SettingsOptions::SolderingTipType] = 1; settings[SettingsOptions::PowerLimit] = 100; settings[SettingsOptions::USBPDMode] = 1; settings[SettingsOptions::PDNegTimeout] = 20; for (int i = 0; i < 20; i++) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuList(20); settings[SettingsOptions::PowerLimit] = 0; settings[SettingsOptions::USBPDMode] = 0; }},
      {"m08_zuruecksetzen", T, 26, 320, 0, 201, false, MENU_AT(24)},
      {"m09_werte_1", T, 26, 320, 0, 201, false, [] { limitX10 = 900; for (int i = 0; i < 28; i++) { dogDrawMenuList(i); OLED::clearScreen(); } dogDrawMenuList(28); }},
      {"m10_werte_2", T, 26, 320, 0, 201, false, MENU_AT(32)},
      {"m11_ende", T, 26, 320, 0, 201, false, MENU_AT(34)},
      {"m12_gedreht", T, 26, 335, 0, 201, false, [] { orient(false); for (int i = 34; i >= 0; i--) { dogDrawMenuList(i); if (i) OLED::clearScreen(); } }},
      {"w6_sperre_aus_geste", T, 26, 320, 0, 201, false, [] { dogDrawNotice(iconLock, DOG_T("AUS", "OFF"), DOG_T("SPERRE IM MENÜ AUS", "LOCK OFF IN THE MENU")); }},
      // Sleep: T4 starts a burn-in phase and the z loop; 3 s to the loop, 4 s to the shift
#define S0 6000000
#define SLEEPING(frame) [] { currentTempTargetDegC = 150; dogSleepWakeFrame = frame; lastMovementTime = lastButtonTime = S0 - 161000; ui_draw_soldering_basic_sleep(tipC); dogSleepWakeFrame = 0; currentTempTargetDegC = 350; }
      {"s01_schlaf_z_0", S0 + 200, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s02_schlaf_z_1", S0 + 800, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s03_schlaf_z_2", S0 + 1400, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s04_schlaf_z_3", S0 + 2400, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s05_schlaf_z_leer", S0 + 2850, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s10_verschoben_0", S0, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s11_verschoben_1", S0 + 4000, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s12_verschoben_2", S0 + 8000, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s13_verschoben_3", S0 + 12000, 150, 350, 0, 201, false, SLEEPING(0)},
      {"s20_wach_bild1", S0, 150, 350, 0, 201, false, SLEEPING(1)},
      {"s21_wach_bild2", S0, 150, 350, 0, 201, false, SLEEPING(2)},
      {"s22_wach_bild3", S0, 150, 350, 0, 201, false, SLEEPING(3)},
      {"s30_kuehlt_noch", S0 + 1400, 280, 350, 0, 201, false, SLEEPING(0)},
      {"s31_aus_nach_aus", S0 + 1400, 150, 350, 0, 201, false, [] { settings[SettingsOptions::ShutdownTime] = 0; currentTempTargetDegC = 150; ui_draw_soldering_basic_sleep(150); currentTempTargetDegC = 350; }},
      {"s32_gedreht", S0 + 1400, 150, 350, 0, 201, false, [] { orient(false); currentTempTargetDegC = 150; lastMovementTime = lastButtonTime = S0 - 161000; ui_draw_soldering_basic_sleep(150); currentTempTargetDegC = 350; }},
      {"s33_stopp_gehalten", S0, 150, 350, 0, 201, false, [] { pressed(false, 150 + 191); currentTempTargetDegC = 150; ui_draw_soldering_basic_sleep(150); currentTempTargetDegC = 350; }},
      {"w1_spitze", T, 26, 320, 0, 201, false, [] { tipMissing = true; ui_draw_homescreen_simplified(26); }},
      {"w2_strom", T, 26, 320, 0, 85, false, [] { dogDrawNotice(iconWarning, "STROM!", "NUR 8.5V AM EINGANG"); }},
      {"w3_heizung", T, 26, 320, 0, 201, false, [] { dogAlert = DogAlert::Thermal; ui_draw_homescreen_simplified(26); dogAlert = DogAlert::None; }},
      {"w4_griff", T, 26, 320, 0, 201, false, [] { handleX10 = 620; dogAlert = DogAlert::Handle; ui_draw_homescreen_simplified(26); dogAlert = DogAlert::None; handleX10 = 310; }},
      {"w5_sperre", T, 26, 320, 0, 201, false, [] { dogWarnLocked(true, BUTTON_NONE); }},
  };
}

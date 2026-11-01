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

// Leaves the filter settled on this reading, at the scene's time. The iron's angle in degrees: positive = tip end down; gravity 1 g = 8000
static void holdAtAngle(int degrees) {
  const double a = degrees * 3.14159265 / 180;
  const uint32_t end = previewTicks;
  dogHomeMotionReset();
  setAccel((int16_t)(8000 * sin(a)), (int16_t)(-8000 * cos(a)), 0);
  for (int i = 48; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
  }
}

// Calm, then shaken for the given number of frames
static void shaken(int frames) {
  const uint32_t end = previewTicks;
  previewTicks       = end - frames * 125;
  holdAtAngle(0);
  setAccel(0, -16000, 0);
  for (int i = frames - 1; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
  }
}

static void home(int degrees) {
  feedHistory(HomeCurve::Cold, tipC);
  holdAtAngle(degrees);
  ui_draw_homescreen_simplified(tipC);
}

static void homeStatistics(uint16_t minutesTotal, uint16_t counts, uint16_t session) {
  settings[SettingsOptions::TotalSolderMinutes] = minutesTotal;
  settings[SettingsOptions::TotalSolderSessions] = settings[SettingsOptions::TotalBoosts] = counts;
  dogSessionMinutes = session;
  feedHistory(HomeCurve::Heating, 350); // the widest temperature and the arrow: the tightest the top row gets
  holdAtAngle(0);
  ui_draw_homescreen_simplified(350);
}


// The tilt held for this many frames after level: the fade in
static void tilted(int degrees, int frames) {
  feedHistory(HomeCurve::Cold, tipC);
  const uint32_t end = previewTicks;
  previewTicks       = end - frames * 125;
  holdAtAngle(0);
  const double a = degrees * 3.14159265 / 180;
  setAccel((int16_t)(8000 * sin(a)), (int16_t)(-8000 * cos(a)), 0);
  for (int i = frames - 1; i >= 0; i--) {
    previewTicks = end - i * 125;
    for (int k = 0; k < 3; k++) {
      dogHomeMotionUpdate(); // a raw filter: let it settle within the frame, as a held angle does after a few frames
    }
  }
  ui_draw_homescreen_simplified(tipC);
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

static void boostScene(uint8_t secondsLeft) {
  limitX10 = 650;
  settings[SettingsOptions::BoostTemp] = 420;
  setBoostPreview(secondsLeft);
  atTemp(199);
  ui_draw_soldering_basic_status(true);
}

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  const DogKey solderKey = {iconPlay8, "LÖTEN"}, menuKey = {iconMenu, "MENÜ"};
  return {
      // The home screen: H0 is on the statistics' first phase, each phase lasts 2 s
#define H0 6000000
      {"h1_ruhig", H0, 26, 350, 0, 201, false, [] { home(0); }},
      {"h2_links20", H0, 26, 350, 0, 201, false, [] { home(-20); }},
      {"h3_rechts20", H0, 26, 350, 0, 201, false, [] { home(20); }},
      {"h4_spitze_unten", H0, 26, 350, 0, 201, false, [] { home(60); }},
      {"h5_geschuettelt_0", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(1); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(2); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(3); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(4); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(5); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_5", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(6); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_6", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(7); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_7", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shaken(8); ui_draw_homescreen_simplified(26); }},
      {"h2_links20_bild1", H0, 26, 350, 0, 201, false, [] { tilted(-20, 1); }},
      {"h2_links20_bild2", H0, 26, 350, 0, 201, false, [] { tilted(-20, 2); }},
      {"h4_spitze_unten_bild1", H0, 26, 350, 0, 201, false, [] { tilted(60, 1); }},
      {"h4_spitze_unten_bild2", H0, 26, 350, 0, 201, false, [] { tilted(60, 2); }},
      {"h4_spitze_unten_bild3", H0, 26, 350, 0, 201, false, [] { tilted(60, 3); }},
      {"h4_spitze_unten_bild4", H0, 26, 350, 0, 201, false, [] { tilted(60, 4); }},
      {"h4_spitze_unten_bild5", H0, 26, 350, 0, 201, false, [] { tilted(60, 5); }},
      {"h4_spitze_unten_bild6", H0, 26, 350, 0, 201, false, [] { tilted(60, 6); }},
      {"h4_spitze_unten_bild7", H0, 26, 350, 0, 201, false, [] { tilted(60, 7); }},
      {"h4_spitze_unten_bild8", H0, 26, 350, 0, 201, false, [] { tilted(60, 8); }},
      {"h3_rechts8", H0, 26, 350, 0, 201, false, [] { home(8); }},
      {"h6_umgedreht_links20", H0, 26, 350, 0, 201, false, [] { orient(false); home(-20); }},
      {"h6_umgedreht_rechts20", H0, 26, 350, 0, 201, false, [] { orient(false); home(20); }},
      {"h10_hinten_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(0); pressed(false, 100); ui_draw_homescreen_simplified(26); }},
      {"h10_vorne_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(0); pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      // Heating up, the first time to the set point: the dog runs up the curve, its legs as fast as the power allows
      {"k1_heizt_65w", T, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 0); }},
      {"k1_heizt_65w_bild1", T + 50, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 1); }},
      {"k1_heizt_65w_bild2", T + 100, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 2); }},
      {"k1_heizt_65w_bild3", T + 150, 120, 350, 650, 201, false, [] { heatingScene(120, 650, 3); }},
      {"k2_heizt_start", T, 41, 350, 650, 201, false, [] { heatingScene(41, 650, 0); }},
      {"k3_heizt_fast_da", T, 338, 350, 210, 201, false, [] { heatingScene(338, 210, 0); }},
      {"k4_heizt_gedreht", T, 205, 350, 650, 201, false, [] { orient(false); heatingScene(205, 650, 0); }},
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
      {"h6_umgedreht", H0, 26, 350, 0, 201, false, [] { orient(false); home(0); }},
      {"h7_umgedreht_spitze_tiefer20", H0, 26, 350, 0, 201, false, [] { orient(false); home(20); }},
      {"h8_gesamt_max", H0, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_aktuell_38", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 38); }},
      {"h8_aktuell_999", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_aktuell_max", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 59940); }},
      {"h8_vorgaenge_9999", H0 + 4000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 9999, 999); }},
      {"h8_vorgaenge_max", H0 + 4000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_boosts_max", H0 + 6000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_umgedreht_vorgaenge_9999", H0 + 4000, 350, 350, 0, 201, false, [] { orient(false); homeStatistics(65534, 9999, 999); }},
      {"h9_runter", H0, 180, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cooling, 180); holdAtAngle(0); ui_draw_homescreen_simplified(180); }},
      {"h9_runter_wippt", H0 + 500, 180, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cooling, 180); holdAtAngle(0); ui_draw_homescreen_simplified(180); }},
      {"h9_hoch", H0, 200, 350, 0, 201, false, [] { feedHistory(HomeCurve::Heating, 200); holdAtAngle(0); ui_draw_homescreen_simplified(200); }},
      {"h9_umgedreht_hoch", H0, 200, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Heating, 200); holdAtAngle(0); ui_draw_homescreen_simplified(200); }},
      {"g_detail", T, 318, 350, 580, 201, false, [] { limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"g_stopp", T, 350, 350, 90, 201, false, [] { limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      {"g_schlaf", 6000000 + 162000, 150, 350, 0, 201, false, [] { dogSleepEnterTime = previewTicks - 161000; lastMovementTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"g_menu", T, 26, 350, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; settings[SettingsOptions::SleepTemp] = 150; dogDrawMenuList(2, 1); }},
      {"g_wert", T, 26, 350, 0, 201, false, [] { lastButtonTime = previewTicks - 1800; settings[SettingsOptions::SleepTime] = 11; dogDrawMenuValue(2, 1); }},
      // Other states, no board
      {"s4_druck", 6000000 + 3000, 26, 335, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(0); pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      {"s5_halten", 6000000 + 3000, 26, 335, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(0); pressed(true, 150 + 270); ui_draw_homescreen_simplified(26); }},
      {"r4_detail_gedreht", T, 318, 350, 580, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"r5_schlaf_gedreht", 6000000 + 162000, 150, 350, 0, 201, false, [] { orient(false); dogSleepEnterTime = previewTicks - 161000; lastMovementTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"r6_stopp_gedreht", T, 350, 350, 90, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      {"z2_schlaf_aus", 6000000 + 162000, 150, 350, 0, 201, false, [] { settings[SettingsOptions::ShutdownTime] = 0; dogSleepEnterTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"i1_strom", T, 26, 320, 0, 201, false, [] { limitX10 = 900; dogDrawInfoPage(0); }},
      {"i2_spitze", T, 26, 320, 0, 201, false, [] { dogDrawInfoPage(1); }},
      {"i3_griff", T, 26, 320, 0, 201, false, [] { dogDrawInfoPage(2); }},
      {"i4_statistik", T, 26, 320, 0, 201, false, [] { dogDrawInfoPage(3); }},
      {"i5_system", T, 26, 320, 0, 201, false, [] { dogDrawInfoPage(4); }},
      {"w1_spitze", T, 26, 320, 0, 201, false, [] { tipMissing = true; ui_draw_homescreen_simplified(26); }},
      {"w2_strom", T, 26, 320, 0, 85, false, [] { dogDrawNotice(iconWarning, "STROM!", "NUR 8.5V AM EINGANG"); }},
      {"w3_heizung", T, 26, 320, 0, 201, false, [] { dogAlert = DogAlert::Thermal; ui_draw_homescreen_simplified(26); dogAlert = DogAlert::None; }},
      {"w4_griff", T, 26, 320, 0, 201, false, [] { handleX10 = 620; dogAlert = DogAlert::Handle; ui_draw_homescreen_simplified(26); dogAlert = DogAlert::None; handleX10 = 310; }},
      {"w5_sperre", T, 26, 320, 0, 201, false, [] { dogWarnLocked(true, BUTTON_NONE); }},
      // Step 4: the menu (sections: 0 info, 1 solder, 2 sleep, 3 power, 4 display, 5 system)
            {"m2_menu_info", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(DOG_MENU_ROOT, 0); }},
      {"m3_menu_schlaf", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(DOG_MENU_ROOT, 2); }},
      {"m4_item", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; dogDrawMenuList(2, 1); }},
      {"m5_toggle", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(2, 0); }},
      {"m6_edit", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks - 600; settings[SettingsOptions::SleepTime] = 11; dogDrawMenuValue(2, 1); }},
      {"m9_halten", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; pressed(false, 150 + 270); dogDrawMenuList(2, 1); }},
      {"m10_hilfe", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks - 5000; settings[SettingsOptions::SleepTime] = 10; dogDrawMenuList(2, 1); }},
      {"m13_vorne_halten", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; pressed(true, 150 + 270); dogDrawMenuList(2, 1); }},
      {"i6_debug_halten", T, 26, 320, 0, 201, false, [] { pressed(true, 150 + 270); dogDrawInfoPage(4); }},
      {"m14_anzeige_schalter", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::OLEDBrightness] = 3; settings[SettingsOptions::OrientationMode] = 2; settings[SettingsOptions::AnimationSpeed] = 2; dogDrawMenuList(4, 3); }},
      {"m15_system", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::KeepAwakePulse] = 5; settings[SettingsOptions::ReverseButtonTempChangeEnabled] = 0; dogDrawMenuList(5, 4); }},
      {"m16_strom", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::USBPDMode] = 1; settings[SettingsOptions::PDNegTimeout] = 20; settings[SettingsOptions::PowerLimit] = 0; dogDrawMenuList(3, 0); }},
      {"m11_loeten_spitze", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(1, 3); }},
  };
}

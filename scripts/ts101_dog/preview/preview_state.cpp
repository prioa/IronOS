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
static void setAngle(int degrees) {
  const double a = degrees * 3.14159265 / 180;
  setAccel((int16_t)(8000 * sin(a)), (int16_t)(-8000 * cos(a)), 0);
}

// frames of the dog at 8 a second, the last at the scene's time
static void runFrames(int frames) {
  const uint32_t end = previewTicks;
  for (int i = frames - 1; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
  }
}

static void holdAtAngle(int degrees) {
  const uint32_t end = previewTicks;
  previewTicks       = end - 40 * 125;
  dogHomeMotionReset();
  setAngle(degrees);
  dogHomeMotionUpdate();
  previewTicks = end;
  runFrames(40);
}

static void calm() { holdAtAngle(0); }

// Level, then held at this angle for this many frames
static void turnedFor(int degrees, int frames) {
  const uint32_t end = previewTicks;
  previewTicks       = end - frames * 125;
  calm();
  previewTicks = end;
  setAngle(degrees);
  runFrames(frames);
}

// Level, then shaken for this many frames, then calm for this many
static void shakenFor(int frames, int calmFrames = 0) {
  const uint32_t end = previewTicks;
  previewTicks       = end - (frames + calmFrames) * 125;
  calm();
  const uint32_t shakeEnd = end - calmFrames * 125;
  for (int i = frames - 1; i >= 0; i--) {
    previewTicks = shakeEnd - i * 125;
    setAccel(0, (frames - 1 - i) % 2 ? -3000 : -16000, 0); // at the end of the shake too, the time it holds is the firmware's business
    dogHomeMotionUpdate();
  }
  setAngle(0);
  for (int i = calmFrames - 1; i >= 0; i--) {
    previewTicks = end - i * 125;
    dogHomeMotionUpdate();
  }
  previewTicks = end;
}

// Level and alone until the next idle moment: frames counted so that the blink or the tongue is showing
static void idleFor(int extraFrames, int firstAt = 52) {
  const uint32_t end = previewTicks;
  previewTicks       = end - (firstAt + extraFrames) * 125;
  dogHomeMotionReset();
  setAngle(0);
  dogHomeMotionUpdate();
  previewTicks = end;
  runFrames(firstAt + extraFrames);
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


// The tip's way to the scene's time: each step is a target temperature and how many samples of 0.2 s it takes to get there
struct CurveStep {
  int temp, samples;
};
static void curve(int start, std::initializer_list<CurveStep> steps) {
  int total = 0;
  for (const CurveStep &s : steps) {
    total += s.samples;
  }
  const uint32_t end = previewTicks;
  int            at  = 0, from = start;
  dogCurveReset();
  previewTicks = end - total * 200;
  dogCurveSample(start, 200);
  for (const CurveStep &s : steps) {
    for (int i = 1; i <= s.samples; i++) {
      at++;
      previewTicks = end - (total - at) * 200;
      dogCurveSample(from + (s.temp - from) * i / s.samples, 200);
    }
    from = s.temp;
  }
  previewTicks = end;
}

// The screen with the solder last changed this long ago (0: not at all) and the set point reached this long ago (0: not yet)
static void solderShot(uint32_t changedMs, uint32_t reachedS) {
  dogLotChangedAt = changedMs ? (previewTicks - changedMs) | 1 : 0;
  if (reachedS) {
    atTemp(reachedS);
  }
  ui_draw_soldering_basic_status(false);
}

static void heating170() {
  curve(26, {{170, 16}}); // 9 C in a sample of 0.2 s: 45 C a second
  solderShot(0, 0);
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
      {"h2_links10", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(-10); ui_draw_homescreen_simplified(26); }},
      {"h2_links20", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(-20); ui_draw_homescreen_simplified(26); }},
      {"h2_links30", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(-30); ui_draw_homescreen_simplified(26); }},
      {"h3_rechts10", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(10); ui_draw_homescreen_simplified(26); }},
      {"h3_rechts20", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(20); ui_draw_homescreen_simplified(26); }},
      {"h3_rechts30", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(30); ui_draw_homescreen_simplified(26); }},
      {"h4_spitze_unten", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); holdAtAngle(60); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_links10", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(10); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_links20", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(20); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_links30", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(30); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_rechts10", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(-10); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_rechts20", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(-20); ui_draw_homescreen_simplified(26); }},
      {"h6_umgedreht_rechts30", H0, 26, 350, 0, 201, false, [] { orient(false); feedHistory(HomeCurve::Cold, 26); holdAtAngle(-30); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(1); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(2); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(3); ui_draw_homescreen_simplified(26); }},
      {"h5_geschuettelt_4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4); ui_draw_homescreen_simplified(26); }},
      {"h7_nach_schuetteln_1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4, 3); ui_draw_homescreen_simplified(26); }},
      {"h7_nach_schuetteln_2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4, 4); ui_draw_homescreen_simplified(26); }},
      {"h7_nach_schuetteln_3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4, 5); ui_draw_homescreen_simplified(26); }},
      {"h7_nach_schuetteln_4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4, 6); ui_draw_homescreen_simplified(26); }},
      {"h7_nach_schuetteln_5", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); shakenFor(4, 7); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 1); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 2); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 3); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 4); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild5", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 5); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild6", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 6); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild7", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 7); ui_draw_homescreen_simplified(26); }},
      {"h3a_rechts_bild8", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(20, 8); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild1", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 1); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 2); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild3", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 3); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild4", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 4); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild5", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 5); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild6", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 6); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild7", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 7); ui_draw_homescreen_simplified(26); }},
      {"h2a_links_bild8", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); turnedFor(-20, 8); ui_draw_homescreen_simplified(26); }},
      {"h8a_blinzelt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); idleFor(0); ui_draw_homescreen_simplified(26); }},
      {"h8b_blinzelt_2", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); idleFor(1); ui_draw_homescreen_simplified(26); }},
      {"h8c_zunge", H0, 80, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 80); idleFor(3, 24); ui_draw_homescreen_simplified(80); }},
      {"h10_hinten_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(false, 100); ui_draw_homescreen_simplified(26); }},
      {"h10_vorne_gedrueckt", H0, 26, 350, 0, 201, false, [] { feedHistory(HomeCurve::Cold, 26); calm(); pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      // Soldering: one screen, heating, cooling or ready
      {"n1_aufheizen_170", T, 170, 350, 650, 201, false, [] { heating170(); }},
      {"n1_aufheizen_170_wippt", T + 500, 170, 350, 650, 201, false, [] { heating170(); }},
      {"n2_kurz_vor_ziel", T, 338, 350, 280, 201, false, [] { curve(26, {{320, 16}, {338, 15}}); solderShot(0, 0); }},
      {"n3_bereit", T, 351, 350, 180, 201, false, [] { curve(26, {{350, 18}, {350, 39}}); solderShot(0, 38); }},
      {"n4_lotwechsel", T, 358, 370, 650, 201, false, [] { curve(26, {{350, 18}, {350, 36}, {358, 3}}); solderShot(600, 45); }},
      {"n4b_lotwechsel_vorbei", T, 370, 370, 200, 201, false, [] { curve(26, {{350, 18}, {350, 30}, {370, 9}}); solderShot(2100, 45); }},
      {"n5_abkuehlen", T, 345, 320, 0, 201, false, [] { curve(26, {{350, 18}, {350, 30}, {345, 4}}); solderShot(800, 45); }},
      {"n6_umgedreht", T, 170, 350, 650, 201, false, [] { orient(false); heating170(); }},
      {"n6b_umgedreht_bereit", T, 351, 350, 180, 201, false, [] { orient(false); curve(26, {{350, 18}, {350, 39}}); solderShot(0, 38); }},
      {"n7_start", T, 26, 350, 650, 201, false, [] { curve(26, {}); solderShot(0, 0); }},
      {"n7b_keine_schaetzung", T, 200, 350, 650, 201, false, [] { curve(190, {{200, 8}, {200, 8}}); solderShot(0, 0); }},
      {"n8_snbi_bereit", T, 240, 240, 60, 201, false, [] { curve(240, {{240, 56}}); solderShot(0, 75); }},
      {"n8b_bleifrei_markiert", T, 350, 350, 180, 201, false, [] { curve(350, {{350, 56}}); solderShot(500, 75); }},
      {"n8c_masse_lange", T, 400, 400, 200, 201, false, [] { curve(400, {{400, 56}}); solderShot(0, 754); }},
      {"n8d_eigen", T, 335, 335, 90, 201, false, [] { curve(335, {{335, 56}}); solderShot(0, 75); }},
      {"n9_alles_breit", T, 100, 400, 1000, 201, false, [] { curve(26, {{100, 1}}); solderShot(0, 0); }},
      {"n9b_lange_zeit", T, 400, 400, 1000, 201, false, [] { curve(400, {{430, 28}, {400, 28}}); solderShot(0, 7425); }},
      {"n9c_ziffer_9", T, 199, 350, 650, 201, false, [] { heating170(); }},
      {"n10_fahrenheit", T, 170, 660, 650, 201, false, [] { settings[SettingsOptions::TemperatureInF] = 1; currentTempTargetDegC = 349; heating170(); settings[SettingsOptions::TemperatureInF] = 0; }},
      // Boost: the dog sprints, the ground runs to the left
      {"b1_boost", 209000, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild1", 209025, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild2", 209050, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b1_boost_bild3", 209075, 400, 350, 650, 201, false, [] { boostScene(23); }},
      {"b2_boost_gedreht", 209000, 400, 350, 650, 201, false, [] { orient(false); boostScene(23); }},
      {"h6_umgedreht", H0, 26, 350, 0, 201, false, [] { orient(false); home(); }},
      {"h8_gesamt_max", H0, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_seit_start_38", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 38); }},
      {"h8_seit_start_100", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 100); }},
      {"h8_seit_start_999", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 999); }},
      {"h8_seit_start_max", H0 + 2000, 350, 350, 0, 201, false, [] { homeStatistics(65534, 65534, 59940); }},
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

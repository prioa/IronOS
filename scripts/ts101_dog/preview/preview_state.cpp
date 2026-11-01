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
  const bool twoStates = o == SettingsOptions::OLEDInversion || o == SettingsOptions::TemperatureInF || o == SettingsOptions::ReverseButtonTempChangeEnabled || o == SettingsOptions::CalibrateCJC || o == SettingsOptions::DogSolderView || o == SettingsOptions::DogHomeVariant;
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
  settings[SettingsOptions::DogHomeVariant] = 0;
  dogHeatupStartTempC                                                     = 25;
  dogLastSessionSeconds                                                   = 27 * 60 + 12;
}

// The same simulated tip temperatures as the mock
enum class Series { Steady, Dip, Drop, Heat, Boost, Cool, Set350, Load, Limited };
static void fillSeries(Series kind, uint32_t spacing = 250) {
  dogCurveReset();
  const uint32_t now = previewTicks;
  for (int i = 0; i < 72; i++) {
    int t = 320;
    switch (kind) {
    case Series::Set350: t = 350; break;
    case Series::Load: { // the dip on the board: rows below the set line, by column
      const int x = i * 78 / 72;
      const int y = x < 57 ? 0 : x < 58 ? 1 : x < 59 ? 2 : x < 60 ? 3 : x < 61 ? 4 : x < 65 ? 5 : x < 67 ? 4 : x < 69 ? 3 : x < 73 ? 2 : 1;
      t = 350 - y * 45 / 7;
      break;
    }
    case Series::Limited: t = 49 + i * 119 / 71; break;
    case Series::Steady: t = 320 + (i % 17 == 0 ? 1 : 0); break;
    case Series::Dip: t = (i < 40 || i >= 60) ? 320 : (i < 46 ? 320 - (i - 40) * 5 : 290 + (i - 46) * 2); break;
    case Series::Drop: t = i < 52 ? 320 : (320 - (i - 52) * 4 > 288 ? 320 - (i - 52) * 4 : 288); break;
    case Series::Heat: t = 25 + i * 155 / 71; break;
    case Series::Boost: t = i < 40 ? 320 : (320 + (i - 40) * 4 < 410 ? 320 + (i - 40) * 4 : 410); break;
    case Series::Cool: t = 25 + (int)(295 * exp(-i / 110.0)); break; // 320 down to 180 over 3 minutes
    }
    previewTicks = now - (71 - i) * spacing;
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

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  const DogKey solderKey = {iconPlay8, "LÖTEN"}, menuKey = {iconMenu, "MENÜ"};
  return {
      // The Gegenentwürfe boards, compared with c_g_*.png
      {"g_start", 6000000 + 3000, 26, 350, 0, 201, false, [] { limitX10 = 650; vX10 = 200; ui_draw_homescreen_simplified(26); }},
      {"g_start_hund", 6000000 + 3000, 26, 350, 0, 201, false, [] { limitX10 = 650; vX10 = 200; settings[SettingsOptions::DogHomeVariant] = 1; lastMovementTime = 0; ui_draw_homescreen_simplified(26); }},
      {"g_heiss", 6000000 + 3500, 180, 350, 0, 201, false, [] { limitX10 = 650; vX10 = 200; fillSeries(Series::Cool, 2500); ui_draw_homescreen_simplified(180); }},
      {"g_start_flip", 6000000 + 3000, 26, 350, 0, 201, false, [] { orient(false); limitX10 = 650; vX10 = 200; ui_draw_homescreen_simplified(26); }},
      {"g_heizt", T, 185, 350, 650, 201, false, [] { limitX10 = 650; fillSeries(Series::Heat); ui_draw_soldering_basic_status(false); }},
      {"g_begrenzt", T, 142, 350, 130, 90, false, [] { limitX10 = 130; fillSeries(Series::Limited); ui_draw_soldering_basic_status(false); }},
      {"g_bereit", T, 350, 350, 90, 201, false, [] { limitX10 = 650; fillSeries(Series::Set350); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"g_erreicht", T, 350, 350, 90, 201, false, [] { limitX10 = 650; fillSeries(Series::Set350); atTemp(0); dogReachedUntil = previewTicks + 100; ui_draw_soldering_basic_status(false); }},
      {"g_last", T, 318, 350, 580, 201, false, [] { limitX10 = 650; fillSeries(Series::Load); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"g_detail", T, 318, 350, 580, 201, false, [] { limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"g_boost", 209000, 385, 350, 650, 201, false, [] { limitX10 = 650; settings[SettingsOptions::BoostTemp] = 400; setBoostPreview(28); ui_draw_soldering_basic_status(true); settings[SettingsOptions::BoostTemp] = 420; }},
      {"g_stopp", T, 350, 350, 90, 201, false, [] { limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      {"g_temp", T, 318, 350, 150, 201, false, [] { limitX10 = 650; dogDrawTemperatureView(false); }},
      {"g_schlaf", 6000000 + 162000, 150, 350, 0, 201, false, [] { dogSleepEnterTime = previewTicks - 161000; lastMovementTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"g_menu", T, 26, 350, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; settings[SettingsOptions::SleepTemp] = 150; dogDrawMenuList(3, 1); }},
      {"g_wert", T, 26, 350, 0, 201, false, [] { lastButtonTime = previewTicks - 1800; settings[SettingsOptions::SleepTime] = 11; dogDrawMenuValue(3, 1); }},
      // Other states, no board
      {"s3_schwach", 6000000 + 3000, 26, 320, 0, 90, false, [] { limitX10 = 180; ui_draw_homescreen_simplified(26); }},
      {"s4_druck", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      {"s5_halten", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; pressed(true, 150 + 270); ui_draw_homescreen_simplified(26); }},
      {"r2_heiss_gedreht", 6000000 + 3500, 180, 350, 0, 201, false, [] { orient(false); limitX10 = 650; vX10 = 200; fillSeries(Series::Cool, 2500); ui_draw_homescreen_simplified(180); }},
      {"r3_bereit_gedreht", T, 350, 350, 90, 201, false, [] { orient(false); limitX10 = 650; fillSeries(Series::Set350); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"r4_detail_gedreht", T, 318, 350, 580, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); ui_draw_soldering_power_status(false); }},
      {"r5_schlaf_gedreht", 6000000 + 162000, 150, 350, 0, 201, false, [] { orient(false); dogSleepEnterTime = previewTicks - 161000; lastMovementTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"r6_stopp_gedreht", T, 350, 350, 90, 201, false, [] { orient(false); limitX10 = 650; atTemp(199); pressed(false, 150 + 191); ui_draw_soldering_basic_status(false); }},
      {"r7_temp_gedreht", T, 318, 350, 150, 201, false, [] { orient(false); limitX10 = 650; dogDrawTemperatureView(false); }},
      {"z2_schlaf_aus", 6000000 + 162000, 150, 350, 0, 201, false, [] { settings[SettingsOptions::ShutdownTime] = 0; dogSleepEnterTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
      {"t2_preset300", T, 318, 300, 150, 201, false, [] { dogDrawTemperatureView(true); }},
      {"t4_preset400", T, 318, 400, 150, 201, false, [] { dogDrawTemperatureView(true); }},
      {"t1_temp_eigen", T, 318, 320, 150, 201, false, [] { fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
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
      // Step 4: the menu (sections: 0 temperature, 1 info, 2 solder, 3 sleep, 4 power, 5 display, 6 system)
      {"m1_menu_temp", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(DOG_MENU_ROOT, 0); }},
      {"m2_menu_info", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(DOG_MENU_ROOT, 1); }},
      {"m3_menu_schlaf", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(DOG_MENU_ROOT, 3); }},
      {"m4_item", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; dogDrawMenuList(3, 1); }},
      {"m5_toggle", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(3, 0); }},
      {"m6_edit", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks - 600; settings[SettingsOptions::SleepTime] = 11; dogDrawMenuValue(3, 1); }},
      {"m9_halten", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; pressed(false, 150 + 270); dogDrawMenuList(3, 1); }},
      {"m10_hilfe", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks - 5000; settings[SettingsOptions::SleepTime] = 10; dogDrawMenuList(3, 1); }},
      {"m13_vorne_halten", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::SleepTime] = 10; pressed(true, 150 + 270); dogDrawMenuList(3, 1); }},
      {"i6_debug_halten", T, 26, 320, 0, 201, false, [] { pressed(true, 150 + 270); dogDrawInfoPage(4); }},
      {"m14_anzeige_schalter", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::OLEDBrightness] = 3; settings[SettingsOptions::OrientationMode] = 2; settings[SettingsOptions::AnimationSpeed] = 2; dogDrawMenuList(5, 3); }},
      {"m15_system", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::KeepAwakePulse] = 5; settings[SettingsOptions::ReverseButtonTempChangeEnabled] = 0; dogDrawMenuList(6, 4); }},
      {"m16_strom", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; settings[SettingsOptions::USBPDMode] = 1; settings[SettingsOptions::PDNegTimeout] = 20; settings[SettingsOptions::PowerLimit] = 0; dogDrawMenuList(4, 0); }},
      {"m11_loeten_spitze", T, 26, 320, 0, 201, false, [] { lastButtonTime = previewTicks; dogDrawMenuList(2, 3); }},
      {"s6_temp", T, 26, 320, 0, 201, false, [] { dogDrawTemperatureView(false); }},
  };
}

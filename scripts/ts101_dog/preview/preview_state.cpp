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
  const bool twoStates = o == SettingsOptions::OLEDInversion || o == SettingsOptions::TemperatureInF || o == SettingsOptions::ReverseButtonTempChangeEnabled || o == SettingsOptions::CalibrateCJC;
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
  dogSolderTimerStart = dogBarkUntil = 0;
  dogHeatupStartTempC                                                     = 25;
  dogLastSessionSeconds                                                   = 27 * 60 + 12;
}

// The same simulated tip temperatures as the mock
enum class Series { Steady, Dip, Drop, Heat, Boost, Cool };
static void fillSeries(Series kind, uint32_t spacing = 250) {
  dogCurveReset();
  const uint32_t now = previewTicks;
  for (int i = 0; i < 72; i++) {
    int t = 320;
    switch (kind) {
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

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  const DogKey solderKey = {iconPlay8, "LÖTEN"}, menuKey = {iconMenu, "MENÜ"};
  return {
      // Step 3: home, sleep, info, interruptions
      {"s1_start", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; ui_draw_homescreen_simplified(26); }},
      {"s7_start_100h", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; settings[SettingsOptions::TotalSolderMinutes] = 123 * 60 + 4; ui_draw_homescreen_simplified(26); settings[SettingsOptions::TotalSolderMinutes] = 42 * 60 + 17; }},
      {"s2_heiss", 6000000 + 3500, 180, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; fillSeries(Series::Cool, 2500); ui_draw_homescreen_simplified(180); }},
      {"s8_heiss_gedreht", 6000000 + 3500, 180, 320, 0, 201, false, [] { orient(false); limitX10 = 900; vX10 = 200; fillSeries(Series::Cool, 2500); ui_draw_homescreen_simplified(180); }},
      {"s9_heiss_frisch", 6000000 + 3500, 300, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; dogCurveReset(); for (int i = 0; i < 6; i++) { previewTicks += 2500; dogCurveSample(320 - i * 4, 2500); } ui_draw_homescreen_simplified(300); }},
      {"s3_schwach", 6000000 + 3000, 26, 320, 0, 90, false, [] { limitX10 = 180; ui_draw_homescreen_simplified(26); }},
      {"s4_druck", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; pressed(true, 100); ui_draw_homescreen_simplified(26); }},
      {"s5_halten", 6000000 + 3000, 26, 320, 0, 201, false, [] { limitX10 = 900; vX10 = 200; pressed(true, 150 + 270); ui_draw_homescreen_simplified(26); }},
      {"r1_start_gedreht", 6000000 + 3000, 26, 320, 0, 201, false, [] { orient(false); limitX10 = 900; vX10 = 200; ui_draw_homescreen_simplified(26); }},
      {"z1_schlaf", 6000000 + 162000, 150, 320, 0, 201, false, [] { dogSleepEnterTime = previewTicks - 161000; ui_draw_soldering_basic_sleep(150); }},
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
      // Step 1: the key row (rows 23-31 are compared with the mock of the same name)
      {"k_s1_start", T, 26, 320, 0, 201, false, [=] { dogDrawKeys(solderKey, menuKey, "20V 90W"); }},
      {"k_s3_schwach", T, 26, 320, 0, 201, false, [=] { dogDrawKeys(solderKey, menuKey, "9V 18W!"); }},
      {"k_s4_druck", T, 26, 320, 0, 201, false, [=] { pressed(true, 100); dogDrawKeys(solderKey, menuKey, "20V 90W"); }},
      {"k_s5_halten", T, 26, 320, 0, 201, false, [] { pressed(true, 150 + 270); dogDrawHold("HALTEN: TEMPERATUR", true, DOG_HOLD_FULL); }},
      {"k_l8_stopp", T, 26, 320, 0, 201, false, [] { pressed(false, 150 + 595); dogDrawHold("HALTEN: STOPP", false, DOG_HOLD_STOP); }},
      {"k_m9_halten", T, 26, 320, 0, 201, false, [] { pressed(false, 150 + 270); dogDrawHold("HALTEN: ZURÜCK", false, DOG_HOLD_FULL); }},
      {"k_l9_druck_temp", T, 26, 320, 0, 201, false, [] { pressed(true, 100); dogDrawCorners(iconUpDown, iconInfo); }},
      {"k_r1_start_gedreht", T, 26, 320, 0, 201, false, [=] { orient(false); dogDrawKeys(solderKey, menuKey, "20V 90W"); }},
      {"k_m1_menu_temp", T, 26, 320, 0, 201, false, [] { dogDrawKeys({iconNext, "ÖFFNEN"}, {iconMenu, "WEITER"}, nullptr); }},
      {"k_m4_item", T, 26, 320, 0, 201, false, [] { dogDrawKeys({iconPlus, "ÄNDERN"}, {iconMenu, "WEITER"}, nullptr); }},
      {"k_m6_edit", T, 26, 320, 0, 201, false, [] { dogDrawCorners(iconPlus, iconMinus); }},
      {"k_s6_big", T, 26, 320, 0, 201, false, [] { dogBig(dogTextWidth(dogFontSmall, "SOLL") + 2, 320); dogSmall(0, "SOLL", 15); }},
      {"k_z1_mid", T, 26, 320, 0, 201, false, [] { dogText(dogFontMid, 127 - dogTextWidth(dogFontMid, "150") - 6, 16, "150"); }},
      {"k_m1_head", T, 26, 320, 0, 201, false, [] { dogText(dogFontHead, (128 - dogTextWidth(dogFontHead, "TEMPERATUR")) / 2, 22, "TEMPERATUR"); dogSmall(0, "MENÜ", 7); dogSmallRight(127, "1/7", 7); }},
      {"k_m8_zurueck", T, 26, 320, 0, 201, false, [] { dogText(dogFontHead, (128 - dogTextWidth(dogFontHead, "ZURÜCK")) / 2, 22, "ZURÜCK"); dogSmall(0, "SCHLAF", 7); dogSmallRight(127, "5/5", 7); }},
      // Step 2: soldering
      {"l1_aufheizen", T, 180, 320, 900, 201, false, [] { fillSeries(Series::Heat); ui_draw_soldering_basic_status(false); }},
      {"l5_begrenzt", T, 142, 320, 180, 90, false, [] { limitX10 = 180; fillSeries(Series::Heat); ui_draw_soldering_basic_status(false); }},
      {"l2_wuff", T, 318, 320, 300, 201, false, [] { fillSeries(Series::Steady); atTemp(0); dogBarkUntil = previewTicks + 1000; ui_draw_soldering_basic_status(false); }},
      {"l3_bereit", T, 320, 320, 150, 201, false, [] { fillSeries(Series::Steady); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"l4_last", T, 298, 320, 520, 201, false, [] { limitX10 = 900; fillSeries(Series::Dip); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"l6_schlafcount", T, 320, 320, 150, 201, false, [] { fillSeries(Series::Steady); atTemp(199); lastMovementTime = previewTicks - 52000; ui_draw_soldering_basic_status(false); }},
      {"l7_boost", 209000, 410, 320, 800, 201, false, [] { fillSeries(Series::Boost); currentTempTargetDegC = 420; ui_draw_soldering_basic_status(true); }},
      {"l8_stopp", T, 320, 320, 150, 201, false, [] { fillSeries(Series::Steady); atTemp(199); pressed(false, 150 + 595); ui_draw_soldering_basic_status(false); }},
      {"l9_druck_temp", T, 320, 320, 150, 201, false, [] { fillSeries(Series::Steady); atTemp(199); pressed(true, 100); ui_draw_soldering_basic_status(false); }},
      {"r2_bereit_gedreht", T, 320, 320, 150, 201, false, [] { orient(false); fillSeries(Series::Steady); atTemp(199); ui_draw_soldering_basic_status(false); }},
      {"d1_details", T, 318, 320, 520, 201, false, [] { fillSeries(Series::Steady); atTemp(199); ui_draw_soldering_power_status(false); }},
      {"d2_details_einbruch", T, 291, 320, 780, 201, false, [] { fillSeries(Series::Drop); atTemp(199); ui_draw_soldering_power_status(false); }},
      {"t1_temp", T, 318, 320, 150, 201, false, [] { fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
      {"t2_preset300", T, 318, 300, 150, 201, false, [] { fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
      {"t3_preset350", T, 318, 350, 150, 201, false, [] { fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
      {"t4_preset400", T, 318, 400, 150, 201, false, [] { fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
      {"t5_halten", T, 318, 320, 150, 201, false, [] { fillSeries(Series::Steady); pressed(true, 150 + 270); dogDrawTemperatureView(true); }},
      {"r3_temp_gedreht", T, 318, 350, 150, 201, false, [] { orient(false); fillSeries(Series::Steady); dogDrawTemperatureView(true); }},
      {"s6_temp", T, 26, 320, 0, 201, false, [] { dogDrawTemperatureView(false); }},
  };
}

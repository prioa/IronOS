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
static void     orient(bool frontRight);

volatile TemperatureType_t                    currentTempTargetDegC = 320;
expMovingAverage<uint32_t, wattHistoryFilter> x10WattHistory;
TickType_t                                    lastButtonTime   = 0;
TickType_t                                    lastMovementTime = 0;

uint16_t          getSettingValue(const enum SettingsOptions option) { return settings[option]; }
void              setSettingValue(const enum SettingsOptions option, const uint16_t v) { settings[option] = v; }
void              saveSettings() {}
TemperatureType_t TipThermoModel::getTipInC(bool) { return tipC; }
TemperatureType_t TipThermoModel::getTipInF(bool) { return tipC * 9 / 5 + 32; }
TemperatureType_t TipThermoModel::convertFtoC(TemperatureType_t f) { return (f - 32) * 5 / 9; }
TemperatureType_t getTipTemp(void) { return tipC; }
uint16_t          getInputVoltageX10(uint16_t, uint8_t) { return vX10; }
int16_t           getHandleTemperature(uint8_t) { return 310; }
uint8_t           getTipResistanceX10() { return 62; }
bool              isTipDisconnected() { return tipMissing; }
bool              getIsPoweredByDCIN() { return false; }
int8_t            getPowerSourceNumber(void) { return 3; }
uint32_t          getSleepTimeout(void) { return 60 * TICKS_SECOND; }
static int32_t    limitX10 = 1000;
int32_t           getX10WattageLimits() { return limitX10; }
uint8_t           X10WattsToPWM(int32_t w, uint8_t) { return w > 255 ? 255 : w; }
ButtonState       getButtonState() { return BUTTON_NONE; }
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
enum class Series { Steady, Dip, Drop, Heat, Boost };
static void fillSeries(Series kind) {
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
    }
    previewTicks = now - (71 - i) * 250;
    dogCurveSample(t);
  }
  previewTicks = now;
}

// The set point was reached this many seconds ago
static void atTemp(uint32_t seconds) { dogSolderTimerStart = (previewTicks - seconds * 1000 - 1) | 1; }

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  const DogKey solderKey = {iconPlay8, "LÖTEN"}, menuKey = {iconMenu, "MENÜ"};
  return {
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
      {"c1_schlafen_einschlafen", T, 150, 320, 30, 201, false, [] { dogSleepEnterTime = previewTicks - 400; ui_draw_soldering_basic_sleep(150); }},
      {"c2_schlafen", T, 150, 320, 30, 201, false, [] { dogSleepEnterTime = 0; ui_draw_soldering_basic_sleep(150); }},
      {"c3_schlafen_detail", T, 150, 320, 30, 201, true, [] { dogSleepEnterTime = 0; ui_draw_soldering_detailed_sleep(150); }},
      {"d10_start_hinweis", 6000000 + 1000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d11_start_spannung", 6000000 + 4000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d12_start_loetzeit", 6000000 + 7000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d13_start_zuletzt", 6000000 + 10000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d14_start_maxwatt", 6000000 + 13000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d15_start_griff", 6000000 + 16000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d16_start_widerstand", 6000000 + 19000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d17_start_version", 6000000 + 22000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = false; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d20_start_gedreht_hinweis", 7008000 + 1000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d21_start_gedreht_spannung", 7008000 + 4000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d22_start_gedreht_loetzeit", 7008000 + 7000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d23_start_gedreht_zuletzt", 7008000 + 10000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d24_start_gedreht_maxwatt", 7008000 + 13000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d25_start_gedreht_griff", 7008000 + 16000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d26_start_gedreht_widerstand", 7008000 + 19000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d27_start_gedreht_version", 7008000 + 22000, 26, 320, 0, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_homescreen_simplified(26); OLED::inLeftHandedMode = false; }},
      {"d30_start_aufgenommen_rennt", 8000000 + 400, 26, 320, 0, 201, false, [] { lastMovementTime = 8000000; ui_draw_homescreen_simplified(26); }},
      {"d31_start_aufgenommen_bellt", 8000000 + 1400, 26, 320, 0, 201, false, [] { ui_draw_homescreen_simplified(26); }},
      {"d4_start_heiss", 8100000 + 1500, 180, 320, 0, 201, false, [] { ui_draw_homescreen_simplified(180); }},
      {"d5_start_ohne_spitze", 8100000 + 1500, 26, 320, 0, 201, false, [] { tipMissing = true; ui_draw_homescreen_simplified(26); }},
      {"d6_start_detail", T, 26, 320, 0, 201, true, [] { ui_draw_homescreen_detailed(26); }},
      {"e1_warnung_gesperrt", T, 26, 320, 0, 201, false, [] { warnUser(translatedString(Tr->WarningKeysLockedString), BUTTON_NONE); }},
      {"e2_warnung_entsperrt", T, 26, 320, 0, 201, false, [] { warnUser(translatedString(Tr->UnlockingKeysString), BUTTON_NONE); }},
  };
}

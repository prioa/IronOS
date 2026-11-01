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

volatile TemperatureType_t                    currentTempTargetDegC = 320;
expMovingAverage<uint32_t, wattHistoryFilter> x10WattHistory;
TickType_t                                    lastButtonTime   = 0;
TickType_t                                    lastMovementTime = 0;

uint16_t          getSettingValue(const enum SettingsOptions option) { return settings[option]; }
void              setSettingValue(const enum SettingsOptions option, const uint16_t v) { settings[option] = v; }
void              saveSettings() {}
TemperatureType_t TipThermoModel::getTipInC(bool) { return tipC; }
TemperatureType_t TipThermoModel::getTipInF(bool) { return tipC * 9 / 5 + 32; }
TemperatureType_t getTipTemp(void) { return tipC; }
uint16_t          getInputVoltageX10(uint16_t, uint8_t) { return vX10; }
int16_t           getHandleTemperature(uint8_t) { return 310; }
uint8_t           getTipResistanceX10() { return 62; }
bool              isTipDisconnected() { return tipMissing; }
bool              getIsPoweredByDCIN() { return false; }
int8_t            getPowerSourceNumber(void) { return 3; }
uint32_t          getSleepTimeout(void) { return 60 * TICKS_SECOND; }
int32_t           getX10WattageLimits() { return 1000; }
uint8_t           X10WattsToPWM(int32_t w, uint8_t) { return w > 255 ? 255 : w; }
ButtonState       getButtonState() { return BUTTON_NONE; }
bool              I2CBB1::I2C_RegisterWrite(uint8_t, uint8_t, uint8_t) { return true; }
bool              I2CBB1::Mem_Write(uint16_t, uint16_t, const uint8_t *, uint16_t) { return true; }

void previewSetup() {
  settings[SettingsOptions::VoltageDiv]         = 700;
  settings[SettingsOptions::BoostTemp]          = 420;
  settings[SettingsOptions::TotalSolderMinutes] = 42 * 60 + 17;
  ui_pre_render_assets();
}

void previewApply(const PreviewScene &s) {
  tipC                                         = s.tipC;
  settings[SettingsOptions::SolderingTemp]     = s.setC;
  currentTempTargetDegC                        = s.setC;
  x10WattHistory.sum                           = s.x10W;
  vX10                                         = s.vX10;
  settings[SettingsOptions::DetailedSoldering] = s.detailed;
  settings[SettingsOptions::DetailedIDLE]      = s.detailed;
  tipMissing                                   = false;
  dogSolderTimerStart = dogBarkUntil = dogWakeUntil = dogPresetShownUntil = 0;
  dogHeatupStartTempC                                                     = 25;
  dogLastSessionSeconds                                                   = 27 * 60 + 12;
}

// Simulated tip temperature: heat-up, then a dip when soldering a big pad
static void fillCurve() {
  dogCurveReset();
  for (int i = 0; i < 72; i++) {
    previewTicks += 250;
    uint16_t t = i < 20 ? 25 + i * 15 : 320;
    if (i > 40 && i < 52) t = 320 - (i < 46 ? (i - 40) * 7 : (52 - i) * 6);
    dogCurveSample(t);
  }
}

std::vector<PreviewScene> previewScenes() {
  const uint32_t T = 200000; // a time well away from the occasional wag
  return {
      {"a1_loeten_aufheizen", T, 180, 320, 900, 201, false, [] { ui_draw_soldering_basic_status(false); }},
      {"a2_loeten_wuff", T, 318, 320, 300, 201, false, [] { dogSolderTimerStart = 1; dogBarkUntil = previewTicks + 1000; ui_draw_soldering_basic_status(false); }},
      {"a3_loeten_timer", T, 320, 320, 150, 201, false, [] { dogSolderTimerStart = previewTicks - 199000; ui_draw_soldering_basic_status(false); }},
      {"a4_loeten_hecheln", T + 300, 300, 320, 700, 201, false, [] { dogSolderTimerStart = 1; ui_draw_soldering_basic_status(false); }},
      {"a6_loeten_boost", T, 400, 320, 800, 201, false, [] { ui_draw_soldering_basic_status(true); }},
      {"a61_loeten_boost_spaeter", T + 900, 410, 320, 800, 201, false, [] { ui_draw_soldering_basic_status(true); }},
      {"a11_loeten_aufheizen_gedreht", T, 180, 320, 900, 201, false, [] { OLED::inLeftHandedMode = true; ui_draw_soldering_basic_status(false); OLED::inLeftHandedMode = false; }},
      {"a7_loeten_linkshaender", T, 320, 320, 150, 201, false, [] { OLED::inLeftHandedMode = true; dogSolderTimerStart = previewTicks - 75000; ui_draw_soldering_basic_status(false); OLED::inLeftHandedMode = false; }},
      {"b5_kurve_gedreht", T, 318, 320, 452, 201, false, [] { OLED::inLeftHandedMode = true; fillCurve(); ui_draw_soldering_power_status(false); OLED::inLeftHandedMode = false; }},
      {"b1_detail_kurve", T, 318, 320, 452, 201, true, [] { fillCurve(); dogSolderTimerStart = previewTicks - 199000; ui_draw_soldering_power_status(false); }},
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
      {"f1_preset_300", T, 320, 300, 150, 201, false, [] { dogPresetShownUntil = previewTicks + 500; ui_draw_soldering_basic_status(false); }},
      {"f2_preset_350", T, 320, 350, 150, 201, false, [] { dogPresetShownUntil = previewTicks + 500; ui_draw_soldering_basic_status(false); }},
      {"f3_preset_400", T, 320, 400, 150, 201, false, [] { dogPresetShownUntil = previewTicks + 500; ui_draw_soldering_basic_status(false); }},
      {"e1_warnung_gesperrt", T, 26, 320, 0, 201, false, [] { warnUser(translatedString(Tr->WarningKeysLockedString), BUTTON_NONE); }},
      {"e2_warnung_entsperrt", T, 26, 320, 0, 201, false, [] { warnUser(translatedString(Tr->UnlockingKeysString), BUTTON_NONE); }},
  };
}

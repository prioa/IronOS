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
      {"a5_loeten_preset", T, 320, 350, 150, 201, false, [] { dogPresetShownUntil = previewTicks + 500; ui_draw_soldering_basic_status(false); }},
      {"a6_loeten_boost", T, 400, 320, 800, 201, false, [] { ui_draw_soldering_basic_status(true); }},
      {"a7_loeten_linkshaender", T, 320, 320, 150, 201, false, [] { OLED::inLeftHandedMode = true; dogSolderTimerStart = previewTicks - 75000; ui_draw_soldering_basic_status(false); OLED::inLeftHandedMode = false; }},
      {"b1_detail_kurve", T, 318, 320, 452, 201, true, [] { fillCurve(); dogSolderTimerStart = previewTicks - 199000; ui_draw_soldering_power_status(false); }},
      {"c1_schlafen_hinlegen", T, 150, 320, 30, 201, false, [] { dogSleepEnterTime = previewTicks - 800; ui_draw_soldering_basic_sleep(150); }},
      {"c2_schlafen", T, 150, 320, 30, 201, false, [] { dogSleepEnterTime = 0; ui_draw_soldering_basic_sleep(150); }},
      {"c3_schlafen_detail", T, 150, 320, 30, 201, true, [] { dogSleepEnterTime = 0; ui_draw_soldering_detailed_sleep(150); }},
      {"d1_start", T, 26, 320, 0, 201, false, [] { lastMovementTime = 0; ui_draw_homescreen_simplified(26); }},
      {"d2_start_wuff", T + 10000, 26, 320, 0, 201, false, [] { lastMovementTime = previewTicks - 200; ui_draw_homescreen_simplified(26); }},
      {"d3_start_heiss", T + 20000, 180, 320, 0, 201, false, [] { ui_draw_homescreen_simplified(180); }},
      {"d4_start_ohne_spitze", T + 30000, 26, 320, 0, 201, false, [] { tipMissing = true; ui_draw_homescreen_simplified(26); }},
      {"d5_start_detail", T, 26, 320, 0, 201, true, [] { ui_draw_homescreen_detailed(26); }},
      {"e1_warnung", T, 26, 320, 0, 201, false, [] { warnUser(translatedString(Tr->WarningKeysLockedString), BUTTON_NONE); }},
  };
}

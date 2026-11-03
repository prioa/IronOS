/*
 * TS101_DOG_UI interface v2: the home screen, heater off
 *
 *   Platine bleifrei            24°      A the solder picked               D the reading
 *   [AUS] 20V 65W                ↓       B AUS, or HEISS above 50 C;       E the arrow while it cools
 *   ......curve.............   [≡] [▶]     behind it the supply           F menu, start
 */
#include "ts101_v2_solder.hpp"
#ifdef TS101_DOG_UI
#include "OperatingModes.h"
#include "power.hpp"
#include "Translation.h"
#include "ui_drawing.hpp"
#include <string.h>

extern int32_t powerSupplyWattageLimit; // the PD contract in watts, 0 without one

#define HOT_FROM_C       50
#define COOLING_SAMPLES  40 // 5 s back
#define COOLING_DROP_C   2  // this much colder than 5 s ago: it cools
#define COOLING_BELOW_C  35 // and near room temperature it no longer counts

static TickType_t noticeUntil = 0;
static bool       noticeLocked;

void dogHomeLockNotice(bool locked) {
  noticeUntil  = xTaskGetTickCount() + TICKS_SECOND * 3 / 2;
  noticeLocked = locked;
}

bool dogHomeCooling() {
  const uint8_t count = dogCurveCount();
  if (count < 2) {
    return false;
  }
  const uint8_t  back = count > COOLING_SAMPLES ? COOLING_SAMPLES : count - 1;
  const uint16_t now  = dogCurveAt(0);
  return now >= COOLING_BELOW_C && now + COOLING_DROP_C <= dogCurveAt(back);
}

// "20V 65W": the voltage there is, and what the PD contract allows; without one only the voltage
static void formatSupply(char *out) {
  const uint32_t x10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  out                = dogFormatUnit(out, (x10 + 5) / 10, "V");
  if (powerSupplyWattageLimit > 0) {
    *out++ = ' ';
    dogFormatUnit(out, powerSupplyWattageLimit, "W");
  }
}

void dogDrawHomeV2(uint16_t shownTemperature) {
  dogCurveSample(TipThermoModel::getTipInC()); // the curve goes on from soldering
  gridName(dogSolderName());
  char text[16];
  if (xTaskGetTickCount() < noticeUntil) {
    if (!getSettingValue(SettingsOptions::LockingMode)) {
      gridWarn(DOG_T("! SPERRE IM MENÜ AUS", "! LOCK OFF IN MENU"));
    } else {
      gridState(DOG_T("SPERRE", "LOCK"), noticeLocked ? DOG_T("AN", "ON") : DOG_T("AUS", "OFF"));
    }
  } else {
    formatSupply(text);
    gridState(TipThermoModel::getTipInC() > HOT_FROM_C ? DOG_T("HEISS", "HOT") : DOG_T("AUS", "OFF"), text);
  }
  gridCurve(false, 0);
  gridTemp(shownTemperature);
  if (dogHomeCooling()) {
    gridTarget(-1, 0);
  }
  gridKeys(&iconV2Menu, &iconV2Play);
}
#endif

#ifdef TS101_DOG_UI
/*
 * Warnings. Not critical: only row 2, the whole of it inverted, starting with "!". Critical: also the name row says what
 * happened, the arrow down (the heater is off) and one wide key, since both buttons do the same.
 */
void dogDrawCritical(const char *name, const char *row2, uint16_t shownTemperature, const char *key) {
  dogCurveSample(TipThermoModel::getTipInC());
  gridName(name);
  gridWarn(row2);
  gridCurve(false, 0);
  gridTemp(shownTemperature);
  gridTarget(-1, 0);
  if (key) {
    gridWideKey(key);
  } else {
    gridKeys(&iconV2Menu, nullptr); // no tip: the menu still opens, there is nothing to start
  }
}

// The stock texts are stored in the IronOS fonts' own encoding, not as letters: the ones the TS101 can show get their own words
struct DogStockWarning {
  const char *de, *en;
};
static bool stockWarning(const char *text, DogStockWarning &out) {
  const struct {
    const char     *stock;
    DogStockWarning own;
  } table[] = {
      {translatedString(Tr->SettingsResetMessage), {"! EINSTELLUNGEN ZURÜCKGESETZT", "! SETTINGS RESET"}},
      {translatedString(Tr->NoAccelerometerMessage), {"! KEIN LAGESENSOR", "! NO MOTION SENSOR"}},
      {translatedString(Tr->NoPowerDeliveryMessage), {"! KEIN USB-PD-CHIP", "! NO USB-PD CHIP"}},
      {translatedString(Tr->DeviceFailedValidationWarning), {"! GERÄT NICHT ECHT?", "! DEVICE NOT GENUINE?"}},
      {translatedString(Tr->CalibrationDone), {"! KALIBRIERT", "! CALIBRATED"}},
      {translatedString(Tr->ResetOKMessage), {"! ZURÜCKGESETZT", "! RESET DONE"}},
  };
  for (const auto &row : table) {
    if (row.stock == text) {
      out = row.own;
      return true;
    }
  }
  return false;
}

// The stock firmware's warnings (settings reset, no sensor, calibration and the like) as a warning in row 2 of the home screen.
// Wider than the row it runs through it, with a pause at both ends
bool dogWarnUser(const char *text) {
  DogStockWarning own;
  if (!stockWarning(text, own)) {
    return false;
  }
  const char *row = DOG_T(own.de, own.en);
  static TickType_t shownSince = 0, lastShown = 0;
  const TickType_t  now        = xTaskGetTickCount();
  if (now - lastShown > TICKS_SECOND / 2) {
    shownSince = now; // a new warning: the text from its start
  }
  lastShown = now;
  OLED::clearScreen();
  dogCurveSample(TipThermoModel::getTipInC());
  gridName(dogSolderName());
  const int16_t width = dogTextWidth(dogFontName, row) + 2;
  int16_t       shift = 0;
  if (width > GRID_LEFT_END + 1) {
    const int16_t  over   = width - GRID_LEFT_END - 1;
    const uint32_t pause  = 20;                     // frames at 25 a second
    const uint32_t period = pause * 2 + over * 2;   // 2 frames a pixel
    const uint32_t t      = ((xTaskGetTickCount() - shownSince) / (TICKS_SECOND / 25)) % period;
    shift                 = t < pause ? 0 : (t < pause + over * 2 ? (t - pause) / 2 : over);
  }
  gridClip(0, GRID_LEFT_END);
  dogText(dogFontName, 1 - shift, GRID_ROW2_BASE, row);
  gridInvert(0, GRID_ROW2_TOP, GRID_LEFT_END, GRID_ROW2_BOT);
  gridClipAll();
  gridCurve(false, 0);
  gridTemp(getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC());
  gridWideKey("OK");
  return true;
}
#endif

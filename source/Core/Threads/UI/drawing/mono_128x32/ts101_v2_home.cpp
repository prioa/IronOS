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

/*
 * TS101_DOG_UI: soldering, sleep and home screens with the German Shepherd.
 * They replace the dense 128x32 versions; the two halves swap over with the screen orientation,
 * like the stock layouts, so the temperature stays on the same side as there.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ui_drawing.hpp"

extern TickType_t lastMovementTime;

static const uint8_t halfWidth = OLED_WIDTH / 2;

// Boost: the dog runs across the whole screen, below it the tip temperature and the boost target
static void drawBoostScreen() {
  dogDrawRunAcross();
  dogDrawFlame(OLED_WIDTH - 8);
  OLED::setCursor((OLED_WIDTH - 66) / 2, 24); // 320°C  +420°
  ui_draw_tip_temperature(true, FontStyle::TINY);
  OLED::print(SmallSymbolSpace, FontStyle::TINY);
  OLED::print(SmallSymbolPlus, FontStyle::TINY);
  OLED::printNumber(getSettingValue(SettingsOptions::BoostTemp), 3, FontStyle::TINY);
  OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
}

/*
 * Simple soldering view, heating up: the dog runs until the set point is reached
 *   [ running dog 60px ][  180°C (large) ]
 *   [ heat-up bar                   320° ]
 * At temperature: its head, barking once, flicking an ear after waking up and panting while the heater works hard
 *   [ head ][     320°C (large)      ]
 *   [ 34px ][     session time       ]
 * The blinking flame in the top corner next to the temperature says the iron is in soldering mode.
 */
void ui_draw_soldering_basic_status(bool boostModeOn) {
  if (dogDrawPresetNotice()) {
    return;
  }
  if (boostModeOn) {
    drawBoostScreen();
    return;
  }
  const bool    flip    = OLED::getRotation();
  const bool    heating = !dogSolderTimerStart;
  const uint8_t dogW    = heating ? DOG_RUN_WIDTH : DOG_HEAD_WIDTH;
  const int16_t areaX   = flip ? dogW : 0;
  const uint8_t areaW   = OLED_WIDTH - dogW;
  const int16_t flameX  = flip ? OLED_WIDTH - 8 : OLED_WIDTH - dogW - 9;

  // Three large digits and the 12 px °C symbol, centred; with the dog on the right the flame shares the area
  OLED::setCursor(flip ? areaX + (areaW - 48) / 2 : (areaW - 9 - 48) / 2, 0);
  ui_draw_tip_temperature(true, FontStyle::LARGE);
  dogDrawFlame(flameX);

  if (heating) {
    dogDrawRunning(flip ? 0 : OLED_WIDTH - DOG_RUN_WIDTH, 0);
    const uint8_t  width  = 94;
    const uint8_t  barX   = 2;
    const uint16_t tip    = TipThermoModel::getTipInC();
    const uint16_t target = currentTempTargetDegC;
    uint32_t       len    = 0;
    if (target > dogHeatupStartTempC && tip > dogHeatupStartTempC) {
      len = ((uint32_t)(tip - dogHeatupStartTempC) * width) / (target - dogHeatupStartTempC);
      if (len > width) {
        len = width;
      }
    }
    OLED::drawFilledRect(barX, 25, barX + width + 1, 25, false);
    OLED::drawFilledRect(barX, 30, barX + width + 1, 30, false);
    OLED::drawFilledRect(barX, 25, barX, 30, false);
    OLED::drawFilledRect(barX + width + 1, 25, barX + width + 1, 30, false);
    if (len) {
      OLED::drawFilledRect(barX + 1, 27, barX + len, 28, false);
    }
    OLED::setCursor(barX + width + 6, 24);
    OLED::printNumber(getSettingValue(SettingsOptions::SolderingTemp), 3, FontStyle::TINY);
    OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
  } else {
    const DogPose pose = dogSolderingPose(x10WattHistory.average(), getX10WattageLimits());
    dogDrawHead(flip ? 0 : OLED_WIDTH - DOG_HEAD_WIDTH, pose);
    if (pose == DogPose::Bark) {
      dogDrawWuff(flip ? DOG_HEAD_WIDTH + 2 : flameX - DOG_WUFF_WIDTH + 1, 0);
    }
    dogPrintSessionTime(areaX + (areaW - 33) / 2, 24);
  }
}

static void hRule(uint8_t y) { OLED::drawFilledRect(0, y, OLED_WIDTH - 1, y, false); }
static void vRule(uint8_t x, uint8_t y0, uint8_t y1) { OLED::drawFilledRect(x, y0, x, y1, false); }

static uint8_t digitCount(uint32_t v) {
  uint8_t n = 1;
  while (v >= 10) {
    v /= 10;
    n++;
  }
  return n;
}

static uint32_t ampsX10() {
  uint32_t voltX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  return voltX10 ? (x10WattHistory.average() * 10) / voltX10 : 0;
}

static void printTinyX10(uint32_t x10, const char *unit) {
  OLED::printNumber(x10 / 10, digitCount(x10 / 10), FontStyle::TINY);
  OLED::print(SmallSymbolDot, FontStyle::TINY);
  OLED::printNumber(x10 % 10, 1, FontStyle::TINY);
  OLED::print(unit, FontStyle::TINY);
}

// Bottom row "52W | 20.1V | 2.2A", three centred cells split by short rules
static void drawPowerRow() {
  const uint32_t watts = (x10WattHistory.average() + 5) / 10;
  const uint32_t volts = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  const uint32_t amps  = ampsX10();
  const uint8_t  cellW = OLED_WIDTH / 3;
  OLED::setCursor((cellW - (digitCount(watts) + 1) * 6) / 2, 24);
  OLED::printNumber(watts, digitCount(watts), FontStyle::TINY);
  OLED::print(SmallSymbolWatts, FontStyle::TINY);
  OLED::setCursor(cellW + (cellW - (digitCount(volts / 10) + 3) * 6) / 2, 24);
  printTinyX10(volts, SmallSymbolVolts);
  OLED::setCursor(2 * cellW + (cellW - (digitCount(amps / 10) + 3) * 6) / 2, 24);
  printTinyX10(amps, SmallSymbolAmps);
  vRule(cellW, 26, 29);
  vRule(2 * cellW, 26, 29);
}

// Vertical power bar, 4 segments filling from the bottom of rows 8..18, under the flame
static void drawPowerBar(int16_t x) {
  const int32_t limit = getX10WattageLimits();
  uint32_t      lit   = limit > 0 ? (x10WattHistory.average() * 4 + limit / 2) / limit : 0;
  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t yBottom = 18 - i * 3;
    if (i < lit) {
      OLED::drawFilledRect(x, yBottom - 1, x + 5, yBottom, false);
    } else {
      OLED::drawFilledRect(x, yBottom, x, yBottom, false);
      OLED::drawFilledRect(x + 5, yBottom, x + 5, yBottom, false);
    }
  }
}

/*
 * Curve view:
 *   [ temperature curve 55px ] | [ 320°C (large) ] [power bar]
 *   ------------------------------------------------------------
 *
 *   [   52W    |    20.1V    |    2.2A   ]
 */
void ui_draw_soldering_power_status(bool boostModeOn) {
  (void)boostModeOn;
  const bool    flip   = OLED::getRotation();
  const uint8_t curveW = 55;
  if (dogDrawPresetNotice()) {
    return;
  }
  dogDrawCurve(flip ? 0 : OLED_WIDTH - curveW, curveW, 19, currentTempTargetDegC);
  vRule(flip ? curveW + 2 : OLED_WIDTH - curveW - 3, 0, 18);
  OLED::setCursor(flip ? curveW + 6 : 13, 0);
  ui_draw_tip_temperature(true, FontStyle::LARGE);
  drawPowerBar(flip ? OLED_WIDTH - 7 : 1);
  dogDrawFlame(flip ? OLED_WIDTH - 8 : curveW + 7);
  hRule(20);
  drawPowerRow();
}

/*
 * Sleep: the dog lies there awake for a moment, closes its eyes and "z Z Z" drifts up from its head;
 * on the right the tip temperature, input voltage and how long it has been asleep
 */
static void drawSleepingDog() {
  const TickType_t inSleep = xTaskGetTickCount() - dogSleepEnterTime;
  if (inSleep < TICKS_100MS * 8) {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLyingAwake);
  } else {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLying);
    uint8_t step = (xTaskGetTickCount() / (TICKS_100MS * 6)) % DOG_ZZZ_STEPS;
    OLED::drawArea(50, 0, DOG_ZZZ_WIDTH, 8, dogZzz[step]);
  }
}

static void drawSleepScreen(TemperatureType_t tipTemp) {
  const int16_t x = 85;
  OLED::clearScreen();
  drawSleepingDog();
  OLED::setCursor(x + 2, 0);
  OLED::printNumber(tipTemp, 3, FontStyle::SMALL);
  OLED::printSymbolDeg(FontStyle::SMALL);
  OLED::drawArea(x, 16, 8, 8, iconBolt);
  OLED::setCursor(x + 9, 16);
  dogPrintVoltage();
  uint32_t seconds = (xTaskGetTickCount() - dogSleepEnterTime) / TICKS_SECOND;
  dogPrintTime(x, 24, dogClock, seconds / 60, seconds % 60);
  OLED::refresh();
}

void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }
void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }

/*
 * Home:
 *   [     [thermo] 320°C     ][ dog ]  <- set point; while still hot the tip temperature with a down arrow
 *   [▶] item, every 3 s  [⚙] [ head]  <- play next to its button, gear next to the head
 * Picking the iron up after it lay still makes the dog run across and bark; without a tip a full screen notice.
 */
#define HOME_STATUS_ITEMS 8

static void drawStatusItem(int16_t x, uint8_t w, uint8_t item) {
  const char *hint = "+LÖTEN -MENÜ";
  uint8_t     width;
  switch (item) {
  case 0:
    dogPrintLabel(x + (w - dogLabelWidth(hint)) / 2, 24, hint);
    break;
  case 1: { // input voltage
    uint32_t volts = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
    width          = 9 + (digitCount(volts / 10) + 3) * 6;
    OLED::drawArea(x + (w - width) / 2, 24, 8, 8, iconBolt);
    OLED::setCursor(x + (w - width) / 2 + 9, 24);
    printTinyX10(volts, SmallSymbolVolts);
    break;
  }
  case 2: { // lifetime soldering time, h:mm
    uint16_t minutes = getSettingValue(SettingsOptions::TotalSolderMinutes);
    width            = 9 + (digitCount(minutes / 60) + 3) * 6;
    dogPrintTotalTime(x + (w - width) / 2, 24);
    break;
  }
  case 3: { // last session, m:ss
    uint32_t m = dogLastSessionSeconds / 60;
    width      = 9 + (digitCount(m) + 3) * 6;
    dogPrintTime(x + (w - width) / 2, 24, dogClock, m, dogLastSessionSeconds % 60);
    break;
  }
  case 4: { // power limit
    int32_t  limit = getX10WattageLimits();
    uint16_t watts = limit > 0 ? limit / 10 : 0;
    width          = (3 + digitCount(watts) + 1) * 6;
    OLED::setCursor(x + (w - width) / 2, 24);
    OLED::print(SmallSymbolMax, FontStyle::TINY);
    OLED::printNumber(watts, digitCount(watts), FontStyle::TINY);
    OLED::print(SmallSymbolWatts, FontStyle::TINY);
    break;
  }
  case 5: { // handle temperature
    uint16_t handle = getHandleTemperature(0) / 10;
    width           = 9 + (digitCount(handle) + 1) * 6;
    OLED::drawArea(x + (w - width) / 2, 24, 8, 8, iconThermo8);
    OLED::setCursor(x + (w - width) / 2 + 9, 24);
    OLED::printNumber(handle, digitCount(handle), FontStyle::TINY);
    OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
    break;
  }
  case 6: { // tip resistance
    uint8_t ohmsX10 = getTipResistanceX10();
    width           = (digitCount(ohmsX10 / 10) + 3) * 6;
    OLED::setCursor(x + (w - width) / 2, 24);
    printTinyX10(ohmsX10, SmallSymbolOhm);
    break;
  }
  default: { // firmware version
    width = 9 + strlen(SmallSymbolVersionNumber) * 6;
    if (width > w) {
      width = w;
    }
    OLED::drawArea(x + (w - width) / 2, 24, 8, 8, iconPaw);
    OLED::setCursor(x + (w - width) / 2 + 9, 24);
    OLED::print(SmallSymbolVersionNumber, FontStyle::TINY, (w - 9) / 6);
    break;
  }
  }
}

void ui_draw_homescreen_simplified(TemperatureType_t tipTemp) {
  static TickType_t lastSeenMovement = 0;
  static TickType_t pickedUpAt       = 0;
  static bool       tempOnDisplay    = false;
  const bool        flip             = OLED::getRotation();
  const TickType_t  now              = xTaskGetTickCount();

  if (isTipDisconnected()) {
    dogDrawNotice(iconWarning, "SPITZE", "FEHLT!", nullptr);
    return;
  }
  if (lastMovementTime != lastSeenMovement) {
    if (lastMovementTime - lastSeenMovement > TICKS_SECOND * 5) {
      pickedUpAt = lastMovementTime; // picked up after lying still
    }
    lastSeenMovement = lastMovementTime;
  }

  // Dog: runs across for a second, then the head barks for a second and calms down
  const TickType_t sincePickup = now - pickedUpAt;
  const bool       running     = pickedUpAt && sincePickup < TICKS_SECOND;
  if (running) {
    dogDrawRunAcross();
  } else {
    bool bark = pickedUpAt && sincePickup < TICKS_SECOND * 2 && ((now / (TICKS_100MS * 3)) % 2);
    OLED::drawArea(flip ? 0 : OLED_WIDTH - DOG_HEAD_WIDTH, 0, DOG_HEAD_WIDTH, DOG_HEAD_HEIGHT, bark ? dogHeadBark : dogHead);
  }

  // Big value: the set point, or the tip temperature while it is still hot
  if (tipTemp > 55) {
    tempOnDisplay = true;
  } else if (tipTemp < 45) {
    tempOnDisplay = false;
  }
  const int16_t areaX  = flip ? DOG_HEAD_WIDTH : 0; // the part of the screen next to the head
  const uint8_t areaW  = OLED_WIDTH - DOG_HEAD_WIDTH;
  const int16_t valueX = areaX + (areaW - 58) / 2;
  if (running) {
    // The dog has the upper part of the screen to itself
  } else if (tempOnDisplay) {
    if (!(getSettingValue(SettingsOptions::CoolingTempBlink) && (now % 1000 < 300))) {
      OLED::drawArea(valueX + 2, 8, 8, 8, iconArrowDown8); // cooling down
      OLED::setCursor(valueX + 10, 0);
      ui_draw_tip_temperature(true, FontStyle::LARGE);
    }
  } else {
    OLED::drawArea(valueX, 8, 8, 16, iconThermo16);
    OLED::setCursor(valueX + 10, 0);
    OLED::printNumber(getSettingValue(SettingsOptions::SolderingTemp), 3, FontStyle::LARGE);
    OLED::printSymbolDeg(FontStyle::EXTRAS);
  }

  // Status bar: the start hint next to the front button, the settings hint next to the head
  OLED::drawArea(flip ? OLED_WIDTH - 8 : 0, 24, 8, 8, iconPlay8);
  OLED::drawArea(flip ? DOG_HEAD_WIDTH + 1 : OLED_WIDTH - DOG_HEAD_WIDTH - 9, 24, 8, 8, iconGear8);
  if (tempOnDisplay) {
    dogPrintLabel(areaX + (areaW - dogLabelWidth("HEISS!")) / 2, 24, "HEISS!");
  } else {
    drawStatusItem(areaX + 9, areaW - 18, (now / (TICKS_SECOND * 3)) % HOME_STATUS_ITEMS);
  }
}
#endif

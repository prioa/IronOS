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
extern uint8_t    buttonAF[sizeof(buttonA)];
extern uint8_t    disconnectedTipF[sizeof(disconnectedTip)];

static const uint8_t halfWidth = OLED_WIDTH / 2;

/*
 * Simple soldering view:
 *   [ sitting dog 60px ][ 320°C  (large)          ]
 *   [                  ][ heat-up bar / timer      ]
 * The dog barks once the set point is reached, wags after waking up and pants while the heater works hard.
 */
void ui_draw_soldering_basic_status(bool boostModeOn) {
  const bool     flip     = OLED::getRotation();
  const int16_t  dogX     = flip ? 0 : OLED_WIDTH - DOG_SIT_WIDTH;
  const int16_t  textX    = flip ? halfWidth + 2 : 2;
  const uint32_t x10Watt  = x10WattHistory.average();
  const int32_t  x10Limit = getX10WattageLimits();
  const DogPose  pose     = dogSolderingPose(x10Watt, x10Limit);

  dogDrawSitting(dogX, pose);
  if (pose == DogPose::Bark) {
    dogDrawWuff(dogX, 0);
  }

  OLED::setCursor(textX, 0);
  if (dogPresetShownUntil && xTaskGetTickCount() < dogPresetShownUntil) {
    // A preset was just picked: show the new set point framed instead of the tip temperature
    OLED::printNumber(getSettingValue(SettingsOptions::SolderingTemp), 3, FontStyle::LARGE);
    OLED::printSymbolDeg(FontStyle::EXTRAS);
    OLED::drawFilledRect(textX, 0, textX + 55, 0, false);
    OLED::drawFilledRect(textX, 23, textX + 55, 23, false);
  } else {
    ui_draw_tip_temperature(true, FontStyle::LARGE);
  }

  // Bottom row: boost target, heat-up bar with the set point, or the session time once it is reached
  if (boostModeOn) {
    OLED::setCursor(textX, 24);
    OLED::print(SmallSymbolPlus, FontStyle::TINY);
    OLED::printNumber(getSettingValue(SettingsOptions::BoostTemp), 3, FontStyle::TINY);
    OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
  } else if (!dogSolderTimerStart) {
    const uint8_t  width  = 30;
    const uint16_t tip    = TipThermoModel::getTipInC();
    const uint16_t target = currentTempTargetDegC;
    uint32_t       len    = 0;
    if (target > dogHeatupStartTempC && tip > dogHeatupStartTempC) {
      len = ((uint32_t)(tip - dogHeatupStartTempC) * width) / (target - dogHeatupStartTempC);
      if (len > width) {
        len = width;
      }
    }
    OLED::drawFilledRect(textX, 25, textX + width + 1, 25, false);
    OLED::drawFilledRect(textX, 30, textX + width + 1, 30, false);
    OLED::drawFilledRect(textX, 25, textX, 30, false);
    OLED::drawFilledRect(textX + width + 1, 25, textX + width + 1, 30, false);
    if (len) {
      OLED::drawFilledRect(textX + 1, 27, textX + len, 28, false);
    }
    OLED::setCursor(textX + width + 4, 24);
    OLED::printNumber(getSettingValue(SettingsOptions::SolderingTemp), 3, FontStyle::TINY);
    OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
  } else {
    dogPrintSessionTime(textX, 24);
  }
}

/*
 * Detailed soldering view:
 *   [ temperature curve 72px ][ 320°C (large) ]
 *   [ 45W 20.1V 320° ⏱3:19                    ]
 */
void ui_draw_soldering_power_status(bool boostModeOn) {
  const bool    flip   = OLED::getRotation();
  const uint8_t curveW = 70;
  const int16_t curveX = flip ? 0 : OLED_WIDTH - curveW;
  const int16_t tempX  = flip ? curveW + 4 : 0;

  dogDrawCurve(curveX, curveW, 23, currentTempTargetDegC);

  OLED::setCursor(tempX, 0);
  ui_draw_tip_temperature(true, FontStyle::LARGE);

  OLED::setCursor(0, 24);
  {
    uint32_t x10Watt = x10WattHistory.average();
    OLED::printNumber((x10Watt + 5) / 10, x10Watt >= 995 ? 3 : 2, FontStyle::TINY);
    OLED::print(SmallSymbolWatts, FontStyle::TINY);
    OLED::print(SmallSymbolSpace, FontStyle::TINY);
  }
  dogPrintVoltage();
  OLED::print(SmallSymbolSpace, FontStyle::TINY);
  OLED::print(boostModeOn ? SmallSymbolPlus : SmallSymbolSpace, FontStyle::TINY);
  OLED::printNumber(getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp), 3, FontStyle::TINY);
  OLED::print(SmallSymbolDegreeSign, FontStyle::TINY);
  if (dogSolderTimerStart) {
    dogPrintSessionTime(OLED::getCursorX() + 3, 24);
  }
}

/*
 * Sleep: the dog lies down, falls asleep and "z Z Z" drifts up from its head; tip temperature next to it
 */
static void drawSleepingDog() {
  const TickType_t inSleep = xTaskGetTickCount() - dogSleepEnterTime;
  if (inSleep < TICKS_100MS * 5) {
    dogDrawSitting(0, DogPose::Sit);
  } else if (inSleep < TICKS_100MS * 13) {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLyingAwake);
  } else {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLying);
    uint8_t step = (xTaskGetTickCount() / (TICKS_100MS * 6)) % DOG_ZZZ_STEPS;
    OLED::drawArea(52, 0, DOG_ZZZ_WIDTH, 8, dogZzz[step]);
  }
}

void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  drawSleepingDog();
  OLED::setCursor(80, 0);
  OLED::printNumber(tipTemp, 3, FontStyle::SMALL);
  OLED::printSymbolDeg(FontStyle::SMALL);
  OLED::setCursor(98, 24);
  dogPrintVoltage();
  OLED::refresh();
}

void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  drawSleepingDog();
  OLED::setCursor(78, 4); // 3 large digits + degree symbol, flush right; y=4 centres the 24px font
  OLED::printNumber(tipTemp, 3, FontStyle::LARGE);
  OLED::printSymbolDeg(FontStyle::EXTRAS);
  OLED::refresh();
}

/*
 * Home: the start soldering pictogram (or the tip temperature while it is still hot) next to the dog,
 * which barks when the iron is picked up after lying still, and looks sad without a tip.
 */
void ui_draw_homescreen_simplified(TemperatureType_t tipTemp) {
  static TickType_t lastSeenMovement = 0;
  static TickType_t barkUntil        = 0;
  static bool       tempOnDisplay    = false;
  const bool        flip             = OLED::getRotation();
  const int16_t     buttonX          = flip ? 71 : 0;
  const int16_t     dogX             = flip ? 0 : OLED_WIDTH - DOG_SIT_WIDTH;
  const TickType_t  now              = xTaskGetTickCount();

  if (lastMovementTime != lastSeenMovement) {
    if (lastMovementTime - lastSeenMovement > TICKS_SECOND * 5) {
      barkUntil = lastMovementTime + TICKS_100MS * 15; // picked up after lying still: WUFF!
    }
    lastSeenMovement = lastMovementTime;
  }

  DogPose pose = DogPose::Sit;
  if (isTipDisconnected()) {
    pose = DogPose::Sad;
  } else if (now < barkUntil) {
    pose = ((now / (TICKS_100MS * 3)) % 2) ? DogPose::Sit : DogPose::Bark;
  } else if ((now % (TICKS_SECOND * 6)) < TICKS_100MS * 8) {
    pose = ((now / (TICKS_100MS * 2)) % 2) ? DogPose::Sit : DogPose::Wag;
  }
  dogDrawSitting(dogX, pose);
  if (pose == DogPose::Bark) {
    dogDrawWuff(dogX, 0);
  }

  if (tipTemp > 55) {
    tempOnDisplay = true;
  } else if (tipTemp < 45) {
    tempOnDisplay = false;
  }
  if (isTipDisconnected()) {
    OLED::drawArea(buttonX, 0, 56, 32, flip ? disconnectedTipF : disconnectedTip);
  } else if (tempOnDisplay) {
    if (!(getSettingValue(SettingsOptions::CoolingTempBlink) && (now % 1000 < 300))) {
      OLED::setCursor(flip ? 74 : 2, 4); // y=4 centres the 24px digits
      ui_draw_tip_temperature(true, FontStyle::LARGE);
    }
  } else {
    OLED::drawArea(buttonX, 0, 56, 32, flip ? buttonAF : buttonA);
  }
}
#endif

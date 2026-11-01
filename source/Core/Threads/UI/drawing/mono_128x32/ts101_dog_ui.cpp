#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "Settings.h"
#include "ui_drawing.hpp"

TickType_t dogSolderTimerStart = 0;
uint16_t   dogHeatupStartTempC = 0;
TickType_t dogPresetShownUntil = 0;
TickType_t dogBarkUntil        = 0;
TickType_t dogWakeUntil        = 0;
TickType_t dogSleepEnterTime   = 0;

void dogDrawSitting(int16_t x, DogPose pose) {
  const uint8_t *sprite = dogSit;
  switch (pose) {
  case DogPose::Wag:
    sprite = dogSitWag;
    break;
  case DogPose::Pant:
    sprite = dogSitPant;
    break;
  case DogPose::Bark:
    sprite = dogSitBark;
    break;
  case DogPose::Sad:
    sprite = dogSitSad;
    break;
  default:
    break;
  }
  OLED::drawArea(x, 0, DOG_SIT_WIDTH, DOG_SIT_HEIGHT, sprite);
}

void dogDrawWuff(int16_t x, int8_t y) { OLED::drawArea(x, y, DOG_WUFF_WIDTH, 8, dogWuff); }

void dogPrintTime(int16_t x, int8_t y, const uint8_t *icon, uint16_t major, uint8_t minor) {
  if (icon) {
    OLED::drawArea(x, y, 8, 8, icon);
    x += 9;
  }
  OLED::setCursor(x, y);
  OLED::printNumber(major, major >= 100 ? 3 : (major >= 10 ? 2 : 1), FontStyle::TINY);
  OLED::print(SmallSymbolColon, FontStyle::TINY);
  OLED::printNumber(minor, 2, FontStyle::TINY, false);
}

void dogPrintSessionTime(int16_t x, int8_t y) {
  uint32_t seconds = dogSolderTimerStart ? (xTaskGetTickCount() - dogSolderTimerStart) / TICKS_SECOND : 0;
  uint32_t minutes = seconds / 60;
  if (minutes < 100) {
    dogPrintTime(x, y, dogClock, minutes, seconds % 60);
  } else {
    dogPrintTime(x, y, dogClock, minutes / 60, minutes % 60);
  }
}

void dogPrintTotalTime(int16_t x, int8_t y) {
  uint16_t minutes = getSettingValue(SettingsOptions::TotalSolderMinutes);
  dogPrintTime(x, y, dogSigma, minutes / 60, minutes % 60);
}

void dogPrintVoltage() {
  uint32_t voltX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  OLED::printNumber(voltX10 / 10, 2, FontStyle::TINY);
  OLED::print(SmallSymbolDot, FontStyle::TINY);
  OLED::printNumber(voltX10 % 10, 1, FontStyle::TINY);
  OLED::print(SmallSymbolVolts, FontStyle::TINY);
}

DogPose dogSolderingPose(uint32_t x10Watt, int32_t x10Limit) {
  const TickType_t now = xTaskGetTickCount();
  if (now < dogBarkUntil) {
    return ((now / (TICKS_100MS * 3)) % 2) ? DogPose::Sit : DogPose::Bark;
  }
  if (now < dogWakeUntil) {
    return ((now / (TICKS_100MS * 2)) % 2) ? DogPose::Sit : DogPose::Wag;
  }
  if (x10Limit > 0 && x10Watt * 100 > (uint32_t)x10Limit * 40) {
    // Heating hard: pant
    return ((now / (TICKS_100MS * 3)) % 2) ? DogPose::Sit : DogPose::Pant;
  }
  // Two quick wags every six seconds
  uint32_t phase = now % (TICKS_SECOND * 6);
  if (phase < TICKS_100MS * 8) {
    return ((phase / (TICKS_100MS * 2)) % 2) ? DogPose::Sit : DogPose::Wag;
  }
  return DogPose::Sit;
}

// ---- Temperature curve ----
#define DOG_CURVE_SAMPLES      72
#define DOG_CURVE_SAMPLE_TICKS (TICKS_100MS * 2 + TICKS_100MS / 2) // 4 samples a second, 18 s on screen
static uint16_t   curve[DOG_CURVE_SAMPLES];
static uint8_t    curveHead  = 0; // next slot to write
static uint8_t    curveCount = 0;
static TickType_t curveLastSample;

void dogCurveReset() {
  curveHead  = 0;
  curveCount = 0;
}

void dogCurveSample(uint16_t tipC) {
  const TickType_t now = xTaskGetTickCount();
  if (curveCount && (now - curveLastSample) < DOG_CURVE_SAMPLE_TICKS) {
    return;
  }
  curveLastSample  = now;
  curve[curveHead] = tipC;
  curveHead        = (curveHead + 1) % DOG_CURVE_SAMPLES;
  if (curveCount < DOG_CURVE_SAMPLES) {
    curveCount++;
  }
}

void dogDrawCurve(int16_t x, uint8_t width, uint8_t height, uint16_t setC) {
  if (width > DOG_CURVE_SAMPLES) {
    width = DOG_CURVE_SAMPLES;
  }
  const uint8_t count = curveCount < width ? curveCount : width;
  // Vertical range: zoomed in round the set point so dips while soldering show up; it grows upwards for
  // overshoot, and anything colder (heating up) runs off the bottom
  int32_t lo = (int32_t)setC - 45, hi = (int32_t)setC + 15;
  for (uint8_t i = 0; i < count; i++) {
    uint16_t v = curve[(curveHead + DOG_CURVE_SAMPLES - count + i) % DOG_CURVE_SAMPLES];
    hi         = v > hi ? v : hi;
  }
  const uint8_t bottom = height - 1;
  auto          toY    = [&](int32_t v) -> uint8_t {
    if (v <= lo) {
      return bottom;
    }
    return bottom - ((v - lo) * bottom) / (hi - lo);
  };

  // Dotted set point line
  const uint8_t setY = toY(setC);
  for (uint8_t i = 0; i < width; i += 3) {
    OLED::drawFilledRect(x + i, setY, x + i, setY, false);
  }
  // Newest sample on the right; join neighbours with vertical strokes so steep changes stay visible
  const int16_t start = x + width - count;
  uint8_t       prevY = 0;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t y = toY(curve[(curveHead + DOG_CURVE_SAMPLES - count + i) % DOG_CURVE_SAMPLES]);
    if (i == 0) {
      prevY = y;
    }
    OLED::drawFilledRect(start + i, y < prevY ? y : prevY, start + i, y < prevY ? prevY : y, false);
    prevY = y;
  }
}

// ---- Lifetime soldering time ----
static TickType_t countLastTick = 0;
static uint32_t   countTicks    = 0;
static bool       countUnsaved  = false;

void dogCountSolderTime() {
  const TickType_t now   = xTaskGetTickCount();
  TickType_t       delta = now - countLastTick;
  countLastTick          = now;
  if (!dogSolderTimerStart || delta > TICKS_SECOND) {
    return; // not at temperature yet, or first call after a break
  }
  countTicks += delta;
  if (countTicks >= 60 * TICKS_SECOND) {
    countTicks -= 60 * TICKS_SECOND;
    uint16_t total = getSettingValue(SettingsOptions::TotalSolderMinutes);
    if (total < 65535) {
      setSettingValue(SettingsOptions::TotalSolderMinutes, total + 1);
      countUnsaved = true;
    }
  }
}

void dogSaveSolderTime() {
  if (countUnsaved) {
    countUnsaved = false;
    saveSettings();
  }
}
#endif

#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "Settings.h"
#include "ui_drawing.hpp"

TickType_t dogSolderTimerStart = 0;
uint16_t   dogHeatupStartTempC = 0;
TickType_t dogBarkUntil        = 0;
TickType_t dogSleepEnterTime   = 0;
uint32_t   dogLastSessionSeconds = 0;

void dogDrawWuff(int16_t x, int8_t y) { OLED::drawArea(x, y, DOG_WUFF_WIDTH, 8, dogWuff); }

void dogDrawRunning(int16_t x, int8_t y) {
  static const uint8_t cycle[] = {0, 1, 2, 3, 2, 1}; // stretched to gathered and back
  dogBlit(x, y, DOG_RUN_WIDTH, DOG_RUN_HEIGHT, dogRun[cycle[(xTaskGetTickCount() / (TICKS_100MS * 15 / 10)) % sizeof(cycle)]]);
}

void dogDrawRunAcross() {
  const uint32_t lap  = TICKS_SECOND * 2;
  const int32_t  span = OLED_WIDTH + DOG_RUN_WIDTH;
  dogDrawRunning((int32_t)((xTaskGetTickCount() % lap) * span / lap) - DOG_RUN_WIDTH, 0);
}

void dogDrawFlame(int16_t x) {
  if ((xTaskGetTickCount() / (TICKS_100MS * 5)) % 2 == 0) {
    OLED::drawArea(x, 0, 8, 8, iconFlame);
  }
}

char *dogFormatTime(char *out, uint16_t major, uint8_t minor) {
  out    = dogFormat(out, major);
  *out++ = ':';
  *out++ = '0' + minor / 10;
  *out++ = '0' + minor % 10;
  *out   = 0;
  return out;
}

void dogBlit(int16_t x, int8_t y, uint8_t width, uint8_t height, const uint8_t *data) {
  for (uint8_t page = 0; page < height / 8; page++) {
    for (uint8_t col = 0; col < width; col++) {
      uint8_t bits = data[page * width + col];
      for (uint8_t bit = 0; bits && bit < 8; bit++) {
        int16_t px = x + col, py = y + page * 8 + bit;
        if (((bits >> bit) & 1) && px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
          OLED::drawFilledRect(px, py, px, py, false);
        }
      }
    }
  }
}

void dogDrawNotice(const uint8_t *icon, const char *word, const char *footer) {
  OLED::clearScreen();
  dogBlit(1, 0, 24, 24, icon);
  OLED::drawFilledRect(0, 23, OLED_WIDTH - 1, 23, true);
  dogText(dogFontHead, 27 + (OLED_WIDTH - 27 - dogTextWidth(dogFontHead, word)) / 2, 20, word);
  dogSmallCentre(0, OLED_WIDTH - 1, footer);
}

bool dogWarnLocked(bool locked, ButtonState buttons) {
  dogDrawNotice(iconLock, locked ? DOG_T("GESPERRT", "LOCKED") : DOG_T("OFFEN", "OPEN"), locked ? DOG_T("LÖSEN: BEIDE HALTEN", "UNLOCK: HOLD BOTH") : DOG_T("TASTEN FREI", "BUTTONS FREE"));
  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) {
    return true;
  }
  return buttons != BUTTON_NONE;
}

DogAlert dogAlert = DogAlert::None;

const char *dogTipName() {
  // The order of tipType_t for the TS101: auto, then the three T12 tunings
  static const char *const de[] = {"AUTO", "TS100 LANG", "PINE KURZ", "PTS200"};
  static const char *const en[] = {"AUTO", "TS100 LONG", "PINE SHORT", "PTS200"};
  const uint16_t           type = getSettingValue(SettingsOptions::SolderingTipType);
  return type < sizeof(de) / sizeof(de[0]) ? (dogLangDE() ? de : en)[type] : "?";
}

bool dogHandleTooHot() {
  static TickType_t hotSince = 0;
  if (getHandleTemperature(0) < DOG_HANDLE_LIMIT_X10C) {
    hotSince = 0;
    return false;
  }
  if (!hotSince) {
    hotSince = xTaskGetTickCount() | 1;
  }
  return (xTaskGetTickCount() - hotSince) >= TICKS_SECOND * 2;
}

// ---- Temperature curve ----
#define DOG_CURVE_SAMPLES      72
static uint16_t   curve[DOG_CURVE_SAMPLES];
static uint8_t    curveHead  = 0; // next slot to write
static uint8_t    curveCount = 0;
static TickType_t curveLastSample;

void dogCurveReset() {
  curveHead  = 0;
  curveCount = 0;
}

void dogCurveSample(uint16_t tipC, TickType_t interval) {
  const TickType_t now = xTaskGetTickCount();
  if (curveCount && (now - curveLastSample) < interval) {
    return;
  }
  curveLastSample  = now;
  curve[curveHead] = tipC;
  curveHead        = (curveHead + 1) % DOG_CURVE_SAMPLES;
  if (curveCount < DOG_CURVE_SAMPLES) {
    curveCount++;
  }
}

void dogDrawCurve(int16_t x0, int16_t x1, uint8_t y0, uint8_t y1, uint16_t setC, DogCurve range, bool dots) {
  int32_t lo = (int32_t)setC - 45, hi = (int32_t)setC + 15; // zoomed in round the set point so dips while soldering show up
  if (range == DogCurve::Heat) {
    lo = 20;
    hi = (int32_t)setC + 5;
  } else if (range == DogCurve::Cool) {
    lo = 20;
    hi = 60;
    for (uint8_t i = 0; i < curveCount; i++) {
      hi = curve[i] > hi ? curve[i] : hi;
    }
  } else if (range == DogCurve::Boost) {
    lo = (int32_t)setC - 120;
    hi = (int32_t)setC + 10;
  } else {
    for (uint8_t i = 0; i < curveCount; i++) {
      hi = curve[i] > hi ? curve[i] : hi; // overshoot grows the range upwards
    }
  }
  auto toY = [&](int32_t v) -> uint8_t {
    v = v < lo ? lo : (v > hi ? hi : v);
    return y1 - ((v - lo) * (y1 - y0)) / (hi - lo);
  };
  if (dots) {
    const uint8_t setY = toY(setC);
    for (int16_t x = x0; x <= x1; x += 3) {
      OLED::drawFilledRect(x, setY, x, setY, false);
    }
  }
  // Every width shows the same 18 s; join neighbours with vertical strokes so steep changes stay visible
  const uint8_t width = x1 - x0 + 1;
  const uint8_t empty = DOG_CURVE_SAMPLES - curveCount;
  bool          first = true;
  uint8_t       prevY = 0;
  for (uint8_t i = 0; i < width; i++) {
    const uint8_t sample = ((uint16_t)i * DOG_CURVE_SAMPLES) / width;
    if (sample < empty) {
      continue;
    }
    const uint8_t y = toY(curve[(curveHead + sample - empty + DOG_CURVE_SAMPLES - curveCount) % DOG_CURVE_SAMPLES]);
    if (first) {
      prevY = y;
      first = false;
    }
    OLED::drawFilledRect(x0 + i, y < prevY ? y : prevY, x0 + i, y < prevY ? prevY : y, false);
    prevY = y;
  }
}

// ---- Lifetime soldering time ----
static TickType_t countLastTick = 0;
static uint32_t   countTicks    = 0;
static bool       settingsUnsaved  = false;

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
      settingsUnsaved = true;
    }
  }
}

void dogNextSolderView() {
  setSettingValue(SettingsOptions::DogSolderView, (getSettingValue(SettingsOptions::DogSolderView) + 1) % 2);
  settingsUnsaved = true;
}

void dogSettingsChanged() { settingsUnsaved = true; }

void dogSessionEnded() {
  if (dogSolderTimerStart) {
    dogLastSessionSeconds = (xTaskGetTickCount() - dogSolderTimerStart) / TICKS_SECOND;
    dogSolderTimerStart   = 0;
  }
  if (settingsUnsaved) {
    settingsUnsaved = false;
    saveSettings();
  }
}
#endif

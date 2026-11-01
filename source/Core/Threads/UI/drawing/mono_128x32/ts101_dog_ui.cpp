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

static const uint8_t *labelGlyph(const char *text, uint8_t *bytes) {
  for (uint8_t g = 0; g < DOG_LABEL_GLYPHS; g++) {
    const char *c   = dogLabelChars[g];
    uint8_t     len = c[1] ? 2 : 1;
    if (text[0] == c[0] && (len == 1 || text[1] == c[1])) {
      *bytes = len;
      return &dogLabelFont[g * 5];
    }
  }
  *bytes = (text[0] & 0x80) ? 2 : 1; // unknown: skip it (two bytes for anything non-ASCII we use)
  return nullptr;
}

uint8_t dogLabelWidth(const char *text) {
  uint8_t width = 0, bytes;
  for (; *text; text += bytes) {
    labelGlyph(text, &bytes);
    width += 6;
  }
  return width ? width - 1 : 0;
}

void dogPrintLabel(int16_t x, int8_t y, const char *text) {
  uint8_t bytes;
  for (; *text; text += bytes) {
    const uint8_t *glyph = labelGlyph(text, &bytes);
    if (glyph) {
      OLED::drawArea(x, y, 5, 8, glyph);
    }
    x += 6;
  }
}

uint8_t dogLabelBigWidth(const char *text) { return dogLabelWidth(text) * 2; }

void dogPrintLabelBig(int16_t x, int8_t y, const char *text) {
  uint8_t bytes;
  for (; *text; text += bytes) {
    const uint8_t *glyph = labelGlyph(text, &bytes);
    for (uint8_t col = 0; glyph && col < 5; col++) {
      for (uint8_t row = 0; row < 7; row++) {
        if ((glyph[col] >> row) & 1) {
          OLED::drawFilledRect(x + col * 2, y + row * 2, x + col * 2 + 1, y + row * 2 + 1, false);
        }
      }
    }
    x += 12;
  }
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

void dogDrawNotice(const uint8_t *icon, const char *line1, const char *line2, const char *footer) {
  OLED::clearScreen();
  const uint8_t textH = line2 ? 30 : 14;
  const uint8_t areaH = footer ? 23 : 32;
  const int16_t textX = 28;
  const uint8_t textW = OLED_WIDTH - textX;
  dogBlit(1, (areaH - 24) / 2, 24, 24, icon);
  int8_t y = (areaH - textH) / 2;
  dogPrintLabelBig(textX + (textW - dogLabelBigWidth(line1)) / 2, y, line1);
  if (line2) {
    dogPrintLabelBig(textX + (textW - dogLabelBigWidth(line2)) / 2, y + 16, line2);
  }
  if (footer) {
    OLED::drawFilledRect(2, 23, OLED_WIDTH - 3, 23, false);
    dogPrintLabel((OLED_WIDTH - dogLabelWidth(footer)) / 2, 24, footer);
  }
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

void dogDrawCurve(int16_t x0, int16_t x1, uint8_t y0, uint8_t y1, uint16_t setC, DogCurve range, bool dots) {
  int32_t lo = (int32_t)setC - 45, hi = (int32_t)setC + 15; // zoomed in round the set point so dips while soldering show up
  if (range == DogCurve::Heat) {
    lo = 20;
    hi = (int32_t)setC + 5;
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

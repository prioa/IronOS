#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "Settings.h"
#include "ui_drawing.hpp"

TickType_t dogSolderTimerStart = 0;
uint16_t   dogHeatupStartTempC = 0;
TickType_t dogReachedUntil     = 0;
TickType_t dogLotChangedAt     = 0;
bool       dogWokeHeating      = false;
TickType_t dogSleepEnterTime   = 0;
uint32_t   dogLastSessionSeconds = 0;

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
  dogDrawNotice(iconLock, locked ? DOG_T("GESPERRT", "LOCKED") : DOG_T("OFFEN", "OPEN"), locked ? DOG_T("LÖSEN: AM START", "UNLOCK: ON THE START") : DOG_T("TASTEN FREI", "BUTTONS FREE"));
  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) {
    return true;
  }
  return buttons != BUTTON_NONE;
}

DogAlert dogAlert = DogAlert::None;
bool     dogLockArmed = false;

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
#define DOG_CURVE_SAMPLES      93 // one a column across the stage
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

uint8_t dogDrawCurve(int16_t x0, uint8_t width, uint8_t y0, uint8_t y1, uint16_t setC, uint8_t *rows) {
  // One sample a column, the oldest on the left; when the screen is full it runs to the left
  const uint8_t count = curveCount < width ? curveCount : width;
  auto          at    = [&](uint8_t i) { return curve[(curveHead + DOG_CURVE_SAMPLES - count + i) % DOG_CURVE_SAMPLES]; };
  // The range follows what is shown and the set point, so the set line moves when the set point does: at least 36 degrees
  int32_t lo = setC, hi = setC;
  for (uint8_t i = 0; i < count; i++) {
    lo = at(i) < lo ? at(i) : lo;
    hi = at(i) > hi ? at(i) : hi;
  }
  lo -= 2;
  hi += 2;
  if (hi - lo < 36) {
    const int32_t middle = (lo + hi) / 2;
    lo                   = middle - 18;
    hi                   = middle + 18;
  }
  auto toY = [&](int32_t v) -> uint8_t { return y1 - ((v - lo) * (y1 - y0) + (hi - lo) / 2) / (hi - lo); };
  const uint8_t setY = toY(setC);
  for (uint8_t i = 0; i < width; i += 3) {
    OLED::drawFilledRect(x0 + i, setY, x0 + i, setY, false);
  }
  // Neighbours joined with vertical strokes, so steep changes stay visible
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t y = toY(at(i)), prev = i ? rows[i - 1] : y;
    rows[i]         = y;
    OLED::drawFilledRect(x0 + i, y < prev ? y : prev, x0 + i, y < prev ? prev : y, false);
  }
  return count;
}

// ---- Lifetime soldering time ----
static TickType_t countLastTick = 0;
static uint32_t   countTicks    = 0;
static bool       settingsUnsaved  = false;
uint16_t          dogSessionMinutes = 0;

// One more on a lifetime counter; 65534 is the settings' maximum, see settings.yaml. Saved on the home screen.
static void countUp(enum SettingsOptions option) {
  const uint16_t value = getSettingValue(option);
  if (value < 65534) {
    setSettingValue(option, value + 1);
    settingsUnsaved = true;
  }
}

void dogCountSession() { countUp(SettingsOptions::TotalSolderSessions); }
void dogCountBoost() { countUp(SettingsOptions::TotalBoosts); }

void dogCountSolderTime() {
  const TickType_t now   = xTaskGetTickCount();
  TickType_t       delta = now - countLastTick;
  countLastTick          = now;
  if (delta > TICKS_SECOND) {
    return; // first call after a break
  }
  countTicks += delta;
  if (countTicks >= 60 * TICKS_SECOND) {
    countTicks -= 60 * TICKS_SECOND;
    countUp(SettingsOptions::TotalSolderMinutes);
    if (dogSessionMinutes < 59940) { // 999 hours: the longest the home screen can show
      dogSessionMinutes++;
    }
  }
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

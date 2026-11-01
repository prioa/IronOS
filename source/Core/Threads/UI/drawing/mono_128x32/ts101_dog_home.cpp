/*
 * TS101_DOG_UI: the home screen. The dog on one side and the button pair next to the buttons; between them the statistics
 * and the temperature curve of the last 10 minutes. Held the other way round the blocks swap sides, nothing is mirrored.
 *
 * The dog is never deformed: it sits with its head relaxed and blinks now and then, looks up with its ears raised when the iron
 * is moved and hops when it is shaken. Only how strongly the reading changes counts, so no sensor axis has to be known.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OLED.hpp"
#include "Settings.h"
#include "TipThermoModel.h"
#include "ui_drawing.hpp"
#include <string.h>

// To tune on the iron: how much change counts as moved, in sixteenths of gravity, and how strong a reading counts as a shake
#define DOG_MOVED_SIXTEENTHS 2
#define DOG_SHAKE_X10        16

volatile int16_t dogAccelRaw[3] = {0, 0, 0};

static inline bool normal() { return OLED::getRotation(); }

// ---- History: fed on every frame in every mode, so that the curve has no hole after soldering ----
#define HISTORY_SAMPLES 55
#define HISTORY_TICKS   (TICKS_SECOND * 11)
#define TREND_SAMPLES   10
#define TREND_ON_X100   30 // the arrow appears at 0.30 C/s
#define TREND_OFF_X100  15 // and goes away below 0.15 C/s, so it does not flicker

static uint16_t   history[HISTORY_SAMPLES];
static uint8_t    historyHead = 0, historyCount = 0;
static uint32_t   historySum = 0, historyFrames = 0;
static TickType_t historyLast = 0;

static int16_t    trend[TREND_SAMPLES];
static uint8_t    trendHead = 0, trendCount = 0;
static TickType_t trendLast = 0;
static int8_t     trendDirection = 0;

void dogHomeHistoryReset() {
  historyHead = historyCount = trendHead = trendCount = 0;
  historySum = historyFrames = 0;
  trendDirection             = 0;
}

static void updateTrend() {
  if (trendCount < TREND_SAMPLES) {
    trendDirection = 0;
    return;
  }
  // The mean of the newest three samples minus that of the oldest three, whose middles are 7 s apart
  int32_t sum = 0;
  for (uint8_t i = 0; i < 3; i++) {
    sum += trend[(trendHead + TREND_SAMPLES - 1 - i) % TREND_SAMPLES];
    sum -= trend[(trendHead + i) % TREND_SAMPLES];
  }
  const int32_t x100 = sum * 100 / 21;
  if (x100 >= TREND_ON_X100 || (trendDirection > 0 && x100 >= TREND_OFF_X100)) {
    trendDirection = 1;
  } else if (x100 <= -TREND_ON_X100 || (trendDirection < 0 && x100 <= -TREND_OFF_X100)) {
    trendDirection = -1;
  } else {
    trendDirection = 0;
  }
}

void dogHomeTick() {
  const TickType_t now = xTaskGetTickCount();
  const uint32_t   tip = TipThermoModel::getTipInC();
  historySum += tip;
  historyFrames++;
  if (historyCount == 0 || now - historyLast >= HISTORY_TICKS) { // the first value at once, so the curve is not empty after boot
    history[historyHead] = historySum / historyFrames;
    historyHead          = (historyHead + 1) % HISTORY_SAMPLES;
    if (historyCount < HISTORY_SAMPLES) {
      historyCount++;
    }
    historySum = historyFrames = 0;
    historyLast                = now;
  }
  if (trendCount == 0 || now - trendLast >= TICKS_SECOND) {
    trend[trendHead] = tip;
    trendHead        = (trendHead + 1) % TREND_SAMPLES;
    if (trendCount < TREND_SAMPLES) {
      trendCount++;
    }
    trendLast = now;
    updateTrend();
  }
}

// ---- Motion: 8 updates a second, integers only ----
#define MOTION_TICKS  (TICKS_SECOND / 8)
#define ALERT_HOLD    (TICKS_SECOND * 3 / 2) // it stays alert at least this long, so that it does not flicker
#define HOP_PAUSE     TICKS_SECOND           // between two hops
#define BLINK_TICKS   (TICKS_100MS * 2)
#define DOG_HOME_SIZE 32
#define HEAD_ROWS     14 // rows 0-13 are the head

static const int8_t  HOP_RISE[]    = {1, 2, 1}; // pixels off the ground, frame by frame
static const uint8_t BLINK_GAPS[]  = {3, 5, 2, 6, 4}; // seconds between blinks, so that it does not look like a clock
#define HOP_FRAMES ((uint8_t)sizeof(HOP_RISE))

static int32_t    filtered[3];       // 16 = 1 raw count
static uint32_t   restMagnitude = 0; // 16 = 1 raw count, 0 = not known yet
static TickType_t motionLast    = 0;
static TickType_t alertUntil    = 0;
static TickType_t hopPauseUntil = 0;
static TickType_t nextBlink     = 0;
static TickType_t blinkUntil    = 0;
static bool       motionStarted = false, alert = false, blinking = false, hopPause = false;
static uint8_t    hopFrame      = 0; // 1-HOP_FRAMES while hopping, 0 otherwise
static uint8_t    blinkIndex    = 0;

static uint32_t squareRoot(uint32_t value) {
  uint32_t root = 0;
  for (uint32_t bit = 1UL << 30; bit; bit >>= 2) {
    if (value >= root + bit) {
      value -= root + bit;
      root = (root >> 1) + bit;
    } else {
      root >>= 1;
    }
  }
  return root;
}

static uint32_t magnitude(int32_t x, int32_t y, int32_t z) { return squareRoot((uint32_t)(x * x) + (uint32_t)(y * y) + (uint32_t)(z * z)); }

static inline bool reached(TickType_t now, TickType_t when) { return (int32_t)(now - when) >= 0; }

void dogHomeMotionReset() {
  restMagnitude = 0;
  motionStarted = alert = blinking = hopPause = false;
  hopFrame      = 0;
}

void dogHomeMotionUpdate() {
  const TickType_t now = xTaskGetTickCount();
  if (motionStarted && now - motionLast < MOTION_TICKS) {
    return;
  }
  const int32_t raw[3] = {dogAccelRaw[0], dogAccelRaw[1], dogAccelRaw[2]};
  if (!motionStarted || now - motionLast > TICKS_SECOND) {
    // First update here, or back from another screen: start from the present, no reaction to what happened elsewhere
    for (uint8_t i = 0; i < 3; i++) {
      filtered[i] = raw[i] * 16;
    }
    motionStarted = true;
    motionLast    = now;
    alert         = blinking = hopPause = false;
    hopFrame      = 0;
    nextBlink     = now + BLINK_GAPS[blinkIndex] * TICKS_SECOND;
  } else {
    motionLast += MOTION_TICKS;
  }

  // The sensor's scale differs between chips, so everything below is relative to gravity as this one reports it
  const uint32_t strength = magnitude(raw[0], raw[1], raw[2]);
  if (strength == 0) {
    restMagnitude = 0; // no sensor, or it could not be read: it just sits and blinks
  } else {
    if (!restMagnitude) {
      restMagnitude = strength * 16;
    }
    uint32_t change = 0; // 16 = 1 raw count
    for (uint8_t i = 0; i < 3; i++) {
      const int32_t difference = raw[i] * 16 - filtered[i];
      change += difference < 0 ? -difference : difference;
      filtered[i] += difference * 3 / 10;
    }
    const bool shaken = strength * 160 > restMagnitude * DOG_SHAKE_X10; // 16 * 10 against 10
    if (change * 16 > restMagnitude * DOG_MOVED_SIXTEENTHS || shaken) {
      alert      = true;
      alertUntil = now + ALERT_HOLD;
    } else if (alert && reached(now, alertUntil)) {
      alert = false;
    }
    if (!shaken && !alert) {
      restMagnitude += ((int32_t)(strength * 16) - (int32_t)restMagnitude) / 8;
    }
    if (hopPause && reached(now, hopPauseUntil)) {
      hopPause = false;
    }
    if (hopFrame) {
      if (++hopFrame > HOP_FRAMES) {
        hopFrame      = 0;
        hopPause      = true;
        hopPauseUntil = now + HOP_PAUSE;
      }
    } else if (shaken && !hopPause) {
      hopFrame = 1;
    }
  }

  if (blinking && reached(now, blinkUntil)) {
    blinking = false;
  }
  if (reached(now, nextBlink)) {
    blinking   = !alert && !hopFrame; // looking up it does not blink, the next one comes in its time
    blinkUntil = now + BLINK_TICKS;
    blinkIndex = (blinkIndex + 1) % sizeof(BLINK_GAPS);
    nextBlink  = now + BLINK_GAPS[blinkIndex] * TICKS_SECOND;
  }
}

// ---- The dog: always the bitmap, moved as a whole or with its head as one block ----
static inline bool dogPixel(uint8_t x, uint8_t y) { return (dogHome[(y / 8) * DOG_HOME_SIZE + x] >> (y % 8)) & 1; }

static void plot(int16_t x, int16_t y) {
  if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT) {
    OLED::drawFilledRect(x, y, x, y, false);
  }
}

// rise lifts all of it, headDrop lowers the head on top of that; lids close the two eye gaps on row 7 and row 9
static void drawPose(int16_t originX, uint8_t rise, uint8_t headDrop, bool lid7, bool lid9) {
  for (uint8_t y = 0; y < DOG_HOME_SIZE; y++) {
    const bool lid = (y == 7 && lid7) || (y == 9 && lid9);
    for (uint8_t x = 0; x < DOG_HOME_SIZE; x++) {
      if (dogPixel(x, y) || (lid && (x == 6 || x == 9))) {
        plot(originX + x, y - rise + (y < HEAD_ROWS ? headDrop : 0));
      }
    }
  }
}

static void drawDog(int16_t originX) {
  if (hopFrame) {
    const uint8_t rise = HOP_RISE[hopFrame - 1];
    drawPose(originX, rise, rise, false, false); // the head stays down a bit in the jump, so the ears are not cut off at the top
  } else {
    drawPose(originX, 0, alert ? 0 : 1, blinking, blinking);
  }
}

// Asleep: rows 7 and 9 of the two eye gaps filled, only row 8 stays open, and the head a pixel lower.
// Waking: frame 1 opens row 9, frame 2 row 7, frame 3 lifts the head back up
void dogDrawSleepingDog(int16_t originX, uint8_t wakeFrame) { drawPose(originX, 0, wakeFrame < 3 ? 1 : 0, wakeFrame < 2, wakeFrame < 1); }

// ---- The buttons: two boxes next to the buttons, so they change sides with the orientation ----
static void outline(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  OLED::drawFilledRect(x0, y0, x1, y0, false);
  OLED::drawFilledRect(x0, y1, x1, y1, false);
  OLED::drawFilledRect(x0, y0, x0, y1, false);
  OLED::drawFilledRect(x1, y0, x1, y1, false);
}

// The box of one button, 15x15 on rows 17-31; the inside inverts while it is pressed
void dogKeyBox(uint8_t x, bool front, const uint8_t *icon, uint8_t iconX, uint8_t iconY, uint8_t width, uint8_t height) {
  outline(x, 17, x + 14, 31);
  if (icon) {
    dogBlit(x + iconX, 17 + iconY, width, height, icon);
  } else {
    OLED::drawFilledRect(x + 2, 24, x + 12, 24, false); // a minus, or with the stroke down the middle a plus
    if (front) {
      OLED::drawFilledRect(x + 7, 19, x + 7, 29, false);
    }
  }
  if (dogPressed(front)) {
    OLED::invertRect(x + 1, 18, x + 13, 30);
  }
}

// The left box at x, the right one 17 px further; the front button is the one nearest the tip, "Swap buttons" swaps the roles
void dogKeyPair(uint8_t x, const uint8_t *front, const uint8_t *rear) {
  const bool frontRight = dogFrontOnRight();
  for (uint8_t i = 0; i < 2; i++) {
    const bool isFront = (i == 1) == frontRight;
    // The play triangle sits half a pixel right of the middle on purpose: its optical middle
    dogKeyBox(x + 17 * i, isFront, isFront ? front : rear, isFront ? 5 : 2, isFront ? 2 : 4, isFront ? 6 : 11, isFront ? 16 : 8);
  }
}

static void drawButtonPair() { dogKeyPair(normal() ? 96 : 0, iconHomePlay, iconSliders); }

// ---- Statistics ----
#define STAT_TICKS (TICKS_SECOND * 2)

static void formatCount(char *out, uint16_t value) {
  if (value < 10000) {
    dogFormat(out, value);
  } else {
    strcpy(dogFormat(out, value / 1000), "K"); // 5 digits don't fit behind VORGÄNGE
  }
}

static void formatHours(char *out, uint16_t hours, uint16_t wideFrom) {
  strcpy(dogFormat(out, hours), hours < wideFrom ? " H" : "H");
}

static void drawStatistics(int16_t x) {
  char        value[8];
  const char *label;
  switch ((xTaskGetTickCount() / STAT_TICKS) % 4) {
  case 0:
    label = DOG_T("GESAMT", "TOTAL");
    formatHours(value, getSettingValue(SettingsOptions::TotalSolderMinutes) / 60, 1000);
    break;
  case 1:
    label = DOG_T("AKTUELL", "START");
    if (dogSessionMinutes < 100) {
      strcpy(dogFormat(value, dogSessionMinutes), " M");
    } else if (dogSessionMinutes < 1000) {
      strcpy(dogFormat(value, dogSessionMinutes), "M");
    } else {
      formatHours(value, dogSessionMinutes / 60, 100);
    }
    break;
  case 2:
    label = DOG_T("VORGÄNGE", "RUNS");
    formatCount(value, getSettingValue(SettingsOptions::TotalSolderSessions));
    break;
  default:
    label = "BOOSTS";
    dogFormat(value, getSettingValue(SettingsOptions::TotalBoosts));
    break;
  }
  dogText(dogFontTiny, x, 8, label);
  dogSmall(x + dogTextWidth(dogFontTiny, label) + 2, value, 8);
}

// ---- Temperature and trend ----
#define BIG_PITCH 7 // the label font's digits are 6 wide; its advance of 8 would run into the statistics

static void drawTemperature(int16_t right, uint16_t value, bool blank) {
  char text[6];
  dogFormat(text, value);
  const uint8_t digits = strlen(text);
  const int16_t first  = right - (digits * BIG_PITCH - 1 + 2 + 5) + 1;
  if (trendDirection) {
    dogBlit(first - 7, 3 + (xTaskGetTickCount() / (TICKS_SECOND / 2)) % 2, 5, 8, trendDirection > 0 ? dogTrendUp : dogTrendDown);
  }
  if (blank) {
    return;
  }
  for (uint8_t i = 0; i < digits; i++) {
    const char digit[2] = {text[i], 0};
    dogText(dogFontLabel, first + i * BIG_PITCH, 10, digit);
  }
  dogBlit(right - 4, 0, 5, 8, dogRing);
}

// ---- Curve: 350 C on row 12, 26 C and below on row 21 ----
static uint8_t curveRow(uint16_t tipC) {
  const int32_t c = tipC < 26 ? 26 : (tipC > 350 ? 350 : tipC);
  return 21 - ((c - 26) * 9 + 162) / 324;
}

static void drawHistory(int16_t x0) {
  for (int16_t x = x0; x < x0 + HISTORY_SAMPLES; x += 4) {
    OLED::drawFilledRect(x, 23, x, 23, false);
  }
  uint8_t previous = 0;
  for (uint8_t i = 0; i < historyCount; i++) {
    const uint8_t row = curveRow(history[(historyHead + HISTORY_SAMPLES - historyCount + i) % HISTORY_SAMPLES]);
    const uint8_t x   = x0 + HISTORY_SAMPLES - historyCount + i; // the newest on the right, empty columns on the left
    if (i == 0) {
      previous = row;
    }
    OLED::drawFilledRect(x, row < previous ? row : previous, x, row < previous ? previous : row, false);
    previous = row;
  }
}

void dogDrawHome(uint16_t shownTemperature) {
  static bool hot = false; // the tip is still hot: the number blinks if the setting asks for it
  const uint16_t tipC = TipThermoModel::getTipInC();
  if (tipC > 55) {
    hot = true;
  } else if (tipC < 45) {
    hot = false;
  }
  const bool     blank = hot && getSettingValue(SettingsOptions::CoolingTempBlink) && (xTaskGetTickCount() % 1000 < 300);
  const int16_t  x0    = normal() ? 37 : 36;

  dogHomeMotionUpdate();
  drawDog(normal() ? 2 : OLED_WIDTH - DOG_HOME_SIZE + 2);
  drawStatistics(x0);
  drawTemperature(normal() ? OLED_WIDTH - 1 : 33, shownTemperature, blank);
  drawHistory(x0);
  drawButtonPair();
}
#endif

/*
 * TS101_DOG_UI: the home screen. The dog's head on one side and the button pair next to the buttons; between them the statistics
 * and the temperature curve of the last 10 minutes. Held the other way round the blocks swap sides, nothing is mirrored.
 *
 * Tilt is one axis in the plane of the screen: the angle of the iron's long axis to the horizon. That is 0 both lying flat on
 * the table and held level with the screen towards you, which an angle from two axes of the screen would not be.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OLED.hpp"
#include "Settings.h"
#include "TipThermoModel.h"
#include "ts101_dog_home_assets.h"
#include "ui_drawing.hpp"
#include <string.h>

// The sensor axis along the iron and the sign that makes "tip down" positive, derived from the drivers and not yet checked on
// the hardware. Tip up shows the "tip down" pose: change DOG_ACCEL_ALONG_SIGN. Only the head turns the wrong way: change DOG_ROLL_SIGN.
#define DOG_ACCEL_ALONG      0
#define DOG_ACCEL_ALONG_SIGN (+1)
#define DOG_ROLL_SIGN        (+1)
#define DOG_SHAKE_X10        16 // a shake: the reading this many tenths of gravity at rest, to tune on the iron
// Turning the iron about its long axis: the axis across the board and the one through it. From the drivers' orientation (LIS2DH12
// tells left from right hand by Y), not yet checked on the hardware. The drawn iron turns the wrong way: change DOG_ACCEL_TURN_SIGN
#define DOG_ACCEL_ACROSS     1
#define DOG_ACCEL_NORMAL     2
#define DOG_ACCEL_TURN_SIGN  (+1)

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

// ---- The dog: 8 frames a second, integers only ----
#define MOTION_TICKS  (TICKS_SECOND / 8)
#define SHAKE_HOLD    (TICKS_SECOND / 2) // a shake is seen at 10 Hz at most: it counts as going on in between
#define TIP_DOWN_ON   45
#define TIP_DOWN_OFF  35
#define COCK_FRAMES   5                  // after a shake
#define BLINK_FRAMES  2
#define TONGUE_FRAMES 6
#define TONGUE_FROM_C 60                 // just soldered
#define TONGUE_FIRST  24                 // frames on the home screen until it first shows, then
#define TONGUE_GAP    160                // this many between two: 20 s
#define GAZE_FRONT    2                  // the middle of the five steps

// The five steps of the gaze, left to right as the array runs. Both turned poses look the same way, the other side is them mirrored
static const struct {
  DogPose pose;
  bool    mirrored;
} GAZE[5] = {{DOG_POSE_TURN_RIGHT, true}, {DOG_POSE_TURN_LEFT, true}, {DOG_POSE_FRONT, false}, {DOG_POSE_TURN_LEFT, false}, {DOG_POSE_TURN_RIGHT, false}};

// asin(i / 16) in degrees * 10
static const uint16_t ASIN_X10[17] = {0, 36, 72, 108, 145, 182, 220, 259, 300, 342, 387, 434, 486, 543, 610, 696, 900};
static const uint8_t  HOP_ROWS[4]  = {0, 1, 2, 1};
static const uint8_t  IDLE_GAPS[5] = {52, 60, 48, 64, 56}; // frames until the next blink: 6-8 s, unevenly

static int32_t    filtered[3];       // 16 = 1 raw count
static uint32_t   restMagnitude = 0; // 16 = 1 raw count, 0 = not known yet
static TickType_t motionLast    = 0;
static TickType_t shakeUntil    = 0;
static bool       motionStarted = false, tipDown = false, shaking = false, poseMirrored = false, stepFrame = false;
static uint8_t    gaze = GAZE_FRONT, gazeTarget = GAZE_FRONT; // steps of GAZE: where the head is, where it is going
static int16_t    rollDeg = 0;                                // the tilt as the viewer sees it, positive with the left side down
static int16_t    alongDeg = 0;                               // the long axis against the horizon, tip down positive
static uint8_t    cockFrames = 0, idleFrames = 0, idleGap = 0, tongueGap = 0, hopFrame = 0, hopRow = 0, idleCount = 0;
static DogPose    idlePose   = DOG_POSE_BLINK;
static DogPose    pose       = DOG_POSE_FRONT;

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

static int16_t asinDeg(int32_t along, uint32_t length) {
  const bool negative = along < 0;
  uint32_t   ratio    = (uint32_t)(negative ? -along : along) * 256 / length; // 256 = 1
  ratio               = ratio > 255 ? 255 : ratio;
  const uint8_t  i    = ratio / 16;
  const uint16_t x10  = ASIN_X10[i] + (ASIN_X10[i + 1] - ASIN_X10[i]) * (ratio % 16) / 16;
  const int16_t  deg  = (x10 + 5) / 10;
  return negative ? -deg : deg;
}

void dogHomeMotionReset() {
  restMagnitude = 0;
  motionStarted = false;
}

// What the sensor says: shaking, tipDown and gazeTarget
static void readLie(const int32_t raw[3], TickType_t now) {
  // The sensor's scale differs between chips, so everything is relative to gravity as this one reports it
  const uint32_t strength = magnitude(raw[0], raw[1], raw[2]);
  if (strength == 0) {
    shaking = tipDown = false; // no sensor, or it could not be read
    gazeTarget        = GAZE_FRONT;
    rollDeg           = 0;
    return;
  }
  if (!restMagnitude) {
    restMagnitude = strength * 16;
  }
  if (strength * 160 > restMagnitude * DOG_SHAKE_X10) { // 16 * 10 against 10
    shakeUntil = now + SHAKE_HOLD;
  } else if ((int32_t)(now - shakeUntil) >= 0) {
    restMagnitude += ((int32_t)(strength * 16) - (int32_t)restMagnitude) / 8;
  }
  shaking = (int32_t)(now - shakeUntil) < 0;
  for (uint8_t i = 0; i < 3; i++) {
    filtered[i] += (raw[i] * 16 - filtered[i]) * 3 / 10;
  }
  const uint32_t length = magnitude(filtered[0] / 16, filtered[1] / 16, filtered[2] / 16);
  const int16_t  angle  = length ? asinDeg(DOG_ACCEL_ALONG_SIGN * filtered[DOG_ACCEL_ALONG] / 16, length) : 0; // positive: tip end down
  tipDown               = angle >= TIP_DOWN_ON || (tipDown && angle >= TIP_DOWN_OFF);
  alongDeg              = angle;
  // As the viewer sees it, positive with the left side down. The tip is on the right, or on the left held the other way round.
  // round(clamp(-roll / 22, -1, 1) * 2): full at 22 degrees, a step changes at 5.5 and 16.5
  const int16_t roll  = DOG_ROLL_SIGN * (normal() ? -angle : angle);
  rollDeg             = roll;
  const int16_t size  = roll < 0 ? -roll : roll;
  const uint8_t steps = size >= 17 ? 2 : (size * 2 + 11) / 22;
  gazeTarget          = roll > 0 ? GAZE_FRONT - steps : GAZE_FRONT + steps; // against the tilt, towards you
}

// One frame of the pose: a shake before the tip before the gaze; alone in the middle, it blinks and pants
static void nextPose() {
  poseMirrored = false;
  if (shaking) {
    pose       = DOG_POSE_EARS_BACK;
    hopRow     = HOP_ROWS[hopFrame];
    hopFrame   = (hopFrame + 1) % 4;
    cockFrames = COCK_FRAMES;
    idleFrames = 0;
    return; // the filter is no use during a shake: the head waits
  }
  hopFrame = hopRow = 0;
  stepFrame         = !stepFrame;
  if (stepFrame) { // every other frame at most one step: you see it turn
    if (gaze < gazeTarget) {
      gaze++;
    } else if (gaze > gazeTarget) {
      gaze--;
    }
  }
  if (cockFrames) {
    cockFrames--;
    pose = DOG_POSE_COCK;
    return;
  }
  if (tipDown) {
    pose = DOG_POSE_UP;
    return;
  }
  pose         = GAZE[gaze].pose;
  poseMirrored = GAZE[gaze].mirrored;
  if (gaze != GAZE_FRONT) {
    idleFrames = 0;
    return;
  }
  if (idleFrames) {
    idleFrames--;
    pose = idlePose;
  } else if (!idleGap) {
    idleCount++;
    idleGap    = IDLE_GAPS[idleCount % sizeof(IDLE_GAPS)];
    idlePose   = DOG_POSE_BLINK;
    idleFrames = BLINK_FRAMES - 1;
    pose       = idlePose;
  } else if (!tongueGap && TipThermoModel::getTipInC() > TONGUE_FROM_C) { // hot from soldering
    tongueGap  = TONGUE_GAP;
    idlePose   = DOG_POSE_TONGUE;
    idleFrames = TONGUE_FRAMES - 1;
    pose       = idlePose;
  } else {
    idleGap--;
    if (tongueGap) {
      tongueGap--;
    }
  }
}

void dogHomeMotionUpdate() {
  const TickType_t now = xTaskGetTickCount();
  if (motionStarted && now - motionLast < MOTION_TICKS) {
    return;
  }
  const int32_t raw[3] = {dogAccelRaw[0], dogAccelRaw[1], dogAccelRaw[2]};
  if (!motionStarted || now - motionLast > TICKS_SECOND) {
    // First update here, or back from another screen: start from the present instead of swinging over from the past
    for (uint8_t i = 0; i < 3; i++) {
      filtered[i] = raw[i] * 16;
    }
    motionStarted = true;
    motionLast    = now;
    shakeUntil    = now;
    tipDown       = false;
    cockFrames = idleFrames = hopFrame = hopRow = 0;
    idleGap    = IDLE_GAPS[0];
    tongueGap  = TONGUE_FIRST;
    readLie(raw, now);
    gaze = gazeTarget; // the head is already where it should be when the screen comes up
  } else {
    motionLast += MOTION_TICKS;
    readLie(raw, now);
  }
  nextPose();
}

// ---- The dog on the sleep screen: the full figure, its lids and its head moved on its own ----
#define DOG_HOME_SIZE 32
#define HEAD_ROWS     14 // rows 0-13 are the head

static inline bool dogPixel(uint8_t x, uint8_t y) { return (dogHome[(y / 8) * DOG_HOME_SIZE + x] >> (y % 8)) & 1; }

static void plot(int16_t x, int16_t y) {
  if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT) {
    OLED::drawFilledRect(x, y, x, y, false);
  }
}

// Asleep: rows 7 and 9 of the two eye gaps filled, only row 8 stays open, and the head a pixel lower.
// Waking: frame 1 opens row 9, frame 2 row 7, frame 3 lifts the head back up
void dogDrawSleepingDog(int16_t originX, uint8_t wakeFrame) {
  const uint8_t headDrop = wakeFrame < 3 ? 1 : 0;
  for (uint8_t y = 0; y < DOG_HOME_SIZE; y++) {
    const bool lid = (y == 7 && wakeFrame < 2) || (y == 9 && wakeFrame < 1);
    for (uint8_t x = 0; x < DOG_HOME_SIZE; x++) {
      if (dogPixel(x, y) || (lid && (x == 6 || x == 9))) {
        plot(originX + x, y + (y < HEAD_ROWS ? headDrop : 0));
      }
    }
  }
}

// A pose, the columns the other way round if mirrored; below the screen it is cut off
static void drawPose(int16_t x, int8_t y) {
  const uint8_t *data = dogPoses[pose];
  for (uint8_t page = 0; page < DOG_POSE_SIZE / 8; page++) {
    for (uint8_t col = 0; col < DOG_POSE_SIZE; col++) {
      const uint8_t bits = data[page * DOG_POSE_SIZE + (poseMirrored ? DOG_POSE_SIZE - 1 - col : col)];
      for (uint8_t bit = 0; bit < 8; bit++) {
        if ((bits >> bit) & 1) {
          plot(x + col, y + page * 8 + bit);
        }
      }
    }
  }
}

// ---- The buttons: a frame x..x+31 on rows 17-31 with a box per button, next to the buttons, so it changes sides with the orientation ----
static void outline(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  OLED::drawFilledRect(x0, y0, x1, y0, false);
  OLED::drawFilledRect(x0, y1, x1, y1, false);
  OLED::drawFilledRect(x0, y0, x0, y1, false);
  OLED::drawFilledRect(x1, y0, x1, y1, false);
}

// The box of one button, 15x15 on rows 17-31, for the soldering screens; the inside inverts while it is pressed
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
    dogKeyBox(x + 17 * i, isFront, isFront ? front : rear, 2, 4, 11, 8);
  }
}

// ---- Statistics: like a lenticular print, the tilt picks the figure ----
// Strongly to the left BOOSTS, slightly left VORGÄNGE, level GESAMT, to the right SEIT START (the iron has no clock for HEUTE).
// Close to a threshold both figures show, row by row, the rows swapping every frame
#define KPI_NEAR 3 // degrees either side of a threshold
static const int8_t KPI_THRESHOLD[3] = {-18, -6, 6};
enum Kpi : uint8_t { KPI_BOOSTS, KPI_RUNS, KPI_TOTAL, KPI_SESSION };

// The value in its long form, or the short one if the row would run past x=93 (92 turned, where the arrow starts)
static void drawKpi(int16_t x, Kpi kpi) {
  const uint16_t minutes = dogSessionMinutes;
  const char    *label;
  char           value[10], shortValue[8];
  switch (kpi) {
  case KPI_TOTAL: {
    const uint16_t hours = getSettingValue(SettingsOptions::TotalSolderMinutes) / 60;
    label                = DOG_T("GESAMT", "TOTAL");
    strcpy(dogFormat(value, hours), " H");
    strcpy(dogFormat(shortValue, hours), "H");
    break;
  }
  case KPI_SESSION:
    label = DOG_T("SEIT START", "SESSION"); // since power on: the iron has no clock
    if (minutes < 100) {
      strcpy(dogFormat(value, minutes), " MIN");
      strcpy(dogFormat(shortValue, minutes), "M");
    } else {
      const uint16_t hours = minutes / 60;
      strcpy(dogFormat(value, hours), " H");
      strcpy(dogFormat(shortValue, hours), hours < 100 ? "H" : ""); // behind SEIT START there is room for three characters
    }
    break;
  case KPI_RUNS: {
    const uint16_t runs = getSettingValue(SettingsOptions::TotalSolderSessions);
    label               = DOG_T("VORGÄNGE", "RUNS");
    dogFormat(value, runs);
    strcpy(dogFormat(shortValue, runs / 1000), "K"); // 5 digits don't fit behind VORGÄNGE
    break;
  }
  default: {
    const uint16_t boosts = getSettingValue(SettingsOptions::TotalBoosts);
    label                 = "BOOSTS";
    dogFormat(value, boosts);
    strcpy(dogFormat(shortValue, boosts / 1000), "K");
    break;
  }
  }
  dogText(dogFontTiny, x, 8, label);
  const int16_t valueX = x + dogTextWidth(dogFontTiny, label) + 2;
  const int16_t limit  = x + (normal() && trendDirection ? 55 : 56); // a pixel of air before the arrow, when there is one
  dogSmall(valueX, valueX + dogTextWidth(dogFontSmall, value) - 1 > limit ? shortValue : value, 8);
}

static void drawStatistics(int16_t x) {
  static uint8_t parity = 0;
  parity ^= 1;
  const int16_t tilt = -rollDeg; // negative: tilted to the left
  uint8_t       kpi  = 0;
  while (kpi < 3 && tilt >= KPI_THRESHOLD[kpi]) {
    kpi++;
  }
  uint8_t other = kpi;
  for (uint8_t i = 0; i < 3; i++) {
    const int16_t off = tilt - KPI_THRESHOLD[i];
    if (off > -KPI_NEAR && off < KPI_NEAR) {
      other = kpi == i ? i + 1 : i;
    }
  }
  if (other == kpi) {
    drawKpi(x, (Kpi)kpi);
    return;
  }
  const uint32_t rows = parity ? 0x55555555 : 0xAAAAAAAA;
  dogBlitRows         = ~rows;
  drawKpi(x, (Kpi)kpi);
  dogBlitRows = rows;
  drawKpi(x, (Kpi)other);
  dogBlitRows = 0xFFFFFFFF;
}

// ---- The buttons: two small keys in the bottom corner, the one meant filled, with what it does ----
// x0 is where the block starts: words left of the keys normally, right of them turned
static void miniKey(int16_t x, int16_t y, bool fillRight, bool pressed) {
  outline(x, y, x + 6, y + 4);
  OLED::drawFilledRect(fillRight ? x + 3 : x, y, fillRight ? x + 6 : x + 3, y + 4, false);
  if (pressed) {
    OLED::invertRect(x + 1, y + 1, x + 5, y + 3);
  }
}

static void drawKeyHints() {
  const bool    frontRight = dogFrontOnRight();
  const int16_t keyX       = normal() ? OLED_WIDTH - 7 : 0;
  const char   *menu = DOG_T("MENU", "MENU"), *start = "START";
  // The rear key sits on the side away from the tip: filled there for the menu, on the tip's side for start
  miniKey(keyX, 19, !frontRight, dogPressed(false));
  miniKey(keyX, 26, frontRight, dogPressed(true));
  if (normal()) {
    dogText(dogFontTiny, keyX - 4 - dogTextWidth(dogFontTiny, menu) + 1, 24, menu);
    dogText(dogFontTiny, keyX - 4 - dogTextWidth(dogFontTiny, start) + 1, 31, start);
  } else {
    dogText(dogFontTiny, keyX + 10, 24, menu);
    dogText(dogFontTiny, keyX + 10, 31, start);
  }
}

// ---- Temperature and trend ----
#define BIG_PITCH 7 // the label font's digits are 6 wide; its advance of 8 would run into the statistics

static void drawTemperature(int16_t right, uint16_t value, bool blank) {
  char text[6];
  dogFormat(text, value);
  const uint8_t digits = strlen(text);
  const int16_t first  = right - (digits * BIG_PITCH - 1 + 2 + 5) + 1;
  if (trendDirection) {
    dogBlit(first - 7, 2 + (xTaskGetTickCount() / (TICKS_SECOND / 2)) % 2, 5, 8, trendDirection > 0 ? iconHomeArrowUp : iconHomeArrowDown);
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

// ---- For the other home views (ts101_dog_home_views.cpp) ----
int16_t  dogHomeRollDeg() { return rollDeg; }
// The lie of the iron from the newest reading, without the dog's smoothing: the iron drawn on the screen follows at once.
// along: the long axis against the horizon, tip down positive; turn: about the long axis, 0 board up, +-90 on its side
bool dogIronSteady = true;
bool dogIronLie(int16_t &along, int16_t &turn) {
  const int32_t  raw[3] = {dogAccelRaw[0], dogAccelRaw[1], dogAccelRaw[2]};
  const uint32_t length = magnitude(raw[0], raw[1], raw[2]);
  if (!length) {
    return false; // no sensor
  }
  // Pulled by more or less than gravity: the hand moves it, the reading is not its lie
  static uint32_t rest = 0;
  rest                 = rest ? rest + ((int32_t)length - (int32_t)rest) / 16 : length;
  const uint32_t off   = length > rest ? length - rest : rest - length;
  dogIronSteady        = off * 8 < rest;
  along                 = asinDeg(DOG_ACCEL_ALONG_SIGN * raw[DOG_ACCEL_ALONG], length);
  const int32_t  across = raw[DOG_ACCEL_ACROSS], through = raw[DOG_ACCEL_NORMAL];
  const uint32_t side   = magnitude(across, through, 0);
  if (side) {
    const int16_t a = asinDeg(across, side);
    turn            = DOG_ACCEL_TURN_SIGN * (through >= 0 ? a : (a >= 0 ? 180 - a : -180 - a));
  }
  return true;
}
bool     dogHomeTipDown() { return tipDown; }
bool     dogHomeShaking() { return shaking; }
int8_t   dogHomeTrend() { return trendDirection; }
uint8_t  dogHomeHistoryCount() { return historyCount; }
uint16_t dogHomeHistoryAt(uint8_t i) { return history[(historyHead + HISTORY_SAMPLES - historyCount + i) % HISTORY_SAMPLES]; }
void     dogDrawHomeHead(int16_t x) {
  dogHomeMotionUpdate();
  drawPose(x, hopRow);
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
  drawStatistics(x0);
  drawTemperature(normal() ? OLED_WIDTH - 1 : 33, shownTemperature, blank);
  drawHistory(x0);
  drawKeyHints();
  drawPose(normal() ? 0 : OLED_WIDTH - DOG_POSE_SIZE, hopRow);
}
#endif

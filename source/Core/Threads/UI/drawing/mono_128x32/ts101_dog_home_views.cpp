/*
 * TS101_DOG_UI: the home views besides the dog (setting DogHomeVariant, a rear click on the home screen picks the next one).
 *   1 Symbole:     the dog's head, then a thermometer, a clock and an hourglass with their values and names below
 *   2 Schäferhund: tip and grip temperature small on the left, the rest is a German Shepherd trotting in, wagging, sitting, leaving
 *   3 Abkühlen:    the dog's head, the large temperature with its arrow, when the tip will be hand warm, the last 10 minutes
 *   4 Kolben:      the iron as it lies in the hand, the tip and grip temperatures where they belong
 * Turned the other way round, the head moves to the other side; the shepherd's screen stays as it is, the iron is mirrored.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OLED.hpp"
#include "Settings.h"
#include "TipThermoModel.h"
#include "ts101_dog_home_assets.h"
#include "ui_drawing.hpp"
#include <string.h>

static inline bool normal() { return OLED::getRotation(); }

static void plot(int16_t x, int16_t y, bool on = true) {
  if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT) {
    OLED::drawFilledRect(x, y, x, y, !on);
  }
}

// The grip in the unit of the setting
static uint16_t gripTemperature() {
  const uint32_t c = getHandleTemperature(0) / 10;
  return getSettingValue(SettingsOptions::TemperatureInF) ? c * 9 / 5 + 32 : c;
}

// sin of whole degrees, 1024 = 1
static int32_t isin(int32_t deg) {
  static const uint16_t QUARTER[19] = {0, 89, 178, 265, 350, 433, 512, 587, 658, 724, 784, 839, 887, 928, 962, 989, 1008, 1020, 1024}; // every 5 degrees
  deg = ((deg % 360) + 360) % 360;
  const bool negative = deg >= 180;
  deg             = negative ? deg - 180 : deg;
  deg             = deg > 90 ? 180 - deg : deg;
  const int32_t v = QUARTER[deg / 5] + (QUARTER[(deg + 4) / 5] - QUARTER[deg / 5]) * (deg % 5) / 5;
  return negative ? -v : v;
}
static int32_t icos(int32_t deg) { return isin(deg + 90); }

// ---- 1 Symbole ----
#define V1_ROW_VALUE 23 // base of the values in 5x7
#define V1_ROW_LABEL 31 // base of the names in 3x5

// A value in 5x7 with its unit in 3x5 a pixel behind; the degree ring sits up with the top of the digits
static void valueCentred(int16_t centre, const char *value, const char *unit, bool ring) {
  const int16_t width = dogTextWidth(dogFontSmall, value) + 1 + dogTextWidth(dogFontTiny, unit);
  const int16_t x     = centre - width / 2;
  dogSmall(x, value, V1_ROW_VALUE);
  dogText(dogFontTiny, x + dogTextWidth(dogFontSmall, value) + 1, ring ? V1_ROW_VALUE - 2 : V1_ROW_VALUE, unit);
}

static void labelCentred(int16_t centre, const char *text) { dogText(dogFontTiny, centre - dogTextWidth(dogFontTiny, text) / 2, V1_ROW_LABEL, text); }

// The clock's face fills clockwise from twelve; 1024 = full
static void clockFill(int16_t cx, int16_t cy, uint32_t part) {
  for (int16_t dy = -4; dy <= 4; dy++) {
    for (int16_t dx = -4; dx <= 4; dx++) {
      if (dx * dx + dy * dy > 18) {
        continue;
      }
      // The angle from twelve, clockwise, in 1/1024 of a turn: per quadrant from the ratio, close enough for 9x9 pixels
      const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
      int32_t       a  = (ax + ay) ? ax * 256 / (ax + ay) : 0; // 0 straight up or down, 256 sideways
      if (dx >= 0 && dy < 0) {
        a = a;
      } else if (dx >= 0) {
        a = 512 - a;
      } else if (dy >= 0) {
        a = 512 + a;
      } else {
        a = 1024 - a;
      }
      if ((uint32_t)a < part || (dx == 0 && dy == 0 && part)) {
        plot(cx + dx, cy + dy);
      }
    }
  }
}

static void drawSymbols(uint16_t shown) {
  const int16_t x0 = normal() ? 32 : 0;
  dogDrawHomeHead(normal() ? 0 : OLED_WIDTH - DOG_POSE_SIZE);
  const int16_t c1 = x0 + 17, c2 = x0 + 48, c3 = x0 + 80;
  char          text[8];

  // Thermometer: the column fills with the tip, 0-400 C
  dogBlit(c1 - 3, 0, 7, 16, iconV1Thermo);
  const uint32_t tipC  = TipThermoModel::getTipInC();
  const uint8_t  level = (tipC > 400 ? 400 : tipC) * 8 / 400;
  if (level) {
    OLED::drawFilledRect(c1 - 1, 9 - level, c1 + 1, 8, false);
  }
  dogFormat(text, shown);
  valueCentred(c1, text, "°", true);
  labelCentred(c1, "TEMP");

  // Clock: soldering since power on, full at an hour
  const uint16_t minutes = dogSessionMinutes;
  dogBlit(c2 - 6, 0, 13, 16, iconV1Clock);
  clockFill(c2, 6, minutes >= 60 ? 1024 : minutes * 1024 / 60);
  if (minutes < 100) {
    dogFormat(text, minutes);
    valueCentred(c2, text, "MIN", false);
  } else {
    dogFormat(text, minutes / 60);
    valueCentred(c2, text, DOG_T("STD", "H"), false);
  }
  labelCentred(c2, DOG_T("SITZUNG", "SESSION"));

  // Hourglass: lifetime hours, no fill (the iron knows no lifetime of a tip)
  dogBlit(c3 - 5, 1, 11, 16, iconV1Glass);
  dogFormat(text, getSettingValue(SettingsOptions::TotalSolderMinutes) / 60);
  valueCentred(c3, text, DOG_T("STD", "H"), false);
  labelCentred(c3, DOG_T("GESAMT", "TOTAL"));
}

// ---- 2 Schäferhund ----
#define V3_STAGE_X  32 // the dog is cut off left of here
#define V3_FRAME    (TICKS_SECOND / 8)
#define V3_FRAMES   96 // 12 s
#define V3_TOP      7

// The sprite and its column for one of the 96 frames, as on the design page: 255 = no dog
static uint8_t shepherdFrame(uint8_t i, int16_t &x) {
  if (i >= 1 && i <= 25) { // trots in, 2 px a frame
    x = 2 * i - 3;
    return (i + 2) % 4;
  }
  if ((i >= 26 && i <= 39) || (i >= 66 && i <= 71)) { // stands and wags
    x = 49 + (i & 1);
    return 4 + (i & 1);
  }
  if (i >= 40 && i <= 65) { // sits
    x = 48;
    return 6;
  }
  if (i >= 72 && i <= 86) { // trots out, 5 px a frame
    x = 54 + 5 * (i - 72);
    return (i + 2) % 4;
  }
  return 255;
}

static void shepherdTemperature(uint8_t base, const char *name, uint16_t value) {
  char text[6];
  dogFormat(text, value);
  dogText(dogFontTiny, 0, base, name);
  // As on the design: up to two digits end at x=22, the ring at 25. Three would touch SP: they move a pixel to the right
  const int16_t shift = value >= 100 ? 1 : 0;
  dogSmallRight(23 + shift, text, base); // the small font's digits have an empty column on the right
  dogText(dogFontTiny, 25 + shift, base - 2, "°");
}

static void drawShepherd(uint16_t shown) {
  static const uint8_t *const SPRITES[7]      = {dogV3Trot0, dogV3Trot1, dogV3Trot2, dogV3Trot3, dogV3Wag0, dogV3Wag1, dogV3Sit};
  static const uint8_t        SPRITE_WIDTH[7] = {34, 34, 34, 34, 34, 33, 33}; // as in art/home.txt
  dogHomeMotionUpdate(); // keeps the head's state current for the other views
  shepherdTemperature(11, DOG_T("SP", "TP"), shown);
  shepherdTemperature(24, DOG_T("GR", "HD"), gripTemperature());
  for (uint8_t y = 0; y < 30; y += 2) {
    plot(30, y);
  }
  for (uint8_t x = 0; x < OLED_WIDTH; x += 2) {
    plot(x, 30);
  }
  int16_t       x;
  const uint8_t sprite = shepherdFrame((xTaskGetTickCount() / V3_FRAME) % V3_FRAMES, x);
  if (sprite == 255) {
    return;
  }
  const uint8_t *data = SPRITES[sprite];
  const uint8_t  w    = SPRITE_WIDTH[sprite];
  for (uint8_t col = 0; col < w; col++) {
    if (x + col < V3_STAGE_X) {
      continue;
    }
    for (uint8_t row = 0; row < 24; row++) {
      if ((data[(row / 8) * w + col] >> (row % 8)) & 1) {
        plot(x + col, V3_TOP + row);
      }
    }
  }
}

// ---- 3 Abkühlen ----
#define HAND_WARM_C 50

// The factor per 11 s that, five times over, gives ratio (65536 = 1)
static uint32_t fifthRoot(uint32_t ratio) {
  uint32_t lo = ratio, hi = 65536;
  while (hi - lo > 1) {
    const uint32_t mid = (lo + hi) / 2;
    uint64_t       p   = 65536;
    for (uint8_t i = 0; i < 5; i++) {
      p = p * mid / 65536;
    }
    (p > ratio ? hi : lo) = mid;
  }
  return lo;
}

// Minutes until the tip is down to 50 C, cooling as it did over the last 55 s towards the grip's temperature: 0 = no estimate
static uint16_t minutesToHandWarm() {
  const uint8_t n = dogHomeHistoryCount();
  if (n < 6) {
    return 0;
  }
  const int32_t room = getHandleTemperature(0) / 10 < HAND_WARM_C - 5 ? getHandleTemperature(0) / 10 : HAND_WARM_C - 5;
  const int32_t now = (int32_t)dogHomeHistoryAt(n - 1) - room, before = (int32_t)dogHomeHistoryAt(n - 6) - room;
  if (now <= HAND_WARM_C - room || before <= now) {
    return 0;
  }
  // Each 11 s the distance to the room shrinks by the fifth root of now/before; count the steps until it is under 50 C
  const uint32_t factor = fifthRoot((uint32_t)now * 65536 / before);
  uint32_t       left   = (uint32_t)now * 256;
  uint16_t       steps  = 0;
  while (left > (uint32_t)(HAND_WARM_C - room) * 256 && steps < 1000) {
    left = (uint64_t)left * factor / 65536;
    steps++;
  }
  return steps >= 1000 ? 0 : ((steps * 11 + 30) / 60 ? (steps * 11 + 30) / 60 : 1); // rounded, at least a minute
}

static void drawCooling(uint16_t shown) {
  const int16_t x0 = normal() ? 32 : 0, right = x0 + 95;
  dogDrawHomeHead(normal() ? 0 : OLED_WIDTH - DOG_POSE_SIZE);
  char text[12];
  dogFormat(text, shown);
  dogText(dogFontSeg, x0 + 4, dogFontSeg.baseline, text);
  int16_t x = x0 + 4 + dogTextWidth(dogFontSeg, text) + 2;
  dogBlit(x, 0, 4, 8, dogSegRing);
  if (dogHomeTrend()) {
    dogBlit(x + 7, 12 + (xTaskGetTickCount() / (TICKS_SECOND / 2)) % 2, 5, 8, dogHomeTrend() > 0 ? iconHomeArrowUp : iconHomeArrowDown);
  }

  const char *caption = DOG_T("HANDWARM IN", "HAND WARM IN");
  dogText(dogFontTiny, right - dogTextWidth(dogFontTiny, caption) + 1, 6, caption);
  if (TipThermoModel::getTipInC() <= HAND_WARM_C) {
    strcpy(text, DOG_T("JETZT", "NOW"));
  } else if (const uint16_t minutes = minutesToHandWarm()) {
    strcpy(dogFormat(text, minutes), " MIN");
  } else {
    strcpy(text, "-");
  }
  dogSmallRight(right - 1, text, 15);

  // The last 10 minutes across the full width, 50 C on the dotted line at the bottom
  const int16_t left = x0 + 4;
  for (int16_t dx = 0; left + dx <= right; dx += 3) {
    plot(left + dx, 30);
  }
  const uint8_t n = dogHomeHistoryCount();
  if (n < 2) {
    return;
  }
  uint16_t top = 60;
  for (uint8_t i = 0; i < n; i++) {
    top = dogHomeHistoryAt(i) > top ? dogHomeHistoryAt(i) : top;
  }
  auto rowOf = [&](uint16_t c) -> int16_t { return c <= HAND_WARM_C ? 29 : 29 - (int32_t)(c - HAND_WARM_C) * 7 / (top - HAND_WARM_C); }; // rows 22-29, above the line
  const int16_t width = right - left + 1;
  int16_t       previous = rowOf(dogHomeHistoryAt(0));
  for (int16_t dx = 0; dx < width; dx++) {
    const int16_t row = rowOf(dogHomeHistoryAt(dx * (n - 1) / (width - 1)));
    OLED::drawFilledRect(left + dx, row < previous ? row : previous, left + dx, row < previous ? previous : row, false);
    previous = row;
  }
}

// ---- 4 Kolben ----
// The iron along its axis in 1/256 of the model's units: end cap, joint, grip with the display, joint, neck, joint, tip
struct Ring {
  int16_t u, r;
  bool    joinsPrevious; // false: a dark joint lies between this and the ring before
};
static const Ring PROFILE[] = {{-1267, 102, false}, {-1152, 113, true}, {-1062, 128, false}, {-998, 143, true}, {486, 143, true}, {563, 128, true}, {640, 108, false},
                               {819, 87, true},     {896, 61, false},   {998, 41, true},     {1331, 18, true}, {1459, 0, true}};
#define IRON_PX_X100   853 // pixels a unit x 100
#define IRON_THICK_X100 115
#define IRON_SHIFT     192 // the axis is moved this far towards the cap, so the iron sits in the middle
#define IRON_CY        15
#define IRON_ELEVATION 30 // the iron is seen from above at this angle
#define IRON_PITCH_DOWN 22

// The radius at u, 0 in a joint or beyond the ends
static int32_t radiusAt(int32_t u) {
  for (uint8_t i = 1; i < sizeof(PROFILE) / sizeof(PROFILE[0]); i++) {
    if (u <= PROFILE[i].u) {
      if (u < PROFILE[i - 1].u || !PROFILE[i].joinsPrevious) {
        return 0;
      }
      return PROFILE[i - 1].r + (int32_t)(PROFILE[i].r - PROFILE[i - 1].r) * (u - PROFILE[i - 1].u) / (PROFILE[i].u - PROFILE[i - 1].u);
    }
  }
  return 0;
}

// A strip on the surface between the angles centre-half and centre+half (0 = straight up on the screen, 90 = facing the viewer)
static void surfaceStrip(int16_t x, int32_t cy, int32_t radius, int32_t centre, int32_t half, bool on) {
  int32_t a0 = centre - half, a1 = centre + half;
  a0 = a0 < 1 ? 1 : a0;
  a1 = a1 > 179 ? 179 : a1;
  if (a0 >= a1) {
    return; // on the far side
  }
  const int32_t y0 = cy - radius * icos(a0) / 1024, y1 = cy - radius * icos(a1) / 1024;
  OLED::drawFilledRect(x, y0, x, y1 > y0 ? y1 : y0, !on);
}

static void drawIronAt(int32_t rollDeg, int32_t pitchDeg, bool mirror, int16_t &tipX, int16_t &tipY, int16_t &gripX, int16_t &gripTop, int16_t &gripBottom) {
  const int32_t cp = icos(pitchDeg), sp = isin(pitchDeg), ce = icos(IRON_ELEVATION);
  const int32_t facing = 90 + rollDeg; // the display's angle on the screen: at rest it faces the viewer
  auto screenX = [&](int32_t along) -> int16_t { // along the axis, in 1/256 units from the middle
    const int32_t px = OLED_WIDTH / 2 - along * cp / 1024 * IRON_PX_X100 / 100 / 256;
    return mirror ? OLED_WIDTH - 1 - px : px;
  };
  gripTop = OLED_HEIGHT;
  gripBottom = 0;
  for (int16_t col = 0; col < OLED_WIDTH; col++) {
    const int32_t dx = mirror ? col - (OLED_WIDTH / 2 - 1) : OLED_WIDTH / 2 - col;
    const int32_t along = dx * 100 * 256 / IRON_PX_X100 * 1024 / (cp ? cp : 1);
    const int32_t u = along - IRON_SHIFT;
    const int32_t r = radiusAt(u) * IRON_THICK_X100 / 100 * IRON_PX_X100 / 100 / 256; // pixels
    if (u < PROFILE[0].u || u > PROFILE[sizeof(PROFILE) / sizeof(PROFILE[0]) - 1].u) {
      continue;
    }
    const int32_t cy = IRON_CY - along * sp / 1024 * ce / 1024 * IRON_PX_X100 / 100 / 256;
    if (radiusAt(u) == 0) {
      continue; // a joint: dark
    }
    const int32_t h = r < 1 ? 0 : r;
    OLED::drawFilledRect(col, cy - h, col, cy + h, false);
    if (u >= -691 && u <= 38) { // the display window, the display inside it
      surfaceStrip(col, cy, h, facing, 25, false);
      if (u >= -627 && u <= -26) {
        surfaceStrip(col, cy, h, facing, 8, true);
      }
    } else if (u >= 141 && u <= 243) { // the button
      surfaceStrip(col, cy, h, facing, 18, false);
    }
    if (u >= -400 && u <= -200) { // where the grip's temperature goes
      gripTop    = cy - h < gripTop ? cy - h : gripTop;
      gripBottom = cy + h > gripBottom ? cy + h : gripBottom;
      gripX      = col;
    }
  }
  tipX = screenX(1459 + IRON_SHIFT);
  tipY = IRON_CY - (int32_t)(1459 + IRON_SHIFT) * sp / 1024 * ce / 1024 * IRON_PX_X100 / 100 / 256;
  // The cable from the end cap, hanging down; it turns with the roll
  const int32_t cr = icos(rollDeg - 30), sr = isin(rollDeg - 30), se = isin(IRON_ELEVATION); // the design rests the iron rolled by -30
  int16_t       lastX = 0, lastY = 0;
  for (int32_t s = 0; s <= 16; s++) {
    const int32_t x  = -1267 - 538 * s / 16 + IRON_SHIFT; // 2.1 units long
    const int32_t y  = -243 * s * s / 256 * IRON_THICK_X100 / 100; // .95 units down at the end
    const int32_t y1 = y * cr / 1024, z1 = y * sr / 1024;
    const int32_t x2 = (x * cp - y1 * sp) / 1024, y2 = (x * sp + y1 * cp) / 1024;
    const int32_t up = (y2 * ce - z1 * se) / 1024;
    const int16_t px = screenX(x2), py = IRON_CY - up * IRON_PX_X100 / 100 / 256;
    if (s) {
      const int16_t steps = (px > lastX ? px - lastX : lastX - px) + (py > lastY ? py - lastY : lastY - py) + 1;
      for (int16_t k = 0; k <= steps; k++) {
        plot(lastX + (px - lastX) * k / steps, lastY + (py - lastY) * k / steps);
      }
    }
    lastX = px;
    lastY = py;
  }
}

// A temperature with its unit, a black pixel around it so it stands free of the iron
static void ironLabel(int16_t centreX, int16_t top, uint16_t value) {
  char text[10];
  char *end = dogFormat(text, value);
  strcpy(end, getSettingValue(SettingsOptions::TemperatureInF) ? "°F" : "°C");
  const int16_t width = dogTextWidth(dogFontSmall, text);
  int16_t       x     = centreX - width / 2;
  x                   = x < 1 ? 1 : (x + width > OLED_WIDTH - 1 ? OLED_WIDTH - 1 - width : x);
  top                 = top < 1 ? 1 : (top > OLED_HEIGHT - 9 ? OLED_HEIGHT - 9 : top);
  OLED::drawFilledRect(x - 1, top - 1, x + width, top + 7, true);
  dogSmall(x, text, top + 7);
}

static void drawIron(uint16_t shown) {
  static int32_t roll = 0, pitch = 0; // what is shown, following the sensor smoothly
  dogHomeMotionUpdate();
  const int32_t targetRoll = dogHomeRollDeg() > 60 ? 60 : (dogHomeRollDeg() < -60 ? -60 : dogHomeRollDeg());
  const int32_t targetPitch = dogHomeTipDown() ? -IRON_PITCH_DOWN : 0;
  auto ease = [](int32_t &value, int32_t target) { // a third of the way each frame, at least a degree
    const int32_t step = (target - value) / 3;
    value += step ? step : (target > value) - (target < value);
  };
  ease(roll, targetRoll);
  ease(pitch, targetPitch);
  int32_t shownRoll = roll, shownPitch = pitch;
  if (dogHomeShaking()) { // it wobbles, three times a second
    const int32_t phase = (xTaskGetTickCount() * 1080 / TICKS_SECOND) % 360;
    shownRoll += 16 * isin(phase) / 1024;
    shownPitch += 5 * isin(phase * 13 / 10 + 57) / 1024;
  }
  for (uint8_t x = 0; x < OLED_WIDTH; x += 2) {
    plot(x, 31);
  }
  int16_t tipX = 0, tipY = 0, gripX = 0, gripTop = 0, gripBottom = 0;
  drawIronAt(shownRoll, shownPitch, !normal(), tipX, tipY, gripX, gripTop, gripBottom);
  // The tip's temperature under the tip, or above it if there is no room; the grip's above the grip, or below it
  ironLabel(tipX, tipY + 9 <= OLED_HEIGHT - 2 ? tipY + 3 : tipY - 10, shown);
  ironLabel(gripX, gripTop - 9 >= 1 ? gripTop - 9 : gripBottom + 2, gripTemperature());
}

void dogDrawHomeView(uint8_t view, uint16_t shownTemperature) {
  switch (view) {
  case 1:
    drawSymbols(shownTemperature);
    break;
  case 2:
    drawShepherd(shownTemperature);
    break;
  case 3:
    drawCooling(shownTemperature);
    break;
  default:
    drawIron(shownTemperature);
    break;
  }
}
#endif

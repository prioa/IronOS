/*
 * TS101_DOG_UI: the home views besides the dog (setting DogHomeVariant, a rear click on the home screen picks the next one).
 *   1 Symbole:     the dog's head, then a thermometer, a clock and an hourglass with their values and names below
 *   2 Schäferhund: tip and grip temperature small on the left, the rest is a German Shepherd trotting in, wagging, sitting, leaving
 *   3 Abkühlen:    the dog's head, the large temperature with its arrow, when the tip will be hand warm, the last 10 minutes
 *   4 Kolben:      the iron as it lies in the hand, from the sensor alone: its long axis tilts with the iron's, the display, button
 *                  and cable turn as it turns about that axis; the tip and grip temperatures where they belong
 * Turned the other way round, the head moves to the other side; the shepherd's screen stays as it is, the iron's tip is where the real one is.
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
    uint32_t       p   = 65536; // p * mid stays below 2^32: mid is under 65536
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
  uint32_t       left   = (uint32_t)now * 16; // 16 = 1 C; times the factor it stays below 2^32
  uint16_t       steps  = 0;
  while (left > (uint32_t)(HAND_WARM_C - room) * 16 && steps < 1000) {
    left = left * factor / 65536;
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

// ---- 4 Kolben, and the small iron of the home screen ----
// The iron along its axis in 1/256 of the model's units: end cap, joint, grip with the display, joint, neck, joint, tip
struct Ring {
  int16_t u, r;
  bool    joinsPrevious; // false: a dark joint lies between this and the ring before
};
static const Ring PROFILE[] = {{-1267, 102, false}, {-1152, 113, true}, {-1062, 128, false}, {-998, 143, true}, {486, 143, true}, {563, 128, true}, {640, 108, false},
                               {819, 87, true},     {896, 61, false},   {998, 41, true},     {1331, 18, true}, {1459, 0, true}};
#define PROFILE_RINGS  (sizeof(PROFILE) / sizeof(PROFILE[0]))
#define IRON_SHIFT     192 // the axis is moved this far towards the cap, so the iron sits in the middle
#define IRON_ELEVATION 30  // the iron is seen from in front and above at this angle
#define IRON_TILT_MAX  40 // steeper, the long iron would leave the screen

// The radius at u, 0 in a joint or beyond the ends
static int32_t radiusAt(int32_t u) {
  for (uint8_t i = 1; i < PROFILE_RINGS; i++) {
    if (u <= PROFILE[i].u) {
      if (u < PROFILE[i - 1].u || !PROFILE[i].joinsPrevious) {
        return 0;
      }
      return PROFILE[i - 1].r + (int32_t)(PROFILE[i].r - PROFILE[i - 1].r) * (u - PROFILE[i - 1].u) / (PROFILE[i].u - PROFILE[i - 1].u);
    }
  }
  return 0;
}

// A strip on the surface between the screen angles centre-half and centre+half (0 = straight up on the screen, 90 = facing the viewer)
static void surfaceStrip(int16_t x, int32_t cy, int32_t radius, int32_t centre, int32_t half, bool on, int16_t y0, int16_t y1) {
  int32_t a0 = centre - half, a1 = centre + half;
  a0 = a0 < 1 ? 1 : a0;
  a1 = a1 > 179 ? 179 : a1;
  if (a0 >= a1) {
    return; // on the far side
  }
  int32_t top = cy - radius * icos(a0) / 1024, bottom = cy - radius * icos(a1) / 1024;
  top    = top < y0 ? y0 : top;
  bottom = bottom > y1 ? y1 : bottom;
  if (top <= bottom) {
    OLED::drawFilledRect(x, top, x, bottom, !on);
  }
}

struct IronBox {
  int16_t x0, y0, w, h;
  int16_t pxX100;    // pixels a unit x 100
  int16_t thickX100; // the iron drawn this much thicker than it is
};
struct IronMarks {
  int16_t tipX, tipY, gripX, gripTop, gripBottom;
};
// Where the iron was drawn, column by column, so the temperatures can find a free place: top > bottom = nothing there
static int8_t ironTop[OLED_WIDTH], ironBottom[OLED_WIDTH];

// The iron as it lies: tilt is its long axis against the horizon (tip down positive), turn the turn about it (0 board up, 90 on its
// side towards the viewer). The tip points right or left; a black pixel around it keeps it clear of what is behind
static IronMarks drawIronIn(const IronBox &box, int32_t tilt, int32_t turn, bool tipRight) {
  const int32_t ct = icos(tilt), st = isin(tilt), ce = icos(IRON_ELEVATION), dir = tipRight ? 1 : -1;
  const int16_t cx = box.x0 + box.w / 2, cy0 = box.y0 + box.h / 2, last = box.y0 + box.h - 1;
  const int32_t facing = turn + IRON_ELEVATION; // where the display is on the screen, 90 = facing the viewer
  IronMarks     marks  = {cx, cy0, cx, (int16_t)last, box.y0};
  // Tilted, the iron is drawn smaller so its ends stay inside the box; the angle stays the real one
  const int32_t reach = PROFILE[PROFILE_RINGS - 1].u + IRON_SHIFT; // the tip, the farthest from the middle
  const int32_t drop  = reach * (st < 0 ? -st : st) / 1024 * ce / 1024;
  int32_t       px100 = box.pxX100;
  if (drop) {
    const int32_t fit = (int32_t)(box.h / 2 - 3) * 100 * 256 / drop;
    px100             = fit < px100 ? fit : px100;
  }
  auto alongToPx = [&](int32_t along) { return along * px100 / 100 / 256; };
  memset(ironTop, OLED_HEIGHT, sizeof(ironTop));
  memset(ironBottom, -1, sizeof(ironBottom));
  for (uint8_t pass = 0; pass < 2; pass++) { // first the black outline, then the iron
    for (int16_t col = box.x0; col < box.x0 + box.w; col++) {
      const int32_t along = (int32_t)(col - cx) * dir * 100 * 256 / px100 * 1024 / ct;
      const int32_t u     = along - IRON_SHIFT;
      const int32_t r     = radiusAt(u);
      if (!r) {
        continue; // beyond the ends, or a joint: dark
      }
      const int32_t h  = r * box.thickX100 / 100 * px100 / 100 / 256;
      const int32_t cy = cy0 + alongToPx(along * st / 1024 * ce / 1024);
      int32_t       y0 = cy - h - (pass ? 0 : 1), y1 = cy + h + (pass ? 0 : 1);
      y0               = y0 < box.y0 ? box.y0 : y0;
      y1               = y1 > last ? last : y1;
      if (y0 > y1) {
        continue;
      }
      if (!pass) {
        OLED::drawFilledRect(col > box.x0 ? col - 1 : col, y0, col + 1 < box.x0 + box.w ? col + 1 : col, y1, true);
        continue;
      }
      OLED::drawFilledRect(col, y0, col, y1, false);
      ironTop[col]    = y0;
      ironBottom[col] = y1;
      if (u >= -691 && u <= 38) { // the display window, the display inside it
        surfaceStrip(col, cy, h, facing, 25, false, y0, y1);
        if (u >= -627 && u <= -26) {
          surfaceStrip(col, cy, h, facing, 8, true, y0, y1);
        }
      } else if (u >= 141 && u <= 243) { // the button
        surfaceStrip(col, cy, h, facing, 18, false, y0, y1);
      }
      if (u >= -400 && u <= -200) { // where the grip's temperature goes
        marks.gripTop    = y0 < marks.gripTop ? y0 : marks.gripTop;
        marks.gripBottom = y1 > marks.gripBottom ? y1 : marks.gripBottom;
        marks.gripX      = col;
      }
    }
  }
  const int32_t tipAlong = PROFILE[PROFILE_RINGS - 1].u + IRON_SHIFT;
  marks.tipX             = cx + dir * alongToPx(tipAlong * ct / 1024);
  marks.tipY             = cy0 + alongToPx(tipAlong * st / 1024 * ce / 1024);
  // The cable leaves the end cap away from the tip and hangs down
  const int32_t capAlong = PROFILE[0].u + IRON_SHIFT;
  int16_t       lastX = 0, lastY = 0;
  for (int32_t s = 0; s <= 12; s++) {
    const int32_t along = capAlong - 538 * s / 12;                  // 2.1 units out
    const int32_t droop = 243 * s * s / 144 * box.thickX100 / 100; // and .95 units down at the end
    const int16_t px = cx + dir * alongToPx(along * ct / 1024), py = cy0 + alongToPx(along * st / 1024 * ce / 1024 + droop);
    if (s) {
      const int16_t steps = (px > lastX ? px - lastX : lastX - px) + (py > lastY ? py - lastY : lastY - py) + 1;
      for (int16_t k = 0; k <= steps; k++) {
        const int16_t x = lastX + (px - lastX) * k / steps, y = lastY + (py - lastY) * k / steps;
        if (x >= box.x0 && x < box.x0 + box.w && y >= box.y0 && y <= last) {
          plot(x, y);
        }
      }
    }
    lastX = px;
    lastY = py;
  }
  return marks;
}

// What the sensor says: the tilt of the long axis and the turn about it, both as the viewer sees them. The sensor is read 10 times a
// second; halfway to each new reading per frame (25 a second), so the iron follows within a tenth of a second without jumping
void dogIronPose(int32_t &tilt, int32_t &turn) {
  static int32_t shownTilt = 0, shownTurn = 0;
  int16_t        along = 0, about = 0;
  dogIronLie(along, about);
  int32_t targetTilt = along > IRON_TILT_MAX ? IRON_TILT_MAX : (along < -IRON_TILT_MAX ? -IRON_TILT_MAX : along);
  // Held the other way round, the side towards the viewer is the other one
  const int32_t targetTurn = normal() ? about : -about;
  int32_t       turnStep   = targetTurn - shownTurn;
  turnStep                 = turnStep > 180 ? turnStep - 360 : (turnStep < -180 ? turnStep + 360 : turnStep); // the short way round
  auto half = [](int32_t difference) { return difference / 2 ? difference / 2 : difference; }; // the last degree at once
  shownTilt += half(targetTilt - shownTilt);
  shownTurn += half(turnStep);
  shownTurn = shownTurn > 180 ? shownTurn - 360 : (shownTurn < -180 ? shownTurn + 360 : shownTurn);
  tilt      = shownTilt;
  turn      = shownTurn;
}

// The small iron of the home screen, under the statistics; the tip on the side of the real tip
void dogDrawSmallIron() {
  static const IronBox SMALL = {33, 13, 34, 19, 288, 190}; // a pixel clear of the head
  int32_t              tilt, turn;
  dogIronPose(tilt, turn);
  IronBox box = SMALL;
  box.x0      = normal() ? SMALL.x0 : OLED_WIDTH - SMALL.x0 - SMALL.w;
  drawIronIn(box, tilt, turn, normal());
}

// The text's box with a pixel around it is on the screen and clear of the iron
static bool placeFree(int16_t x, int16_t top, int16_t width) {
  if (x < 1 || x + width > OLED_WIDTH - 1 || top < 1 || top + 8 > 30) {
    return false;
  }
  for (int16_t col = x - 1; col <= x + width; col++) {
    if (ironTop[col] <= top + 7 && ironBottom[col] >= top - 1) {
      return false;
    }
  }
  return true;
}

// A temperature with its unit at the first free place of up to four, a black pixel around it
static void ironLabel(const int16_t (*places)[2], uint8_t count, uint16_t value) {
  char  text[10];
  char *end = dogFormat(text, value);
  strcpy(end, getSettingValue(SettingsOptions::TemperatureInF) ? "°F" : "°C");
  const int16_t width = dogTextWidth(dogFontSmall, text);
  int16_t       x = places[0][0], top = places[0][1];
  for (uint8_t i = 0; i < count; i++) {
    int16_t px = places[i][0] < 0 ? -places[i][0] - width : places[i][0]; // negative: the right end is given
    px         = px < 1 ? 1 : (px + width > OLED_WIDTH - 1 ? OLED_WIDTH - 1 - width : px); // pushed back onto the screen
    if (placeFree(px, places[i][1], width)) {
      x   = px;
      top = places[i][1];
      break;
    }
    if (i == 0) {
      x = px;
    }
  }
  x   = x < 1 ? 1 : (x + width > OLED_WIDTH - 1 ? OLED_WIDTH - 1 - width : x);
  top = top < 1 ? 1 : (top > 30 - 8 ? 30 - 8 : top);
  OLED::drawFilledRect(x - 1, top - 1, x + width, top + 7, true);
  dogSmall(x, text, top + 7);
}

static void drawIron(uint16_t shown) {
  static const IronBox FULL = {0, 0, OLED_WIDTH, 30, 853, 115};
  int32_t              tilt, turn;
  dogIronPose(tilt, turn);
  for (uint8_t x = 0; x < OLED_WIDTH; x += 2) {
    plot(x, 31);
  }
  const bool      tipRight = normal();
  const IronMarks m        = drawIronIn(FULL, tilt, turn, tipRight);
  // The tip's temperature beyond the tip, else under or over it; the grip's over the grip, else under it or at the cap's end
  auto          at     = [](int32_t v) { return (int16_t)v; };
  const int16_t beyond = at(tipRight ? m.tipX + 3 : -(m.tipX - 2));
  const int16_t tip[4][2] = {{beyond, at(m.tipY - 3)}, {at(m.tipX - 10), at(m.tipY + 3)}, {at(m.tipX - 10), at(m.tipY - 11)}, {beyond, 1}};
  ironLabel(tip, 4, shown);
  const int16_t cap     = at(tipRight ? 1 : -(OLED_WIDTH - 1));
  const int16_t grip[4][2] = {{at(m.gripX - 12), at(m.gripTop - 9)}, {at(m.gripX - 12), at(m.gripBottom + 2)}, {cap, 1}, {cap, 22}};
  ironLabel(grip, 4, gripTemperature());
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

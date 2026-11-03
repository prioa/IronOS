/*
 * TS101_DOG_UI interface v2: the zones of the grid, see ts101_v2_grid.hpp
 */
#include "ts101_v2_grid.hpp"
#ifdef TS101_DOG_UI
#include "OLED.hpp"
#include <string.h>

int16_t gridOuterX0 = 0, gridOuterX1 = OLED_WIDTH - 1;

static inline int16_t maxOf(int16_t a, int16_t b) { return a > b ? a : b; }
static inline int16_t minOf(int16_t a, int16_t b) { return a < b ? a : b; }

// The rectangle cut to the clip and the screen; false when nothing is left
static bool clipRect(int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) {
  x0 = maxOf(x0, maxOf(dogClipX0, 0));
  x1 = minOf(x1, minOf(dogClipX1, OLED_WIDTH - 1));
  y0 = maxOf(y0, 0);
  y1 = minOf(y1, OLED_HEIGHT - 1);
  return x0 <= x1 && y0 <= y1;
}

void gridFill(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  if (clipRect(x0, y0, x1, y1)) {
    OLED::drawFilledRect(x0, y0, x1, y1, false);
  }
}

void gridInvert(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  if (clipRect(x0, y0, x1, y1)) {
    OLED::invertRect(x0, y0, x1, y1);
  }
}

void gridIcon(int16_t x, int16_t y, const DogIcon &icon) { dogBlit(x, y, icon.width, ((icon.height + 7) / 8) * 8, icon.data); }

void gridDottedRow(int16_t x0, int16_t x1, int16_t y, uint8_t every) {
  for (int16_t x = x0; x <= x1; x += every) {
    gridFill(x, y, x, y);
  }
}

void gridDottedColumn(int16_t x, int16_t y0, int16_t y1, uint8_t every) {
  for (int16_t y = y0; y <= y1; y += every) {
    gridFill(x, y, x, y);
  }
}

void gridTextRight(const DogFont &font, int16_t x1, int16_t base, const char *text) { dogText(font, x1 + 1 - dogTextWidth(font, text), base, text); }

void gridTextCentre(const DogFont &font, int16_t x0, int16_t x1, int16_t base, const char *text) { dogText(font, x0 + (x1 - x0 + 1 - dogTextWidth(font, text)) / 2, base, text); }

// ---- A ----
void gridName(const char *text) {
  gridClip(0, GRID_NAME_END);
  dogText(dogFontName, 0, GRID_NAME_BASE, text);
  gridClipAll();
}

// ---- B ----
void gridValues(int16_t x, const char *values) {
  gridClip(0, GRID_LEFT_END);
  dogText(dogFontName, x, GRID_ROW2_BASE, values);
  gridClipAll();
}

void gridState(const char *label, const char *values) {
  gridClip(0, GRID_LEFT_END);
  const int16_t right = dogTextWidth(dogFontName, label) + 1; // the text from x 1, a pixel of the box on both sides
  dogText(dogFontName, 1, GRID_ROW2_BASE, label);
  gridInvert(0, GRID_ROW2_TOP, right, GRID_ROW2_BOT);
  if (values && *values) {
    dogText(dogFontName, right + 4, GRID_ROW2_BASE, values); // 3 px apart
  }
  gridClipAll();
}

void gridWarn(const char *text) {
  gridClip(0, GRID_LEFT_END);
  dogText(dogFontName, 1, GRID_ROW2_BASE, text);
  gridInvert(0, GRID_ROW2_TOP, GRID_LEFT_END, GRID_ROW2_BOT);
  gridClipAll();
}

// ---- C ----
static int16_t curveY(bool withTarget, int32_t targetC, int32_t c) {
  int32_t y = withTarget ? GRID_CURVE_BOT - ((c - (targetC - 80)) * 8) / 80 : GRID_CURVE_BOT - ((c - 20) * 9) / 400;
  return y < GRID_CURVE_TOP ? GRID_CURVE_TOP : (y > GRID_CURVE_BOT ? GRID_CURVE_BOT : y);
}

void gridCurve(bool withTarget, uint16_t targetC) {
  gridClip(0, GRID_LEFT_END);
  if (withTarget) {
    gridDottedRow(0, GRID_LEFT_END, 23, 3); // 31 - 80 * 8 / 80
  }
  const uint8_t count = dogCurveCount() < GRID_LEFT_END + 1 ? dogCurveCount() : GRID_LEFT_END + 1;
  for (uint8_t back = 0; back < count; back++) {
    const int16_t x    = GRID_LEFT_END - back;
    const int16_t y    = curveY(withTarget, targetC, dogCurveAt(back));
    const int16_t prev = back + 1 < count ? curveY(withTarget, targetC, dogCurveAt(back + 1)) : y; // the older neighbour
    gridFill(x, minOf(y, prev), x, maxOf(y, prev));
  }
  gridClipAll();
}

// ---- D ----
void gridTemp(uint16_t value) {
  char text[6];
  dogFormat(text, value);
  const int16_t width = dogTextWidth(dogFontNum, text) + 1 + dogV2Ring.width;
  const int16_t x     = OLED_WIDTH - width;
  dogText(dogFontNum, x, 11, text);
  gridIcon(OLED_WIDTH - dogV2Ring.width, 0, dogV2Ring);
}

// ---- E ----
void gridTarget(int8_t dir, uint16_t target) {
  char  text[10];
  char *p = text;
  *p++    = dir > 0 ? '{' : (dir < 0 ? '}' : '~');
  if (target) {
    *p++ = ' ';
    p    = dogFormat(p, target);
    strcpy(p, "°");
  } else {
    *p = 0;
  }
  gridTextRight(dogFont3x5, OLED_WIDTH - 1, GRID_TARGET_BASE, text);
}

// ---- F ----
static void keyBox(int16_t x, const DogIcon *icon, bool inverted) {
  gridFill(x, GRID_KEY_TOP, x + GRID_KEY_W - 1, GRID_KEY_TOP);
  gridFill(x, GRID_KEY_BOT, x + GRID_KEY_W - 1, GRID_KEY_BOT);
  gridFill(x, GRID_KEY_TOP, x, GRID_KEY_BOT);
  gridFill(x + GRID_KEY_W - 1, GRID_KEY_TOP, x + GRID_KEY_W - 1, GRID_KEY_BOT);
  if (icon) {
    gridIcon(x + (GRID_KEY_W - icon->width) / 2, GRID_KEY_TOP + (GRID_KEY_BOT - GRID_KEY_TOP + 1 - icon->height) / 2, *icon);
  }
  if (inverted) {
    gridInvert(x + 1, GRID_KEY_TOP + 1, x + GRID_KEY_W - 2, GRID_KEY_BOT - 1);
  }
}

void gridKeys(const DogIcon *rear, const DogIcon *front, bool invertRear, bool invertFront) {
  // The boxes sit over their buttons: with the screen turned the front button is on the left
  const bool frontRight = dogFrontOnRight();
  keyBox(frontRight ? GRID_KEY_REAR_X : GRID_KEY_FRONT_X, rear, invertRear || dogPressed(false));
  keyBox(frontRight ? GRID_KEY_FRONT_X : GRID_KEY_REAR_X, front, invertFront || dogPressed(true));
}

void gridWideKey(const char *text) {
  const int16_t x0 = GRID_KEY_REAR_X, x1 = OLED_WIDTH - 1;
  gridFill(x0, GRID_KEY_TOP, x1, GRID_KEY_TOP);
  gridFill(x0, GRID_KEY_BOT, x1, GRID_KEY_BOT);
  gridFill(x0, GRID_KEY_TOP, x0, GRID_KEY_BOT);
  gridFill(x1, GRID_KEY_TOP, x1, GRID_KEY_BOT);
  gridTextCentre(dogFont3x5, x0, x1, 28, text); // rows 23-27, the middle of 19-30
  if (dogPressed(false) || dogPressed(true)) {
    gridInvert(x0 + 1, GRID_KEY_TOP + 1, x1 - 1, GRID_KEY_BOT - 1);
  }
}
#endif

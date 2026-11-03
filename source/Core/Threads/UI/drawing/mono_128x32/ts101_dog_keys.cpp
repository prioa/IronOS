/*
 * TS101_DOG_UI: text in the bitmap fonts and the key row shared by every screen.
 * The row reads the buttons directly: getButtonState() only reports a press when it ends or after 400 ms,
 * too late to invert a field on touch or to fill a bar from the first moment.
 */
#include "ts101_dog_ui.hpp"
#include "ts101_v2_fonts.h"
#ifdef TS101_DOG_UI
#include "BSP.h"
#include "OLED.hpp"
#include "Settings.h"
#include "configuration.h"

// ---- Text ----
static uint8_t nextCode(const char *&text) {
  uint8_t c = *text++;
  if ((c == 0xC2 || c == 0xC3) && *text) { // (codes below 0x20 are the v2 name font's squashed Ä Ö Ü)
    c = *text++; // Ä Ö Ü ° are stored under their second UTF-8 byte
  }
  return c;
}

static int16_t findGlyph(const DogFont &font, uint8_t code) {
  for (uint8_t g = 0; g < font.count; g++) {
    if (font.codes[g] == code) {
      return g;
    }
  }
  return -1;
}

uint8_t dogTextWidth(const DogFont &font, const char *text) {
  uint8_t pen = 0, width = 0;
  while (*text) {
    int16_t g = findGlyph(font, nextCode(text));
    if (g >= 0) {
      if (font.widths[g]) {
        width = pen + font.widths[g];
      }
      pen += font.advances[g];
    }
  }
  return width;
}

void dogText(const DogFont &font, int16_t x, int16_t base, const char *text) {
  while (*text) {
    int16_t g = findGlyph(font, nextCode(text));
    if (g >= 0) {
      dogBlit(x, base - font.baseline, font.widths[g], font.pages * 8, font.bitmap + font.offsets[g]);
      x += font.advances[g];
    }
  }
}

void dogSmall(int16_t x, const char *text, int16_t base) { dogText(dogFont3x5, x, base, text); } // interface v2: the 3x5 font
void dogSmallCentre(int16_t x0, int16_t x1, const char *text, int16_t base) { dogSmall(x0 + (x1 - x0 + 1 - dogTextWidth(dogFont3x5, text)) / 2, text, base); }
void dogSmallRight(int16_t x1, const char *text, int16_t base) { dogSmall(x1 - dogTextWidth(dogFont3x5, text) + 1, text, base); }

char *dogFormat(char *out, uint16_t value) {
  char     digits[5];
  uint8_t  n = 0;
  do {
    digits[n++] = '0' + value % 10;
    value /= 10;
  } while (value);
  while (n) {
    *out++ = digits[--n];
  }
  *out = 0;
  return out;
}

static const uint8_t RING_WIDTH = 5;

uint8_t dogBigWidth(uint16_t value) {
  char text[6];
  dogFormat(text, value);
  return dogTextWidth(dogFontBig, text) + 2 + RING_WIDTH;
}

void dogBig(int16_t x, uint16_t value) {
  char text[6];
  dogFormat(text, value);
  dogText(dogFontBig, x, 20, text);
  dogBlit(x + dogTextWidth(dogFontBig, text) + 2, 0, RING_WIDTH, 8, dogRing);
}

void dogBigRight(int16_t x1, uint16_t value) { dogBig(x1 - dogBigWidth(value) + 1, value); }

// ---- Buttons ----
static TickType_t heldSince[2];  // 0 = front, 1 = rear; 0 ticks = not held
static bool       bothDown = false; // both buttons were down: lock or back, the one released last must not start anything
static uint8_t    consumed = 0;    // >0: the press has done its job. Counts down after the release, so its click event is hidden too
static bool       locked   = false;
static bool       bothNow  = false; // both buttons are down right now, whatever else is going on

ButtonState dogButtonsPoll(ButtonState buttons) {
  const bool swapped = getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled);
  const bool a       = swapped ? getButtonB() : getButtonA();
  const bool b       = swapped ? getButtonA() : getButtonB();
  bothNow = a && b;
  if (a && b) {
    bothDown = true;
  } else if (!a && !b) {
    bothDown = false;
  }
  const bool       masked = consumed != 0;
  const TickType_t now    = xTaskGetTickCount() | 1;
  if (consumed && !a && !b) {
    consumed--; // getButtonState reads the pins just before this, so this frame and the next can still report the click
  } else if (consumed) {
    consumed = 2;
  }
  const bool idle = bothDown || masked || locked;
  heldSince[0]    = (a && !idle) ? (heldSince[0] ? heldSince[0] : now) : 0;
  heldSince[1]    = (b && !idle) ? (heldSince[1] ? heldSince[1] : now) : 0;
  return masked ? BUTTON_NONE : buttons;
}

bool dogBothDown() { return bothNow; }

bool dogPressed(bool front) { return heldSince[front ? 0 : 1] != 0; }

TickType_t dogHeldTicks(bool front) {
  const TickType_t since = heldSince[front ? 0 : 1];
  return since ? (xTaskGetTickCount() | 1) - since : 0;
}

void dogPressConsumed() {
  consumed     = 2;
  heldSince[0] = heldSince[1] = 0;
}

void dogSetLocked(bool isLocked) { locked = isLocked; }
bool dogLocked() { return locked; }

bool dogHoldDone(bool front, TickType_t full) { return dogHeldTicks(front) >= full; }

// Swap buttons swaps the roles of the two buttons, so the label of the "front" role moves to the other side with them
bool dogFrontOnRight() { return OLED::getRotation() != (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0); }
#endif

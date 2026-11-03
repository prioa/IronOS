/*
 * TS101_DOG_UI interface v2: sleep is a screen saver, no grid and no keys
 *
 * A round at 8 frames a second, the dog on row 1: it trots in from outside on the left with frames 0-7, 2 px a frame, up to its
 * stop - 12; then ss_mid[] (slowing down, sitting, dozing with z rising, getting up, squatting while the heap grows, walking
 * on), then the trot again until it is out on the right. The heap stays where it squatted until the round is over. The second
 * round is mirrored and goes to the left; then the first again. The moon with the temperature and the minutes until the iron
 * switches off stands at the top of the side that is free, 3 px clear of the dog, or not at all. Everything moves: nothing
 * stays lit for good.
 */
#include "ts101_v2_solder.hpp"
#ifdef TS101_DOG_UI
#include "OperatingModes.h"
#include "ts101_screensaver_dog.h"
#include "ui_drawing.hpp"
#include <string.h>

extern TickType_t lastMovementTime;

#define SLEEP_FRAME_TICKS (TICKS_SECOND / 8)
#define SLEEP_DOG_Y       1
#define SLEEP_STEP        2
#define SLEEP_PILE_DX     1  // the heap from this column of the dog's frame: behind the hind legs as it squats
#define SLEEP_PILE_Y      27
#define SLEEP_GAP         3
#define MID_STEPS         (sizeof(ss_mid) / sizeof(ss_mid[0]))

// The dog's frame, row by row with the most significant bit on the left; mirrored it looks the other way
static void blitRows(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t bytesPerRow, const uint8_t *data, bool mirrored) {
  for (uint8_t row = 0; row < height; row++) {
    const int16_t py = y + row;
    if (py < 0 || py >= OLED_HEIGHT) {
      continue;
    }
    for (uint8_t col = 0; col < width; col++) {
      if (!((data[row * bytesPerRow + col / 8] >> (7 - col % 8)) & 1)) {
        continue;
      }
      const int16_t px = mirrored ? x + width - 1 - col : x + col;
      if (px >= 0 && px < OLED_WIDTH) {
        OLED::drawFilledRect(px, py, px, py, false);
      }
    }
  }
}

// Where a round is after this many frames. x is counted as if the round went to the right; the mirrored one is drawn the other
// way round. Returns false once the round is over
struct SleepPose {
  int16_t x;       // left edge of the frame, in the round's own direction
  uint8_t frame;
  uint8_t pile;    // 0-4: how much of the heap there is
  int16_t pileX;   // in the round's own direction
  uint8_t zzz;     // 0 none, else how many z show
};

static bool poseAt(uint16_t n, int16_t stop, SleepPose &p) {
  const int16_t  start = -SS_DOG_W;
  const uint16_t walkIn = (stop - 12 - start) / SLEEP_STEP; // frames of trotting in
  p.pile = 0;
  p.zzz  = 0;
  if (n < walkIn) {
    p.x     = start + n * SLEEP_STEP;
    p.frame = n % 8;
    return true;
  }
  int16_t  x = stop - 12;
  uint16_t i = n - walkIn;
  uint16_t dozing = 0;
  for (uint16_t s = 0; s < MID_STEPS; s++) {
    if (s == i) {
      p.x     = x;
      p.frame = ss_mid[s].frame;
      p.pile  = ss_mid[s].pile;
      p.pileX = x + SLEEP_PILE_DX;
      p.zzz   = ss_mid[s].zzz ? 1 + (dozing / 3) % 3 : 0; // one more every 3 frames up to 3, then from the start
      return true;
    }
    if (ss_mid[s].pile && !p.pile) {
      p.pileX = x + SLEEP_PILE_DX;
    }
    if (ss_mid[s].pile) {
      p.pile = ss_mid[s].pile; // the heap stays behind
    }
    dozing = ss_mid[s].zzz ? dozing + 1 : 0;
    x += ss_mid[s].dx;
  }
  i -= MID_STEPS;
  p.x     = x + i * SLEEP_STEP;
  p.frame = i % 8;
  return p.x < OLED_WIDTH;
}

static uint16_t roundFrames(int16_t stop) {
  SleepPose p;
  uint16_t  n = 0;
  while (poseAt(n, stop, p)) {
    n++;
  }
  return n;
}

static int16_t onScreen(int16_t x, uint8_t width, bool mirrored) { return mirrored ? OLED_WIDTH - width - x : x; }

// "150° 9 MIN": the reading and the minutes until the iron switches off
static void formatInfo(char *out, uint16_t tipTemp) {
  out = dogFormatUnit(out, tipTemp, "°");
  const uint32_t shutdown = getSettingValue(SettingsOptions::ShutdownTime);
  if (shutdown) {
    const TickType_t last    = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
    const TickType_t elapsed = xTaskGetTickCount() - last;
    const TickType_t limit   = shutdown * 60 * TICKS_SECOND;
    const uint32_t   left    = elapsed < limit ? (limit - elapsed + 60 * TICKS_SECOND - 1) / (60 * TICKS_SECOND) : 0; // rounded up
    *out++                   = ' ';
    dogFormatUnit(out, left, " MIN");
  }
}

static TickType_t sleepStart = 0, sleepLastDraw = 0;

void dogDrawSleepV2(uint16_t tipTemp) {
  const TickType_t now = xTaskGetTickCount();
  if (!sleepStart || now - sleepLastDraw > TICKS_SECOND / 2) {
    sleepStart = now; // just fell asleep: the first round from its start
  }
  sleepLastDraw = now;
  static uint16_t lengths[2] = {0, 0};
  static const int16_t STOPS[2] = {28, OLED_WIDTH - SS_DOG_W - 52}; // the second round stops at 52 on the screen, looking left
  for (uint8_t r = 0; r < 2; r++) {
    if (!lengths[r]) {
      lengths[r] = roundFrames(STOPS[r]);
    }
  }
  uint32_t n = ((now - sleepStart) / SLEEP_FRAME_TICKS) % (lengths[0] + lengths[1]);
  const bool mirrored = n >= lengths[0];
  if (mirrored) {
    n -= lengths[0];
  }
  SleepPose p;
  poseAt(n, STOPS[mirrored], p);
  const int16_t dogX = onScreen(p.x, SS_DOG_W, mirrored);
  blitRows(dogX, SLEEP_DOG_Y, SS_DOG_W, SS_DOG_H, SS_DOG_BPR, ss_dog[p.frame], mirrored);
  if (p.pile) {
    // Stage n: the lowest n rows of the heap, sitting on its bottom row
    const int16_t x = onScreen(p.pileX, SS_PILE_W, mirrored);
    blitRows(x, SLEEP_PILE_Y + SS_PILE_H - p.pile, SS_PILE_W, p.pile, 2, ss_pile + (SS_PILE_H - p.pile) * 2, mirrored);
  }
  static const int8_t   ZX[3] = {17, 11, 5}, ZY[3] = {9, 4, -1};
  static const uint8_t *const Z[3] = {ss_z0, ss_z1, ss_z2};
  for (uint8_t i = 0; i < p.zzz; i++) {
    const uint8_t size = 3 + i;
    blitRows(onScreen(p.x + ZX[i], size, mirrored), SLEEP_DOG_Y + ZY[i], size, size, 1, Z[i], false); // the z keep reading as z
  }

  // The moon and the text on the free side, at the top
  char text[16];
  formatInfo(text, tipTemp);
  const int16_t width = dogV2Moon.width + 2 + dogTextWidth(dogFont3x5, text);
  const int16_t right = OLED_WIDTH - width; // the block's left edge on the right side
  int16_t       at    = -1;
  if (dogX + SS_DOG_W - 1 + SLEEP_GAP < right || dogX >= OLED_WIDTH) {
    at = right;
  } else if (width - 1 + SLEEP_GAP < dogX || dogX + SS_DOG_W <= 0) {
    at = 0;
  }
  if (at >= 0) {
    gridIcon(at, 0, dogV2Moon);
    dogText(dogFont3x5, at + dogV2Moon.width + 2, 6, text); // rows 1-5, next to the moon's middle
  }
}

void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  dogDrawSleepV2(tipTemp);
  OLED::refresh();
}
void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) { ui_draw_soldering_basic_sleep(tipTemp); }
#endif

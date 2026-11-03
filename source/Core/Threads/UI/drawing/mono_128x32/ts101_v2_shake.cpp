/*
 * TS101_DOG_UI interface v2: a shake, from the movement thread's readings (LIS2DH12 at 25 Hz, +-2 g as the stock firmware sets it)
 *
 * Per axis the reading minus its slow average (gravity and the hand's lie drop out). A swing counts when that is more than
 * SHAKE_G_X10 tenths of gravity, a direction change when the next counted swing on the same axis has the other sign. At least
 * SHAKE_TURNS changes within SHAKE_WINDOW on one axis is a shake; after it SHAKE_LOCKOUT passes before the next can count.
 * Gravity is the length of the reading at rest as this sensor reports it, so the threshold holds for the LIS clone too.
 * The range stays +-2 g: a higher one would change the stock motion detection that wakes the iron. So a swing can reach at most
 * 2 g on an axis across gravity and 1 g upwards on the one along it: there a reading at the end of the range counts as a swing.
 */
#include "ts101_v2_shake.hpp"
#ifdef TS101_DOG_UI

#define SHAKE_G_X10   13                    // a swing: 1.3 g away from the average
#define SHAKE_TURNS   3                     // direction changes
#define SHAKE_WINDOW  (TICKS_100MS * 6)     // from the first swing to the last
#define SHAKE_LOCKOUT (TICKS_SECOND)        // after a shake
#define CLIP_G_X10    19                    // a reading this far out is at the end of the +-2 g range
#define AVERAGE_SHIFT 4                     // the average follows 1/16 of the difference a reading: about 0.6 s at 25 Hz

static int32_t    average[3];           // * 16
static uint32_t   gravity    = 0;       // length at rest, raw counts * 16; 0 = not known yet
static int8_t     lastSign[3];          // of the last counted swing on each axis
static TickType_t swingAt[3][SHAKE_TURNS + 1]; // the newest counted swings, [0] the newest
static uint8_t    swings[3];
static TickType_t lockedUntil = 0;
static bool       started     = false;
static volatile bool pending  = false;

static uint32_t root(uint32_t value) {
  uint32_t r = 0;
  for (uint32_t bit = 1UL << 30; bit; bit >>= 2) {
    if (value >= r + bit) {
      value -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
  }
  return r;
}

void dogShakeReset() { started = false; }

void dogShakeFeed(int16_t x, int16_t y, int16_t z, TickType_t now) {
  const int32_t  raw[3] = {x, y, z};
  const uint32_t length = root((uint32_t)(x * x) + (uint32_t)(y * y) + (uint32_t)(z * z));
  if (!length) {
    return; // no sensor
  }
  if (!started) {
    for (uint8_t i = 0; i < 3; i++) {
      average[i]  = raw[i] * 16;
      lastSign[i] = 0;
      swings[i]   = 0;
    }
    if (!gravity) {
      gravity = length * 16;
    }
    lockedUntil = now;
    started     = true;
    return;
  }
  const uint32_t threshold = gravity * SHAKE_G_X10 / 160; // raw counts
  bool           calm      = true;
  for (uint8_t i = 0; i < 3; i++) {
    const int32_t off = raw[i] - average[i] / 16;
    average[i] += (raw[i] * 16 - average[i]) >> AVERAGE_SHIFT;
    const uint32_t size = off < 0 ? -off : off;
    if (size * 4 > threshold) {
      calm = false;
    }
    // At the end of the range the reading stops: a swing that reaches it counts too (the axis along gravity gets there with 1 g)
    const bool atClip = (uint32_t)(raw[i] < 0 ? -raw[i] : raw[i]) * 160 >= gravity * CLIP_G_X10 && (raw[i] < 0) == (off < 0);
    if (size <= threshold && !atClip) {
      continue;
    }
    const int8_t sign = off < 0 ? -1 : 1;
    if (sign == lastSign[i]) {
      swingAt[i][0] = now; // the same swing goes on
      continue;
    }
    lastSign[i] = sign;
    for (uint8_t k = SHAKE_TURNS; k > 0; k--) {
      swingAt[i][k] = swingAt[i][k - 1];
    }
    swingAt[i][0] = now;
    if (swings[i] < SHAKE_TURNS + 1) {
      swings[i]++;
    }
    // SHAKE_TURNS changes need SHAKE_TURNS + 1 swings, the oldest of them no longer ago than the window
    if (swings[i] == SHAKE_TURNS + 1 && now - swingAt[i][SHAKE_TURNS] <= SHAKE_WINDOW && (int32_t)(now - lockedUntil) >= 0) {
      pending     = true;
      lockedUntil = now + SHAKE_LOCKOUT;
      for (uint8_t a = 0; a < 3; a++) {
        swings[a]   = 0;
        lastSign[a] = 0;
      }
      return;
    }
  }
  if (calm) {
    gravity += ((int32_t)(length * 16) - (int32_t)gravity) / 32; // learns the sensor's 1 g while the iron is still
  }
}

bool dogShakeTaken() {
  if (!pending) {
    return false;
  }
  pending = false;
  return true;
}
#endif

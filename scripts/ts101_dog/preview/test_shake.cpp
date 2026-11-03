// Host test of the shake detector (ts101_v2_shake.cpp) with synthetic readings at 25 Hz, +-2 g: test_shake.sh
#include "ts101_v2_shake.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const float G = 1024; // raw counts per g in this test (the real scale does not matter: it is learnt)
static int         failures = 0;

static int16_t clip(float v) { return v > 2 * G ? 2 * G : (v < -2 * G ? -2 * G : (int16_t)v); }

// fn(t) gives the acceleration in g on the three axes; returns how many shakes were seen
template <typename F> static int run(const char *name, float seconds, F fn) {
  dogShakeReset();
  dogShakeTaken();
  int count = 0;
  for (int i = 0; i < seconds * 25; i++) {
    float t = i / 25.0f, a[3];
    fn(t, a);
    dogShakeFeed(clip(a[0] * G), clip(a[1] * G), clip(a[2] * G), (TickType_t)(t * 1000));
    count += dogShakeTaken();
  }
  printf("%-44s %d\n", name, count);
  return count;
}

static void expect(bool ok, const char *what) {
  if (!ok) {
    printf("FAIL: %s\n", what);
    failures++;
  }
}

static float noise() { return (rand() % 1000 - 500) / 500.0f * 0.05f; }

int main() {
  srand(1);
  // Lying still, gravity on z
  expect(run("still", 10, [](float, float *a) { a[0] = noise(), a[1] = noise(), a[2] = 1 + noise(); }) == 0, "still");
  // Soldering: slow wrist moves of +-0.4 g at 1 Hz, tilting
  expect(run("soldering, +-0.4 g at 1 Hz", 20, [](float t, float *a) {
           a[0] = 0.4f * sinf(6.28f * t) + noise(), a[1] = 0.3f * sinf(4.0f * t) + noise(), a[2] = 0.9f + noise();
         }) == 0,
         "soldering");
  // Putting it down on the stand: one hard knock of 1.8 g that rings once
  expect(run("knock on the stand", 4, [](float t, float *a) {
           float k = (t > 1.0f && t < 1.04f) ? 1.8f : ((t >= 1.04f && t < 1.12f) ? -0.9f : 0);
           a[0] = noise() + k, a[1] = noise(), a[2] = 1 + noise() + k;
         }) == 0,
         "knock");
  // Tapping solder off: three knocks on the stand 0.3 s apart, each 2 g down with a ring of 0.9 g back
  expect(run("three taps 0.3 s apart", 3, [](float t, float *a) {
           float k = 0;
           for (int i = 0; i < 3; i++) {
             float d = t - 1.0f - 0.3f * i;
             k += (d >= 0 && d < 0.04f) ? -2.0f : ((d >= 0.04f && d < 0.12f) ? 0.9f : 0);
           }
           a[0] = noise(), a[1] = noise(), a[2] = 1 + noise() + k;
         }) == 0,
         "taps");
  // Wiping the tip on brass wool: +-1 g at 5 Hz, short
  expect(run("tip wiped, +-1.0 g at 5 Hz", 3, [](float t, float *a) { a[0] = 1.0f * sinf(31.4f * t) + noise(), a[1] = noise(), a[2] = 1 + noise(); }) == 0, "wipe");
  // Shaking: +-1.8 g at 4 Hz across the iron, for 1 s
  expect(run("shake +-1.8 g at 4 Hz, 1 s", 2, [](float t, float *a) {
           a[0] = (t < 1 ? 1.8f * sinf(25.1f * t) : 0) + noise(), a[1] = noise(), a[2] = 1 + noise();
         }) == 1,
         "shake once");
  // Shaking at 3 Hz, along the axis gravity is on (it clips at 2 g)
  expect(run("shake +-1.8 g at 3 Hz on the gravity axis", 2, [](float t, float *a) {
           a[0] = noise(), a[1] = noise(), a[2] = 1 + (t < 1.2f ? 1.8f * sinf(18.8f * t) : 0) + noise();
         }) == 1,
         "shake on z");
  // Shaking for 3 s: the lockout lets it count about once a second, never twice within one
  int n = run("shake for 3 s", 3, [](float t, float *a) { a[0] = 1.8f * sinf(25.1f * t) + noise(), a[1] = noise(), a[2] = 1 + noise(); });
  expect(n >= 2 && n <= 3, "long shake counts once per second");
  printf(failures ? "%d failed\n" : "all passed\n", failures);
  return failures != 0;
}

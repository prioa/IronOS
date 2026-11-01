/*
 * TS101_DOG_UI: soldering, sleep and home screens, after the "Gegenentwürfe" boards.
 * The large temperature sits on the front button's side and the layout is mirrored with the orientation.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "power.hpp"
#include "ui_drawing.hpp"
#include "../../../../../version.h"

extern TickType_t lastMovementTime;

// The rows of the screens on the boards: the stage on rows 0-20, the power row on 23-30, full width labels on 21-30
static const int16_t BIG_RIGHT = 126;          // the large temperature with its ring ends here

static inline bool normal() { return OLED::getRotation(); }

static void label(int16_t x, uint8_t base, const char *text) { dogText(dogFontLabel, x, base, text); }

static uint32_t voltsX10() { return getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0); }

static uint32_t ampsX10() {
  const uint32_t v = voltsX10();
  return v ? (x10WattHistory.average() * 10 + v / 2) / v : 0;
}

static uint32_t wattsNow() { return (x10WattHistory.average() + 5) / 10; }

static void formatX10(char *out, uint32_t x10, char unit) {
  out    = dogFormat(out, x10 / 10);
  *out++ = '.';
  *out++ = '0' + x10 % 10;
  *out++ = unit;
  *out   = 0;
}

static void formatUnit(char *out, uint32_t value, const char *unit) {
  out = dogFormat(out, value);
  strcpy(out, unit);
}

static const char *sourceName() {
  const int8_t source = getPowerSourceNumber();
  return source == 0 ? "DC" : (source == 1 ? "USB" : "PD");
}

static void drawBig(uint16_t value) {
  if (normal()) {
    dogBigRight(BIG_RIGHT, value);
  } else {
    dogBig(-1, value); // the digits have an empty first column
  }
}

// Rear button held: a block with what it does, the temperature stays, the line below fills from the rear side
static bool drawStopHold(uint16_t temperature) {
  const TickType_t held = dogHeldTicks(false);
  if (held < DOG_HOLD_START) {
    return false;
  }
  OLED::clearScreen();
  const int16_t x0 = normal() ? 0 : OLED_WIDTH - 80;
  label(x0 + 13, 11, DOG_T("Halten:", "Hold:"));
  label(x0 + 21, 26, DOG_T("Stopp", "Stop"));
  OLED::invertRect(x0, 0, x0 + 79, 28);
  drawBig(temperature);
  uint32_t filled = (uint32_t)(held - DOG_HOLD_START) * OLED_WIDTH / (DOG_HOLD_STOP - DOG_HOLD_START);
  filled          = filled > OLED_WIDTH ? OLED_WIDTH : filled;
  if (filled) {
    if (dogFrontOnRight()) {
      OLED::drawFilledRect(0, 30, filled - 1, 31, false);
    } else {
      OLED::drawFilledRect(OLED_WIDTH - filled, 30, OLED_WIDTH - 1, 31, false);
    }
  }
  return true;
}

// ---- Heating, soldering and boost: the stage on the left, the temperature block on the right; turned, they swap ----
// Normal: stage from x=0, block at x=90 (arrow) and 97 (digits), keys at 96. Turned: block at 0, keys at 0, stage from 35.
static int16_t stageX() { return normal() ? 0 : 35; }
static int16_t blockX() { return normal() ? 90 : 0; }
#define STAGE_WIDTH 93
#define CURVE_TOP   19
#define CURVE_BOTTOM 31
#define DIGIT_PITCH 7 // the label font's digits are 6 wide

// The real tip temperature in the label font with the ring, the arrow in front of it: >0 up, <0 down
static void drawTempBlockAt(int16_t x, uint16_t value, int8_t arrow) {
  char          text[6];
  dogFormat(text, value);
  int16_t pen = x + 7;
  for (const char *c = text; *c; c++, pen += DIGIT_PITCH) {
    const char digit[2] = {*c, 0};
    dogText(dogFontLabel, pen, 10, digit);
  }
  dogBlit(pen + 1, 0, 5, 8, dogRing);
  if (arrow) {
    dogBlit(x, 3, 5, 8, arrow > 0 ? dogTrendUp : dogTrendDown);
  }
}

static void drawTempBlock(uint16_t value, int8_t arrow) { drawTempBlockAt(blockX(), value, arrow); }

// The tiny line under the temperature, right aligned under the ring
static void drawUnderTemp(const char *text, bool check) {
  const int16_t right = blockX() + 33, width = dogTextWidth(dogFontTiny, text);
  dogText(dogFontTiny, right - width + 1, 16, text);
  if (check) {
    dogBlit(right - width - 6, 11, 5, 8, iconTinyCheck);
  }
}

static void formatTarget(char *out, uint16_t value) {
  strcpy(out, DOG_T("ZIEL ", "SET "));
  formatUnit(out + strlen(out), value, "°");
}

// "65W 3.2A"
static void formatPower(char *out) {
  formatUnit(out, wattsNow(), "W ");
  formatX10(out + strlen(out), ampsX10(), 'A');
}

// The legs: frames advance at fpsX10 / 10 a second, from the time and not the frame rate of the screen
static const uint8_t *trotFrame(uint32_t fpsX10) {
  static const uint8_t *const frames[] = {dogTrot0, dogTrot1, dogTrot2, dogTrot3};
  static TickType_t           last     = 0;
  static uint32_t             phase    = 0; // frames * 10 * TICKS_SECOND
  const TickType_t            now      = xTaskGetTickCount();
  const TickType_t            dt       = now - last > TICKS_100MS ? TICKS_100MS : now - last;
  last                                 = now;
  phase                                = (phase + fpsX10 * dt) % (4 * 10 * TICKS_SECOND);
  return frames[phase / (10 * TICKS_SECOND)];
}

#define DOG_GROUND_MIN 21 // the dog is 13 rows tall and the row with the watts ends on row 6
#define DOG_FRONT_MAX  79 // its nose is 7 columns ahead of the front paw and has to stop before x=87: the arrow and ZIEL start there
// The dog 24x13, its columns shifted down by rowAt(column): it leans with the ground under it
static void drawTrot(int16_t x, const uint8_t *frame, int16_t yBack, int16_t yFront) {
  for (uint8_t col = 0; col < 24; col++) {
    // the paws are on columns 3 and 16; between and beyond them the line through both
    const int16_t num    = (yFront - yBack) * (col - 3);
    int16_t       ground = yBack + (num + (num < 0 ? -6 : 6)) / 13;
    ground               = ground < DOG_GROUND_MIN ? DOG_GROUND_MIN : ground; // the head stays under the power row
    for (uint8_t row = 0; row < 13; row++) {
      const int16_t px = x + col, py = ground - 13 + row;
      if (((frame[(row / 8) * 24 + col] >> (row % 8)) & 1) && px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
        OLED::drawFilledRect(px, py, px, py, false);
      }
    }
  }
}

static uint16_t setPoint() { return getSettingValue(SettingsOptions::SolderingTemp); }

/*
 * Heating up, only until the set point is reached the first time:
 *   65W 3.2A               ^ 185°
 *                           ZIEL 350°
 *   - - - - - - - - - - -  [ - ][ + ]
 *        dog on the curve  [   ][   ]
 */
static void drawHeatingScreen() {
  const int16_t x0 = stageX();
  char          text[16];
  formatPower(text);
  dogSmall(x0, text, 7);
  uint8_t       rows[STAGE_WIDTH];
  const uint8_t count = dogDrawCurve(x0, STAGE_WIDTH, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, rows);
  // The dog runs at the newest end of the curve, uphill; at the start it waits at the left end, and once it reaches the temperature
  // block it stays there and the curve goes on under it
  const int16_t front = count > DOG_FRONT_MAX ? DOG_FRONT_MAX : (count > 17 ? count - 1 : 16); // column of the front paw, in samples
  auto          rowAt = [&](int16_t i) -> int16_t { return count == 0 ? CURVE_BOTTOM : rows[i < 0 ? 0 : (i >= count ? count - 1 : i)]; };
  int16_t       yBack = rowAt(front - 13), yFront = rowAt(front);
  yFront              = yFront < yBack - 12 ? yBack - 12 : yFront; // at most 45 degrees
  uint32_t fpsX10     = wattsNow() * 200 / 65;                   // 20 frames a second at 65 W
  drawTrot(x0 + front - 16, trotFrame(fpsX10 > 200 ? 200 : fpsX10), yBack, yFront);
  drawTempBlock(getTipTemp(), 1);
  formatTarget(text, setPoint());
  drawUnderTemp(text, false);
  dogKeyPair(normal() ? 96 : 0, nullptr, nullptr);
}

/*
 * Boost, while the front button is held, 30 s at most:
 *   [bolt BOOST]             400°
 *                          NOCH 23 S
 *   ===  ==  dog sprinting
 *   - - - - - - - - - - - - - - - -  (the ground runs to the left)
 */
static void drawBoostScreen() {
  const int16_t x0 = stageX();
  dogBlit(x0 + 2, 1, 5, 8, iconBoltSmall);
  dogSmall(x0 + 9, "BOOST", 8);
  OLED::invertRect(x0, 0, x0 + 39, 9);
  drawTempBlock(getTipTemp(), 0);
  char text[16];
  if (dogLangDE()) {
    strcpy(text, "NOCH ");
    strcpy(dogFormat(text + 5, dogBoostSecondsLeft()), " S");
  } else {
    strcpy(dogFormat(text, dogBoostSecondsLeft()), " S LEFT");
  }
  drawUnderTemp(text, false);

  const TickType_t now  = xTaskGetTickCount();
  const uint8_t    beat = (now / (TICKS_SECOND / 20)) % 4; // the lines and the dust change 20 times a second
  const int16_t    dogX = x0 + 44;
  drawTrot(dogX, trotFrame(400), 30, 30);
  static const uint8_t LINE_ROW[3] = {20, 23, 26}, LINE_LENGTH[3] = {9, 6, 11};
  for (uint8_t i = 0; i < 3; i++) {
    const int16_t end = dogX - 3 - ((beat + i) % 2) * 2;
    OLED::drawFilledRect(end - LINE_LENGTH[i] + 1, LINE_ROW[i], end, LINE_ROW[i], false);
  }
  for (uint8_t i = 0; i < 3; i++) { // dust behind the hind paws, drifting back
    const int16_t dx = dogX - 1 - i * 3 - beat;
    OLED::drawFilledRect(dx, 29 - (i + beat) % 2, dx, 29 - (i + beat) % 2, false);
  }
  const uint8_t scroll = (now * 80 / TICKS_SECOND) % 6; // the ground passes at 80 px a second
  for (int16_t x = x0; x < x0 + STAGE_WIDTH; x++) {
    if ((x - x0 + scroll) % 6 < 3) {
      OLED::drawFilledRect(x, 31, x, 31, false);
    }
  }
}

/*
 * Soldering, once the set point has been reached: no dog
 *   Platine bleifrei     v 352°
 *   65W 3.2A               ZIEL 350°   (2 s after a change: "350° SAC305  o o * o o")
 *   ---- curve ----------  [ - ][ + ]
 */
struct DogSolder {
  const char *de, *en, *alloy;
};
static const DogSolder SOLDERS[] = {
    {"Empfindlich", "Sensitive", "SnBi"}, {"Platine mit Blei", "Leaded PCB", "SnPb"}, {"Platine bleifrei", "Lead-free PCB", "SAC305"},
    {"Dicke Pads", "Thick pads", "SnCu"},  {"Kabel + Masse", "Wires + mass", "Max"},
};
#define SOLDER_COUNT (sizeof(SOLDERS) / sizeof(SOLDERS[0]))

static void drawSolderingScreen() {
  const int16_t  x0     = stageX();
  const int8_t   solder = dogPresetIndex();
  const uint16_t tip = getTipTemp(), set = setPoint();
  dogText(dogFontLabel, x0, 10, solder < 0 ? DOG_T("Eigenes Ziel", "Own target") : (dogLangDE() ? SOLDERS[solder].de : SOLDERS[solder].en));
  char text[16];
  if (dogLotChangedAt && xTaskGetTickCount() - dogLotChangedAt < TICKS_SECOND * 2) {
    formatUnit(text, set, "° ");
    if (solder >= 0) {
      strcat(text, SOLDERS[solder].alloy);
    }
    dogSmall(x0, text, 19);
    int16_t dots = x0 + dogTextWidth(dogFontSmall, text) + 2;
    const uint8_t pitch = dots > x0 + 54 ? 4 : 5; // SAC305 is too wide for x=54: closer together, to stay clear of ZIEL
    dots                = dots < x0 + 54 ? x0 + 54 : dots;
    for (uint8_t i = 0; i < SOLDER_COUNT; i++) {
      const int16_t x = dots + i * pitch;
      if (i == solder) {
        OLED::drawFilledRect(x, 14, x + 2, 16, false);
      } else {
        OLED::drawFilledRect(x + 1, 15, x + 1, 15, false);
      }
    }
  } else {
    formatPower(text);
    dogSmall(x0, text, 19);
  }
  uint8_t rows[STAGE_WIDTH];
  dogDrawCurve(x0, STAGE_WIDTH, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, rows);
  const int16_t error   = (int16_t)tip - (int16_t)set;
  const bool    reached = error > -2 && error < 2;
  drawTempBlock(tip, reached ? 0 : (error < 0 ? 1 : -1));
  if (reached) {
    drawUnderTemp(DOG_T("BEREIT", "READY"), true);
  } else {
    formatTarget(text, set);
    drawUnderTemp(text, false);
  }
  dogKeyPair(normal() ? 96 : 0, nullptr, nullptr);
}

void ui_draw_soldering_basic_status(bool boostModeOn) {
  if (drawStopHold(getTipTemp())) {
    return;
  }
  if (boostModeOn) {
    drawBoostScreen();
  } else if (!dogSolderTimerStart || dogWokeHeating) {
    drawHeatingScreen();
  } else {
    drawSolderingScreen();
  }
}

/*
 * The detail view, a setting: the numbers on four rows next to the large temperature
 *   PD   20.1V      318°
 *   ~58W ~2.9A
 *   GRIFF  31°
 *   ZEIT  3:19
 */
void ui_draw_soldering_power_status(bool boostModeOn) {
  if (drawStopHold(getTipTemp())) {
    return;
  }
  if (boostModeOn) {
    drawBoostScreen();
    return;
  }
  const int16_t x0 = normal() ? 0 : OLED_WIDTH - 59, x1 = x0 + 58;
  char          text[10];
  dogSmall(x0, sourceName(), 7);
  formatX10(text, voltsX10(), 'V');
  dogSmallRight(x1, text, 7);
  text[0] = '~';
  formatUnit(text + 1, wattsNow(), "W");
  dogSmall(x0, text, 15);
  text[0] = '~';
  formatX10(text + 1, ampsX10(), 'A');
  dogSmallRight(x1, text, 15);
  dogSmall(x0, DOG_T("GRIFF", "GRIP"), 23);
  uint32_t handle = getHandleTemperature(0) / 10;
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    handle = handle * 9 / 5 + 32;
  }
  formatUnit(text, handle, "°");
  dogSmallRight(x1, text, 23);
  dogSmall(x0, DOG_T("ZEIT", "TIME"), 31);
  const uint32_t seconds = dogSolderTimerStart ? (xTaskGetTickCount() - dogSolderTimerStart) / TICKS_SECOND : 0;
  const uint32_t minutes = seconds / 60;
  dogFormatTime(text, minutes < 100 ? minutes : minutes / 60, minutes < 100 ? seconds % 60 : minutes % 60);
  dogSmallRight(x1, text, 31);
  drawBig(getTipTemp());
}

/*
 * Sleep: no buttons on this screen, so it stays the same way round when the iron is turned
 *   dog  z z z  SCHLAF         ^ 150°
 *               AUS IN 7 MIN
 *                   BEWEGEN WECKT
 * Against burning in, the whole content moves sideways every 4 s
 */
#define SLEEP_TEXT_X  44
#define SLEEP_BLOCK_X 90 // arrow at 90, digits from 97, as while soldering
#define ZZZ_LOOP      (TICKS_SECOND * 3)
#define ZZZ_RISE      (TICKS_100MS * 12)
#define ZZZ_STAGGER   (TICKS_100MS * 6)
#define ZZZ_END       (TICKS_100MS * 27) // the last 0.3 s of the loop are empty

static void drawZ(int16_t x, int16_t y, uint8_t size) {
  OLED::drawFilledRect(x, y, x + size - 1, y, false);
  OLED::drawFilledRect(x, y + size - 1, x + size - 1, y + size - 1, false);
  for (uint8_t i = 1; i < size - 1; i++) {
    OLED::drawFilledRect(x + size - 1 - i, y + i, x + size - 1 - i, y + i, false);
  }
}

// Three of them rise one after the other, each from 4 px below where it stays
static void drawZzz(int16_t dx) {
  static const uint8_t ZX[3] = {17, 23, 30}, ZY[3] = {7, 4, 0};
  const TickType_t     t     = xTaskGetTickCount() % ZZZ_LOOP;
  if (t >= ZZZ_END) {
    return;
  }
  for (uint8_t i = 0; i < 3; i++) {
    const TickType_t start = i * ZZZ_STAGGER;
    if (t >= start) {
      const TickType_t age  = t - start;
      const uint8_t    rise = age >= ZZZ_RISE ? 4 : age * 4 / ZZZ_RISE;
      drawZ(dx + ZX[i], ZY[i] + 4 - rise, 3 + i);
    }
  }
}

static void drawSleepScreen(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  if (drawStopHold(tipTemp)) {
    OLED::refresh();
    return;
  }
  static const int8_t SHIFT[4] = {0, -1, -2, -1};
  const int16_t       dx       = SHIFT[(xTaskGetTickCount() / (TICKS_SECOND * 4)) % 4];
  dogDrawSleepingDog(2 + dx, dogSleepWakeFrame);
  if (!dogSleepWakeFrame) {
    drawZzz(dx);
  }
  dogSmall(SLEEP_TEXT_X + dx, DOG_T("SCHLAF", "SLEEP"), 8);
  const uint32_t shutdown = getSettingValue(SettingsOptions::ShutdownTime);
  if (shutdown) {
    const TickType_t last    = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
    const TickType_t elapsed = xTaskGetTickCount() - last;
    const TickType_t limit   = shutdown * 60 * TICKS_SECOND;
    const uint32_t   left    = elapsed < limit ? (limit - elapsed + 60 * TICKS_SECOND - 1) / (60 * TICKS_SECOND) : 0; // minutes, rounded up
    char             text[20];
    strcpy(text, DOG_T("AUS IN ", "OFF IN "));
    formatUnit(text + 7, left, " MIN");
    dogSmall(SLEEP_TEXT_X + dx, text, 18);
  }
  // The temperature held, with the arrow while it is still on its way there. Hibernating there is no target: no arrow
  const uint16_t sleepSet = getSettingValue(SettingsOptions::SleepTemp), solderSet = setPoint();
  const int16_t  held     = sleepSet < solderSet ? sleepSet : solderSet;
  const int16_t  error    = (int16_t)tipTemp - held;
  const bool     target   = currentTempTargetDegC != 0;
  drawTempBlockAt(SLEEP_BLOCK_X + dx, tipTemp, !target || (error > -2 && error < 2) ? 0 : (error < 0 ? 1 : -1));
  dogSmallRight(OLED_WIDTH - 1 + dx, DOG_T("BEWEGEN WECKT", "MOVE TO WAKE"), 32);
  OLED::refresh();
}

void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }
void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }

/*
 * Home: the screen itself is in ts101_dog_home.cpp, alerts replace it
 */
static const char *alertFooter(char *text) {
  switch (dogAlert) {
  case DogAlert::Handle: {
    char *end = dogFormat(text, getHandleTemperature(0) / 10);
    strcpy(end, DOG_T("°: HEIZUNG AUS", "°: HEATER OFF"));
    return text;
  }
  default:
    return DOG_T("AUS: SPITZE PRÜFEN", "OFF: CHECK THE TIP");
  }
}

void ui_draw_homescreen_simplified(TemperatureType_t tipTemp) {
  if (isTipDisconnected()) {
    dogDrawNotice(iconWarning, DOG_T("SPITZE!", "NO TIP!"), DOG_T("KEINE SPITZE ERKANNT", "CONNECT A TIP"));
    return;
  }
  if (dogAlert != DogAlert::None) {
    char text[20];
    dogDrawNotice(iconWarning, dogAlert == DogAlert::Handle ? DOG_T("GRIFF!", "GRIP!") : DOG_T("HEIZUNG!", "HEATER!"), alertFooter(text));
    return;
  }
  dogDrawHome(tipTemp);
}

/*
 * The readings at the end of the menu, one label and one value each
 */
const char *dogInfoRow(uint8_t row, char *value) {
  value[0] = 0;
  switch (row) {
  case 0: {
    const int8_t source = getPowerSourceNumber();
    formatX10(value, voltsX10(), 'V');
    return source == 0 ? "DC" : (source == 1 ? "USB" : "USB-PD");
  }
  case 1: {
    const int32_t limit = getX10WattageLimits();
    formatUnit(value, limit > 0 ? limit / 10 : 0, " W");
    return DOG_T("GRENZE", "LIMIT");
  }
  case 2:
    strcpy(value, dogTipName());
    return DOG_T("TYP", "TYPE");
  case 3: {
    char *end = dogFormat(value, getTipResistanceX10() / 10);
    *end++    = '.';
    *end++    = '0' + getTipResistanceX10() % 10;
    strcpy(end, " OHM");
    return DOG_T("WIDERSTAND", "RESISTANCE");
  }
  case 4: {
    const uint32_t handle = getHandleTemperature(0);
    char          *end    = dogFormat(value, handle / 10);
    *end++                = '.';
    *end++                = '0' + handle % 10;
    strcpy(end, "°");
    return DOG_T("GRIFF", "GRIP");
  }
  case 5:
    strcpy(value, normal() ? DOG_T("RECHTS", "RIGHT") : DOG_T("LINKS", "LEFT"));
    return DOG_T("LAGE", "SIDE");
  case 6: {
    const uint16_t minutes = getSettingValue(SettingsOptions::TotalSolderMinutes);
    strcpy(dogFormatTime(value, minutes / 60, minutes % 60), " H");
    return DOG_T("GESAMT", "TOTAL");
  }
  case 7:
    dogFormatTime(value, dogLastSessionSeconds / 60, dogLastSessionSeconds % 60);
    return DOG_T("LETZTE", "LAST");
  case 8:
    strcpy(value, BUILD_VERSION + 1); // without the v
    return "VERSION";
  default:
    return "DEBUG";
  }
}
#endif

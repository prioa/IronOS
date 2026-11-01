/*
 * TS101_DOG_UI: soldering, sleep and home screens, after the "Gegenentwürfe" boards.
 * The large temperature sits on the front button's side and the layout is mirrored with the orientation.
 */
#include "ts101_dog_ui.hpp"
#ifdef TS101_DOG_UI
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "TipThermoModel.h"
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

// "-" on the rear button's side and "+" on the front button's, on the row of the power bar
static void drawMinusPlus() {
  const bool    frontRight = dogFrontOnRight();
  const int16_t minusX = frontRight ? 0 : OLED_WIDTH - 5, plusX = frontRight ? OLED_WIDTH - 5 : 0;
  OLED::drawFilledRect(minusX, 27, minusX + 4, 27, false);
  OLED::drawFilledRect(plusX, 27, plusX + 4, 27, false);
  OLED::drawFilledRect(plusX + 2, 25, plusX + 2, 29, false);
  if (dogPressed(true)) {
    OLED::invertRect(frontRight ? OLED_WIDTH - 6 : 0, 23, frontRight ? OLED_WIDTH - 1 : 5, 31);
  }
  if (dogPressed(false)) {
    OLED::invertRect(frontRight ? 0 : OLED_WIDTH - 6, 23, frontRight ? 5 : OLED_WIDTH - 1, 31);
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
static void drawTempBlock(uint16_t value, int8_t arrow) {
  const int16_t x = blockX();
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

// The dog 24x13, its columns shifted down by rowAt(column): it leans with the ground under it
static void drawTrot(int16_t x, const uint8_t *frame, int16_t yBack, int16_t yFront) {
  for (uint8_t col = 0; col < 24; col++) {
    // the paws are on columns 3 and 16; between and beyond them the line through both
    const int16_t num    = (yFront - yBack) * (col - 3);
    const int16_t ground = yBack + (num + (num < 0 ? -6 : 6)) / 13;
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
  // The dog runs at the newest end of the curve, uphill; at the start it waits at the left end
  const int16_t front = count > 17 ? count - 1 : 16; // column of the front paw, in samples
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
  dogText(dogFontLabel, x0, 10, solder < 0 ? DOG_T("Manuell", "Manual") : (dogLangDE() ? SOLDERS[solder].de : SOLDERS[solder].en));
  char text[16];
  if (dogLotChangedAt && xTaskGetTickCount() - dogLotChangedAt < TICKS_SECOND * 2) {
    formatUnit(text, set, "° ");
    if (solder >= 0) {
      strcat(text, SOLDERS[solder].alloy);
    }
    dogSmall(x0, text, 19);
    int16_t dots = x0 + dogTextWidth(dogFontSmall, text) + 2;
    dots         = dots < x0 + 54 ? x0 + 54 : dots;
    for (uint8_t i = 0; i < SOLDER_COUNT; i++) {
      const int16_t x = dots + i * 5;
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
  } else if (!dogSolderTimerStart) {
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
 * Temperature view, the same while soldering and on the home screen; the scale runs from 100 to 400 °C
 *   Soll 350°  BLEIFREI
 *              SAC305
 *   -  ----|----|----|--v-|  +
 */
static const char *presetUse(uint8_t line) {
  static const char *const de[][2] = {{"NT-LOT", "SN42BI58"}, {"BLEILOT", "SN63PB37"}, {"BLEIFREI", "SAC305"}, {"BLEIFREI", "SNCU0.7"}, {"KABEL +", "MASSE"}};
  static const char *const en[][2] = {{"LOW TEMP", "SN42BI58"}, {"LEADED", "SN63PB37"}, {"UNLEADED", "SAC305"}, {"UNLEADED", "SNCU0.7"}, {"WIRES +", "MASS"}};
  const int8_t             preset  = dogPresetIndex();
  return preset < 0 ? nullptr : (dogLangDE() ? de : en)[preset][line];
}

void dogDrawTemperatureView(bool) {
  const bool     inF  = getSettingValue(SettingsOptions::TemperatureInF);
  const uint16_t set  = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t setC = inF ? TipThermoModel::convertFtoC(set) : set;
  dogSmall(0, DOG_T("Soll", "Set"), 10);
  dogBig(25, set);
  const int16_t xMin = 80, xMax = OLED_WIDTH - 1;
  const char   *use0 = presetUse(0);
  if (use0) {
    dogSmall(xMin, use0, 9);
    dogSmall(xMin, presetUse(1), 18);
  } else {
    char text[12];
    strcpy(text, DOG_T("IST ", "TIP "));
    formatUnit(text + strlen(text), getTipTemp(), "°");
    dogSmallRight(xMax, text, 15);
  }
  if (dogDrawHold(DOG_T("HALTEN: PRESET", "HOLD: PRESET"), true, DOG_HOLD_FULL) || dogDrawHold(DOG_T("HALTEN: FERTIG", "HOLD: DONE"), false, DOG_HOLD_FULL)) {
    return;
  }
  drawMinusPlus();
  // Low on the rear button's side, like the minus
  const bool frontRight = dogFrontOnRight();
  auto       scaleX     = [&](int32_t c) -> int16_t {
    c                 = c < 100 ? 100 : (c > 400 ? 400 : c);
    const int16_t x   = 8 + (c - 100) * 111 / 300;
    return frontRight ? x : OLED_WIDTH - 1 - x;
  };
  OLED::drawFilledRect(frontRight ? 8 : OLED_WIDTH - 120, 27, frontRight ? 119 : OLED_WIDTH - 9, 27, false);
  for (uint16_t t = 100; t <= 400; t += 100) {
    OLED::drawFilledRect(scaleX(t), 25, scaleX(t), 29, false);
  }
  const int16_t c = scaleX(setC);
  OLED::drawFilledRect(c, 24, c, 29, false);
  OLED::drawFilledRect(c - 1, 23, c + 1, 23, false);
  OLED::drawFilledRect(c - 2, 22, c + 2, 22, false);
  OLED::drawFilledRect(c - 3, 21, c + 3, 21, false);
}

/*
 * Sleep: the dog lies there awake for a moment, closes its eyes and "z Z Z" drifts up from its head;
 * on the other side the tip temperature and how long until the heater goes off
 */
static void drawSleepingDog() {
  const TickType_t inSleep = xTaskGetTickCount() - dogSleepEnterTime;
  const int16_t    x       = normal() ? 0 : OLED_WIDTH - DOG_LYING_WIDTH;
  if (inSleep < TICKS_100MS * 8) {
    OLED::drawArea(x, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLyingAwake);
  } else {
    OLED::drawArea(x, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLying);
    uint8_t step = (xTaskGetTickCount() / (TICKS_100MS * 6)) % DOG_ZZZ_STEPS;
    OLED::drawArea(x + 50, 0, DOG_ZZZ_WIDTH, 8, dogZzz[step]);
  }
}

static void drawSleepScreen(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  if (drawStopHold(tipTemp)) {
    OLED::refresh();
    return;
  }
  drawSleepingDog();
  drawBig(tipTemp);
  const uint32_t shutdown = getSettingValue(SettingsOptions::ShutdownTime);
  if (shutdown) {
    const TickType_t last    = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
    const TickType_t elapsed = xTaskGetTickCount() - last;
    const TickType_t limit   = shutdown * 60 * TICKS_SECOND;
    const uint32_t   left    = elapsed < limit ? (limit - elapsed) / TICKS_SECOND : 0;
    char             text[10];
    dogFormatTime(text, left / 60, left % 60);
    dogSmall(normal() ? 79 : 0, DOG_T("AUS", "OFF"), 30);
    dogSmallRight(normal() ? BIG_RIGHT - 1 : 47, text, 30);
  }
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
 * The info pages: a heading with the page number, two lines of label and value, and the key row
 */
void dogDrawInfoPage(uint8_t page) {
  static const char *const titles[DOG_INFO_PAGES][2] = {{"STROM", "POWER"}, {"SPITZE", "TIP"}, {"GRIFF + LAGE", "GRIP + SIDE"}, {"STATISTIK", "STATISTICS"}, {"SYSTEM", "SYSTEM"}};
  char                     left[14], right[14];
  const char              *label1 = "", *label2 = "";
  const char              *value1 = right, *value2 = left;
  switch (page) {
  case 0: {
    const int8_t source = getPowerSourceNumber();
    label1              = source == 0 ? "DC" : (source == 1 ? "USB" : "USB-PD");
    label2              = DOG_T("GRENZE", "LIMIT");
    formatX10(right, getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0), 'V');
    const int32_t limit = getX10WattageLimits();
    formatUnit(left, limit > 0 ? limit / 10 : 0, "W");
    break;
  }
  case 1: {
    label1 = DOG_T("TYP", "TYPE");
    label2 = DOG_T("WIDERSTAND", "RESISTANCE");
    value1 = dogTipName();
    char *end = dogFormat(left, getTipResistanceX10() / 10);
    *end++    = '.';
    *end++    = '0' + getTipResistanceX10() % 10;
    strcpy(end, " OHM");
    break;
  }
  case 2: {
    label1 = DOG_T("GRIFF", "GRIP");
    label2 = DOG_T("LAGE", "SIDE");
    const uint32_t handle = getHandleTemperature(0);
    char *end = dogFormat(right, handle / 10);
    *end++    = '.';
    *end++    = '0' + handle % 10;
    strcpy(end, "°");
    value2 = OLED::getRotation() ? DOG_T("RECHTS", "RIGHT") : DOG_T("LINKS", "LEFT");
    break;
  }
  case 3: {
    label1 = DOG_T("GESAMT", "TOTAL");
    label2 = DOG_T("LETZTE", "LAST");
    const uint16_t minutes = getSettingValue(SettingsOptions::TotalSolderMinutes);
    char *end = dogFormatTime(right, minutes / 60, minutes % 60);
    strcpy(end, " H");
    dogFormatTime(left, dogLastSessionSeconds / 60, dogLastSessionSeconds % 60);
    break;
  }
  default:
    label1 = "VERSION";
    label2 = "DEBUG";
    value1 = BUILD_VERSION + 1; // without the v
    value2 = "";
    break;
  }
  dogSmall(0, titles[page][dogLangDE() ? 0 : 1], 7);
  char number[6];
  char *end = dogFormat(number, page + 1);
  *end++    = '/';
  dogFormat(end, DOG_INFO_PAGES);
  dogSmallRight(OLED_WIDTH - 1, number, 7);
  dogSmall(0, label1, 15);
  dogSmallRight(OLED_WIDTH - 1, value1, 15);
  dogSmall(0, label2, 23);
  dogSmallRight(OLED_WIDTH - 1, value2, 23);
  if (dogDrawHold(DOG_T("HALTEN: ZURÜCK", "HOLD: BACK"), false, DOG_HOLD_FULL) || (page == DOG_INFO_PAGES - 1 && dogDrawHold(DOG_T("HALTEN: DEBUG", "HOLD: DEBUG"), true, DOG_HOLD_FULL))) {
    return;
  }
  dogDrawKeys({iconNext, DOG_T("WEITER", "NEXT")}, {iconBack, DOG_T("VORIGE", "PREV")}, nullptr);
}
#endif

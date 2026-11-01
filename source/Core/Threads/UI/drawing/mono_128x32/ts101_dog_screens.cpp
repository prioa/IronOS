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
static const int16_t MIRROR_STAGE = 46;        // mirrored, what is left of the large temperature starts here
static const uint8_t LABEL_ADVANCE = 8;
static const uint8_t LABEL_BASE = 10;          // the top line in the label font, rows 0-9
static const uint8_t KEY_LABEL_BASE = 31;      // the key words at the bottom, rows 21-30
static const uint8_t CURVE_WIDTH = 78;
static const uint8_t CURVE_TOP = 13, CURVE_BOTTOM = 20; // the set point is the top row
static const uint8_t POWER_TEXT_BASE = 30;

static inline bool normal() { return OLED::getRotation(); }
static int16_t     stageX() { return normal() ? 0 : MIRROR_STAGE; }

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

// Volts and watts of the supply: "20V·65W"
static void formatSupply(char *out, bool withWatts) {
  out = dogFormat(out, (voltsX10() + 5) / 10);
  *out++ = 'V';
  if (withWatts) {
    const int32_t limit = getX10WattageLimits();
    strcpy(out, "·");
    out = dogFormat(out + strlen(out), limit > 0 ? limit / 10 : 0);
    *out++ = 'W';
  }
  *out = 0;
}

static void drawBig(uint16_t value) {
  if (normal()) {
    dogBigRight(BIG_RIGHT, value);
  } else {
    dogBig(-1, value); // the digits have an empty first column
  }
}

static void drawArrow(int16_t x, uint8_t y, uint8_t length) {
  OLED::drawFilledRect(x, y, x + length - 1, y, false);
  OLED::drawFilledRect(x + length - 2, y - 1, x + length - 2, y + 1, false);
  OLED::drawFilledRect(x + length - 3, y - 2, x + length - 3, y - 2, false);
  OLED::drawFilledRect(x + length - 3, y + 2, x + length - 3, y + 2, false);
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

// The power bar with the watts next to it
static void drawPowerRow() {
  const bool    frontRight = dogFrontOnRight();
  const int16_t x0 = frontRight ? 8 : 36, x1 = x0 + 83;
  OLED::drawFilledRect(x0, 23, x1, 23, false);
  OLED::drawFilledRect(x0, 30, x1, 30, false);
  OLED::drawFilledRect(x0, 23, x0, 30, false);
  OLED::drawFilledRect(x1, 23, x1, 30, false);
  const int32_t limit = getX10WattageLimits();
  uint32_t      fill  = limit > 0 ? (x10WattHistory.average() * 80) / limit : 0;
  if (fill > 80) {
    fill = 80;
  }
  if (fill) {
    OLED::drawFilledRect(x0 + 2, 25, x0 + 1 + fill, 28, false);
  }
  char text[8];
  formatUnit(text, wattsNow(), "W");
  if (frontRight) {
    dogSmallRight(115, text, POWER_TEXT_BASE);
  } else {
    dogSmall(10, text, POWER_TEXT_BASE);
  }
  drawMinusPlus();
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

// Boost: what it heads for and how long it may still last; the dog stays out of it
static void drawBoostScreen() {
  const int16_t x0 = stageX();
  label(x0 + 4, 11, "BOOST");
  OLED::invertRect(x0, 0, x0 + 45, 11);
  drawArrow(x0, 17, 6);
  char  text[12];
  char *end = dogFormat(text, getSettingValue(SettingsOptions::BoostTemp));
  strcpy(end, "°");
  dogSmall(x0 + 8, text, 21);
  dogFormat(text, dogBoostSecondsLeft());
  strcat(text, "s");
  dogSmall(x0 + 44, text, 21);
  drawBig(getTipTemp());
  drawPowerRow();
}

// A weak supply slows heating up, and says so in the bottom row
static bool heatingLimited() {
  const int32_t limit = getX10WattageLimits();
  return limit > 0 && limit <= DOG_WEAK_SUPPLY_X10W;
}

static void drawHeatingScreen() {
  if (heatingLimited()) {
    const int16_t x0 = stageX();
    drawArrow(x0, 5, 7);
    char text[12];
    formatUnit(text, getSettingValue(SettingsOptions::SolderingTemp), "°");
    label(x0 + 9, LABEL_BASE, text);
    dogDrawCurve(x0, x0 + 73, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, DogCurve::Heat, true);
    drawBig(getTipTemp());
    label(5, KEY_LABEL_BASE, DOG_T("Netzteil", "Supply"));
    formatSupply(text, true);
    const uint8_t chars = strlen(text) - 1; // the dot takes two bytes
    label(125 - chars * LABEL_ADVANCE, KEY_LABEL_BASE, text);
    OLED::invertRect(0, 21, OLED_WIDTH - 1, OLED_HEIGHT - 1);
    return;
  }
  dogDrawRunning(normal() ? 0 : OLED_WIDTH - DOG_RUN_WIDTH, 0);
  drawBig(getTipTemp());
  drawPowerRow();
}

// Seconds until sleep, 0 unless they are about to run out
static uint32_t sleepCountdown() {
#ifndef NO_SLEEP_MODE
  if (getSettingValue(SettingsOptions::Sensitivity) && getSettingValue(SettingsOptions::SleepTime)) {
    const TickType_t last    = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
    const TickType_t elapsed = xTaskGetTickCount() - last;
    const TickType_t timeout = getSleepTimeout();
    if (elapsed < timeout && timeout - elapsed <= TICKS_SECOND * 10) {
      return (timeout - elapsed) / TICKS_SECOND + 1;
    }
  }
#endif
  return 0;
}

// The top line at temperature: the check mark while nothing draws power, the set point under load, the countdown before sleep
static void drawTopLine(int16_t x0) {
  static bool    underLoad = false;
  const int32_t  limit     = getX10WattageLimits();
  const uint32_t x10W      = x10WattHistory.average();
  underLoad                = limit > 0 && x10W * 100 > (uint32_t)limit * (underLoad ? 30 : 40); // no flicker round the threshold
  const uint32_t countdown = sleepCountdown();
  char           text[16];
  if (countdown) {
    strcpy(text, DOG_T("SCHLAF IN ", "SLEEP IN "));
    dogFormat(text + strlen(text), countdown);
    dogSmall(x0, text, 9);
  } else if (underLoad) {
    strcpy(text, DOG_T("Soll ", "Set "));
    dogFormat(text + strlen(text), getSettingValue(SettingsOptions::SolderingTemp));
    label(x0 + 1, LABEL_BASE, text);
  } else {
    dogBlit(x0, 3, 8, 8, iconReady);
    label(x0 + 12, LABEL_BASE, DOG_T("Bereit", "Ready"));
  }
}

/*
 * Soldering at temperature:
 *   [check] Bereit           350°
 *   ------------------ set line
 *   -  [=====         ]  9W    +
 */
void ui_draw_soldering_basic_status(bool boostModeOn) {
  if (drawStopHold(getTipTemp())) {
    return;
  }
  if (boostModeOn) {
    drawBoostScreen();
    return;
  }
  if (!dogSolderTimerStart) {
    drawHeatingScreen();
    return;
  }
  const int16_t x0 = stageX();
  drawTopLine(x0);
  dogDrawCurve(x0, x0 + CURVE_WIDTH - 1, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, DogCurve::Steady, true);
  drawBig(getTipTemp());
  drawPowerRow();
  if (xTaskGetTickCount() < dogReachedUntil) {
    OLED::invertRect(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1); // the moment the set point is reached
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
static const char *presetUse(uint16_t setC, uint8_t line) {
  static const uint16_t temps[] = TS101_PRESETS_C;
  for (uint8_t i = 0; i < sizeof(temps) / sizeof(temps[0]); i++) {
    if (temps[i] == setC) {
      static const char *const de[][2] = {{"SMD +", "BLEILOT"}, {"BLEIFREI", "SAC305"}, {"KABEL +", "MASSE"}};
      static const char *const en[][2] = {{"SMD +", "LEADED"}, {"UNLEADED", "SAC305"}, {"WIRES +", "MASS"}};
      return (dogLangDE() ? de : en)[i][line];
    }
  }
  return nullptr;
}

void dogDrawTemperatureView(bool heating) {
  const bool     inF  = getSettingValue(SettingsOptions::TemperatureInF);
  const uint16_t set  = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t setC = inF ? TipThermoModel::convertFtoC(set) : set;
  dogSmall(0, DOG_T("Soll", "Set"), 10);
  dogBig(25, set);
  const int16_t xMin = 80, xMax = OLED_WIDTH - 1;
  const char   *use0 = presetUse(setC, 0);
  if (use0) {
    dogSmall(xMin, use0, 9);
    dogSmall(xMin, presetUse(setC, 1), 18);
  } else if (heating) {
    dogDrawCurve(xMin, xMax, 1, 20, setC, DogCurve::Steady, true);
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
 * Home, the iron is not heating: what it would heat to and with what, and what each button does
 *   Bleifrei              350°
 *   PD 20V·65W
 *   = Menü           Löten  >
 * The variant has the dog head on the left and runs and barks when picked up. Alerts replace everything else.
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

static void drawHeadClipped(int16_t x, const uint8_t *sprite) {
  dogBlit(x, 0, DOG_HEAD_WIDTH, 24, sprite);
  OLED::drawFilledRect(x, 20, x + DOG_HEAD_WIDTH - 1, 23, true);
}

// The preset's name, or "Soll" for a set point of your own
static const char *presetLabel(uint16_t setC, bool upper) {
  static const uint16_t temps[] = TS101_PRESETS_C;
  static const char *const de[] = {"Bleilot", "Bleifrei", "Masse"}, *const en[] = {"Leaded", "Unleaded", "Mass"};
  static const char *const deUp[] = {"BLEILOT", "BLEIFREI", "MASSE"}, *const enUp[] = {"LEADED", "UNLEADED", "MASS"};
  for (uint8_t i = 0; i < sizeof(temps) / sizeof(temps[0]); i++) {
    if (temps[i] == setC) {
      return upper ? (dogLangDE() ? deUp : enUp)[i] : (dogLangDE() ? de : en)[i];
    }
  }
  return upper ? DOG_T("SOLL", "SET") : DOG_T("Soll", "Set");
}

static void drawHomeKeys() {
  if (dogDrawHold(DOG_T("HALTEN: TEMPERATUR", "HOLD: TEMPERATURE"), true, DOG_HOLD_FULL) || dogDrawHold(DOG_T("HALTEN: INFO", "HOLD: INFO"), false, DOG_HOLD_FULL)) {
    return;
  }
  const bool    frontRight = dogFrontOnRight();
  const int16_t barsX      = frontRight ? 0 : OLED_WIDTH - 6;
  for (uint8_t y = 26; y <= 30; y += 2) {
    OLED::drawFilledRect(barsX, y, barsX + 5, y, false);
  }
  label(frontRight ? 9 : 86, KEY_LABEL_BASE, DOG_T("Menü", "Menu"));
  // The arrow points at the button
  label(frontRight ? 73 : 7, KEY_LABEL_BASE, DOG_T("Löten", "Solder"));
  for (uint8_t i = 0; i < 4; i++) {
    const int16_t x = frontRight ? OLED_WIDTH - 4 + i : 3 - i;
    OLED::drawFilledRect(x, 24 + i, x, 30 - i, false);
  }
  if (dogPressed(true)) {
    OLED::invertRect(frontRight ? 71 : 0, 20, frontRight ? OLED_WIDTH - 1 : 56, OLED_HEIGHT - 1);
  }
  if (dogPressed(false)) {
    OLED::invertRect(frontRight ? 0 : 84, 20, frontRight ? 41 : OLED_WIDTH - 1, OLED_HEIGHT - 1);
  }
}

void ui_draw_homescreen_simplified(TemperatureType_t tipTemp) {
  static TickType_t lastSeenMovement = 0;
  static TickType_t pickedUpAt       = 0;
  static bool       tempOnDisplay    = false;
  const TickType_t  now              = xTaskGetTickCount();

  if (isTipDisconnected()) {
    dogDrawNotice(iconWarning, DOG_T("SPITZE!", "NO TIP!"), DOG_T("KEINE SPITZE ERKANNT", "CONNECT A TIP"));
    return;
  }
  if (dogAlert != DogAlert::None) {
    char text[20];
    dogDrawNotice(iconWarning, dogAlert == DogAlert::Handle ? DOG_T("GRIFF!", "GRIP!") : DOG_T("HEIZUNG!", "HEATER!"), alertFooter(text));
    return;
  }
  if (lastMovementTime != lastSeenMovement) {
    if (lastMovementTime - lastSeenMovement > TICKS_SECOND * 5) {
      pickedUpAt = lastMovementTime; // picked up after lying still
    }
    lastSeenMovement = lastMovementTime;
  }
  if (tipTemp > 55) {
    tempOnDisplay = true;
  } else if (tipTemp < 45) {
    tempOnDisplay = false;
  }

  const uint16_t set  = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t setC = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::convertFtoC(set) : set;
  if (tempOnDisplay) {
    // Still hot: the triangle, the cooling curve down to the dotted 50 °C, and the tip instead of the set point
    const int16_t x0 = stageX();
    dogBlit(x0, 1, 7, 8, iconHot);
    label(x0 + 11, LABEL_BASE, DOG_T("Heiß", "Hot"));
    dogDrawCurve(x0, x0 + 71, 12, 19, 50, DogCurve::Cool, true);
    if (!(getSettingValue(SettingsOptions::CoolingTempBlink) && (now % 1000 < 300))) {
      drawBig(tipTemp);
    }
  } else if (getSettingValue(SettingsOptions::DogHomeVariant)) {
    // Dog: runs across for a second, then the head barks for a second and calms down
    const TickType_t sincePickup = now - pickedUpAt;
    if (pickedUpAt && sincePickup < TICKS_SECOND) {
      dogDrawRunAcross();
      OLED::drawFilledRect(0, 20, OLED_WIDTH - 1, 23, true);
    } else {
      const bool bark = pickedUpAt && sincePickup < TICKS_SECOND * 2 && ((now / (TICKS_100MS * 3)) % 2);
      const int16_t x = normal() ? 0 : OLED_WIDTH - DOG_HEAD_WIDTH;
      drawHeadClipped(x, bark ? dogHeadBark : dogHead);
      const int16_t textX = normal() ? 35 : 42;
      dogSmall(textX, presetLabel(setC, true), 10);
      char supply[12];
      strcpy(supply, sourceName());
      strcat(supply, " ");
      formatSupply(supply + strlen(supply), false);
      dogSmall(textX, supply, 18);
      drawBig(set);
    }
  } else {
    label(normal() ? 1 : 64, LABEL_BASE, presetLabel(setC, false));
    char supply[16];
    strcpy(supply, sourceName());
    strcat(supply, " ");
    formatSupply(supply + strlen(supply), true);
    if (heatingLimited()) {
      strcat(supply, "!");
    }
    dogSmall(normal() ? 0 : 67, supply, 19);
    drawBig(set);
  }
  drawHomeKeys();
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

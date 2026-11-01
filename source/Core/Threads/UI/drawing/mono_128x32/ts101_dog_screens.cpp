/*
 * TS101_DOG_UI: soldering, sleep and home screens with the German Shepherd.
 * They replace the dense 128x32 versions; the two halves swap over with the screen orientation,
 * like the stock layouts, so the temperature stays on the same side as there.
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

// Rows 0-22 are the stage, row 23 holds the umlaut dots of the key row, rows 24-31 are the key row. On every screen
// here the front button is on the side getRotation() says, and the layout is mirrored with it.
static const uint8_t  CURVE_TOP = 1, CURVE_BOTTOM = 21;
static const int16_t  TEMP_RIGHT = 116, TEMP_LEFT = 10;              // the large temperature, next to the flame
static const uint8_t  TEMP_WIDTH = 42;                               // three typical digits with the ring; fixed, or the curve would jump with the value
static const uint8_t  METRICS_WIDTH = 44, SMALL_CURVE = 27, BIG_CURVE = 72;
static const int16_t  STATUS_X0 = 10, STATUS_X1 = 117;               // the status between the two key icons

static uint8_t digitCount(uint32_t v) {
  uint8_t n = 1;
  while (v >= 10) {
    v /= 10;
    n++;
  }
  return n;
}

static uint32_t ampsX10() {
  uint32_t voltX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  return voltX10 ? (x10WattHistory.average() * 10 + voltX10 / 2) / voltX10 : 0;
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

static void drawTipBig() {
  const uint16_t tip = getTipTemp();
  if (OLED::getRotation()) {
    dogBigRight(TEMP_RIGHT, tip);
  } else {
    dogBig(TEMP_LEFT, tip);
  }
  dogDrawFlame(dogFrontKeyX());
}

// Below the stage: the stop bar while the rear button is held, otherwise the icons
static bool drawSolderKeys() {
  if (dogDrawHold(DOG_T("HALTEN: STOPP", "HOLD: STOP"), false, DOG_HOLD_STOP)) {
    return false;
  }
  dogDrawCorners(iconUpDown, iconInfo);
  return true;
}

// Boost: the dog runs across the whole screen, below it the small curve, the tip and the boost target
static void drawBoostScreen() {
  dogDrawRunAcross();
  OLED::drawFilledRect(0, 23, OLED_WIDTH - 1, 23, true);
  dogDrawFlame(dogFrontKeyX());
  const bool flip = OLED::getRotation();
  dogDrawCurve(flip ? 0 : OLED_WIDTH - 53, flip ? 52 : OLED_WIDTH - 1, 24, 31, currentTempTargetDegC, DogCurve::Boost, false);
  char  text[16];
  char *end = dogFormat(text, getTipTemp());
  strcpy(end, "° / ");
  end = dogFormat(end + strlen(end), getSettingValue(SettingsOptions::BoostTemp));
  strcpy(end, "°");
  if (flip) {
    dogSmallRight(OLED_WIDTH - 1, text);
  } else {
    dogSmall(0, text);
  }
}

// Heating up: the running dog, the tip, the curve growing in the bottom row; with a weak supply the row says why it is slow
static void drawHeatingScreen() {
  const bool flip = OLED::getRotation();
  dogDrawRunning(flip ? 0 : OLED_WIDTH - DOG_RUN_WIDTH, 0);
  OLED::drawFilledRect(0, 23, OLED_WIDTH - 1, 23, true);
  drawTipBig();
  if (!drawSolderKeys()) {
    return;
  }
  char           text[12];
  const int32_t  limit   = getX10WattageLimits();
  const bool     limited = limit > 0 && limit <= DOG_WEAK_SUPPLY_X10W;
  if (limited) {
    strcpy(text, DOG_T("NUR ", "MAX "));
    formatUnit(text + strlen(text), limit / 10, "W");
  } else {
    formatUnit(text, getSettingValue(SettingsOptions::SolderingTemp), "°");
  }
  dogDrawCurve(STATUS_X0, limited ? 60 : 88, 24, 31, currentTempTargetDegC, DogCurve::Heat, false);
  dogSmallRight(STATUS_X1, text);
}

static void drawSessionTime() {
  uint32_t seconds = dogSolderTimerStart ? (xTaskGetTickCount() - dogSolderTimerStart) / TICKS_SECOND : 0;
  uint32_t minutes = seconds / 60;
  const uint16_t major = minutes < 100 ? minutes : minutes / 60;
  const uint8_t  minor = minutes < 100 ? seconds % 60 : minutes % 60;
  char  text[9];
  char *end = dogFormat(text, major);
  *end++    = ':';
  *end++    = '0' + minor / 10;
  *end++    = '0' + minor % 10;
  *end      = 0;
  const uint8_t w = 9 + dogTextWidth(dogFontSmall, text);
  const int16_t x = STATUS_X0 + (STATUS_X1 - STATUS_X0 + 1 - w) / 2;
  OLED::drawArea(x, 24, 8, 8, dogClock);
  dogSmall(x + 9, text);
}

static void drawLoadRow() {
  char text[8];
  formatUnit(text, wattsNow(), "W");
  dogSmall(STATUS_X0, text);
  const int16_t x     = STATUS_X0 + dogTextWidth(dogFontSmall, text) + 3;
  const int16_t right = STATUS_X1 - 1;
  OLED::drawFilledRect(x, 25, right, 25, false);
  OLED::drawFilledRect(x, 30, right, 30, false);
  OLED::drawFilledRect(x, 25, x, 30, false);
  OLED::drawFilledRect(right, 25, right, 30, false);
  const int32_t limit = getX10WattageLimits();
  uint32_t      fill  = limit > 0 ? (x10WattHistory.average() * (right - x - 4)) / limit : 0;
  if (fill > (uint32_t)(right - x - 4)) {
    fill = right - x - 4;
  }
  OLED::drawFilledRect(x + 2, 27, x + 2 + fill, 28, false);
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

// The status between the icons, picked from what matters right now
static void drawSolderStatus() {
  static bool underLoad = false;
  const int32_t  limit  = getX10WattageLimits();
  const uint32_t x10W   = x10WattHistory.average();
  underLoad             = limit > 0 && x10W * 100 > (uint32_t)limit * (underLoad ? 30 : 40); // no flicker round the threshold
  const uint32_t countdown = sleepCountdown();
  if (xTaskGetTickCount() < dogBarkUntil) {
    const char   *ready = DOG_T("BEREIT", "READY");
    const uint8_t w     = DOG_WUFF_WIDTH + 4 + dogTextWidth(dogFontSmall, ready);
    const int16_t x     = STATUS_X0 + (STATUS_X1 - STATUS_X0 + 1 - w) / 2;
    dogDrawWuff(x, 24);
    dogSmall(x + DOG_WUFF_WIDTH + 4, ready);
  } else if (countdown) {
    char text[14];
    strcpy(text, DOG_T("SCHLAF IN ", "SLEEP IN "));
    dogFormat(text + strlen(text), countdown);
    dogSmallCentre(STATUS_X0, STATUS_X1, text);
  } else if (underLoad) {
    drawLoadRow();
  } else {
    drawSessionTime();
  }
}

// The column that took the dog's place: what the big number and the curve don't say
static void drawMetrics() {
  const bool    flip = OLED::getRotation();
  const int16_t x0   = flip ? 0 : OLED_WIDTH - METRICS_WIDTH;
  const int16_t x1   = x0 + METRICS_WIDTH - 1;
  char          text[8];
  dogSmall(x0, DOG_T("SOLL", "SET"), 7);
  dogFormat(text, getSettingValue(SettingsOptions::SolderingTemp));
  dogSmallRight(x1, text, 7);
  dogSmall(x0, DOG_T("EIN", "IN"), 15);
  formatX10(text, getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0), 'V');
  dogSmallRight(x1, text, 15);
  dogSmall(x0, DOG_T("GRIFF", "GRIP"), 23);
  uint32_t handle = getHandleTemperature(0) / 10;
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    handle = handle * 9 / 5 + 32;
  }
  formatUnit(text, handle, "°");
  dogSmallRight(x1, text, 23);
}

/*
 * Soldering at temperature, no dog: the numbers on one side, the temperature on the other, the curve between them
 *   SOLL  320  ------------  320°  [flame]
 *   EIN 20.1V                          
 *   GRIFF 31°                          
 *   [i]        status row           [updown]
 */
void ui_draw_soldering_basic_status(bool boostModeOn) {
  if (boostModeOn) {
    drawBoostScreen();
    return;
  }
  if (!dogSolderTimerStart) {
    drawHeatingScreen();
    return;
  }
  const bool    flip   = OLED::getRotation();
  const int16_t curveX = flip ? METRICS_WIDTH + 1 : TEMP_LEFT + TEMP_WIDTH + 3;
  drawMetrics();
  dogDrawCurve(curveX, curveX + SMALL_CURVE - 1, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, DogCurve::Steady, true);
  drawTipBig();
  if (drawSolderKeys()) {
    drawSolderStatus();
  }
}

/*
 * The second view: the curve as wide as the temperature leaves it, and below it watts, volts and calculated amps
 *   ------------------------  320°  [flame]
 *   [i]  52W   20.1V   ~2.6A     [updown]
 */
void ui_draw_soldering_power_status(bool boostModeOn) {
  if (boostModeOn) {
    drawBoostScreen();
    return;
  }
  const bool    flip   = OLED::getRotation();
  const int16_t curveX = flip ? 0 : OLED_WIDTH - BIG_CURVE;
  dogDrawCurve(curveX, curveX + BIG_CURVE - 1, CURVE_TOP, CURVE_BOTTOM, currentTempTargetDegC, dogSolderTimerStart ? DogCurve::Steady : DogCurve::Heat, true);
  drawTipBig();
  if (drawSolderKeys()) {
    char text[9];
    formatUnit(text, wattsNow(), "W");
    dogSmall(12, text);
    formatX10(text, getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0), 'V');
    dogSmallCentre(STATUS_X0, STATUS_X1, text);
    text[0] = '~';
    formatX10(text + 1, ampsX10(), 'A');
    dogSmallRight(TEMP_RIGHT, text);
  }
}

/*
 * Temperature view, the same while soldering and on the home screen; the scale runs from 250 to 450 °C with the presets on it
 *   SOLL 320°   [curve or the preset's use, or the tip when not heating]   [flame]
 *   [+]  ----|----v----|----|----  [-]
 */
static const char *presetUse(uint16_t setC, uint8_t line) {
  static const uint16_t temps[] = TS101_PRESETS_C;
  for (uint8_t i = 0; i < sizeof(temps) / sizeof(temps[0]); i++) {
    if (temps[i] == setC) {
      static const char *const de[][2] = {{"SMD +", "BLEILOT"}, {"BLEIFREI", "LOT"}, {"KABEL +", "MASSE"}};
      static const char *const en[][2] = {{"SMD +", "LEADED"}, {"LEAD-FREE", "SOLDER"}, {"WIRES +", "MASS"}};
      return (dogLangDE() ? de : en)[i][line];
    }
  }
  return nullptr;
}

void dogDrawTemperatureView(bool heating) {
  const bool     flip  = OLED::getRotation();
  const bool     inF   = getSettingValue(SettingsOptions::TemperatureInF);
  const uint16_t set   = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t setC  = inF ? TipThermoModel::convertFtoC(set) : set;
  const char    *word  = DOG_T("SOLL", "SET");
  dogSmall(0, word, 15);
  const int16_t bigX = dogTextWidth(dogFontSmall, word) + 2;
  dogBig(bigX, set);
  const int16_t right = bigX + TEMP_WIDTH + 3;
  const int16_t xMax  = flip ? 117 : OLED_WIDTH - 1;
  const int16_t xMin  = (flip || right >= 10) ? right : 10;
  if (heating) {
    dogDrawFlame(dogFrontKeyX());
  }
  const char *use0 = presetUse(setC, 0);
  if (use0) {
    dogSmall(xMin, use0, 12);
    dogSmall(xMin, presetUse(setC, 1), 21);
  } else if (heating) {
    dogDrawCurve(xMin, xMax, CURVE_TOP, CURVE_BOTTOM, setC, DogCurve::Steady, true);
  } else {
    char text[12];
    strcpy(text, DOG_T("IST ", "TIP "));
    formatUnit(text + strlen(text), getTipTemp(), "°");
    dogSmallRight(xMax, text, 15);
  }
  if (dogDrawHold(DOG_T("HALTEN: PRESET", "HOLD: PRESET"), true, DOG_HOLD_FULL) || dogDrawHold(DOG_T("HALTEN: FERTIG", "HOLD: DONE"), false, DOG_HOLD_FULL)) {
    return;
  }
  dogDrawCorners(iconPlus, iconMinus);
  const int16_t x0 = STATUS_X0, x1 = STATUS_X1;
  auto          scaleX = [&](int32_t c) -> int16_t {
    c = c < 250 ? 250 : (c > 450 ? 450 : c);
    return x0 + (c - 250) * (x1 - x0) / 200;
  };
  OLED::drawFilledRect(x0, 29, x1, 29, false);
  OLED::drawFilledRect(x0, 28, x0, 30, false);
  OLED::drawFilledRect(x1, 28, x1, 30, false);
  static const uint16_t temps[] = TS101_PRESETS_C;
  for (uint16_t t : temps) {
    OLED::drawFilledRect(scaleX(t), 27, scaleX(t), 31, false);
  }
  const int16_t c = scaleX(setC);
  OLED::drawFilledRect(c - 2, 24, c + 2, 24, false);
  OLED::drawFilledRect(c - 1, 25, c + 1, 25, false);
  OLED::drawFilledRect(c, 26, c, 26, false);
}

/*
 * Sleep: the dog lies there awake for a moment, closes its eyes and "z Z Z" drifts up from its head;
 * on the right the tip temperature, how long it has been asleep and what wakes it
 */
static void drawSleepingDog() {
  const TickType_t inSleep = xTaskGetTickCount() - dogSleepEnterTime;
  if (inSleep < TICKS_100MS * 8) {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLyingAwake);
  } else {
    OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLying);
    uint8_t step = (xTaskGetTickCount() / (TICKS_100MS * 6)) % DOG_ZZZ_STEPS;
    OLED::drawArea(50, 0, DOG_ZZZ_WIDTH, 8, dogZzz[step]);
  }
}

static void drawSleepScreen(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  drawSleepingDog();
  char text[12];
  dogFormat(text, tipTemp);
  dogText(dogFontMid, OLED_WIDTH - 1 - dogTextWidth(dogFontMid, text) - 6, 16, text);
  OLED::drawArea(OLED_WIDTH - 5, 0, 5, 8, dogRing);
  uint32_t seconds = (xTaskGetTickCount() - dogSleepEnterTime) / TICKS_SECOND;
  dogFormatTime(text, seconds / 60, seconds % 60);
  dogSmallRight(OLED_WIDTH - 1, text, 23);
  dogSmallRight(OLED_WIDTH - 1, DOG_T("HEBEN!", "LIFT!"));
  OLED::refresh();
}

void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }
void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) { drawSleepScreen(tipTemp); }

/*
 * Home, the iron is not heating; the power supply is always in the key row because it decides how hard the iron can heat
 *   [ head ]     ZIEL 320°                  <- set point; while still hot HEISS, the tip temperature and a small arrow
 *   [=] MENÜ     20V 90W      LÖTEN [>]
 * Picking the iron up after it lay still makes the dog run across and bark; alerts replace everything else.
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
  OLED::drawFilledRect(x, 23, x + DOG_HEAD_WIDTH - 1, 23, true);
}

void ui_draw_homescreen_simplified(TemperatureType_t tipTemp) {
  static TickType_t lastSeenMovement = 0;
  static TickType_t pickedUpAt       = 0;
  static bool       tempOnDisplay    = false;
  const bool        flip             = OLED::getRotation();
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

  // Dog: runs across for a second, then the head barks for a second and calms down
  const TickType_t sincePickup = now - pickedUpAt;
  const bool       running     = pickedUpAt && sincePickup < TICKS_SECOND;
  if (running) {
    dogDrawRunAcross();
    OLED::drawFilledRect(0, 23, OLED_WIDTH - 1, 23, true);
  } else {
    const bool bark = pickedUpAt && sincePickup < TICKS_SECOND * 2 && ((now / (TICKS_100MS * 3)) % 2);
    drawHeadClipped(flip ? 0 : OLED_WIDTH - DOG_HEAD_WIDTH, bark ? dogHeadBark : dogHead);
  }

  // Big value: the set point, or the tip temperature while it is still hot
  if (tipTemp > 55) {
    tempOnDisplay = true;
  } else if (tipTemp < 45) {
    tempOnDisplay = false;
  }
  const char    *word  = tempOnDisplay ? DOG_T("HEISS", "HOT") : DOG_T("ZIEL", "SET");
  const uint16_t value = tempOnDisplay ? tipTemp : getSettingValue(SettingsOptions::SolderingTemp);
  const uint8_t  wordW = dogTextWidth(dogFontSmall, word);
  const int16_t  areaX = flip ? DOG_HEAD_WIDTH : 0;
  const uint8_t  areaW = OLED_WIDTH - DOG_HEAD_WIDTH;
  const int16_t  x     = areaX + (areaW - (wordW + 3 + TEMP_WIDTH)) / 2;
  if (!running && !(tempOnDisplay && getSettingValue(SettingsOptions::CoolingTempBlink) && (now % 1000 < 300))) {
    dogSmall(x, word, 15);
    dogBig(x + wordW + 3, value);
    if (tempOnDisplay) {
      dogBlit(x + (wordW - 8) / 2, 16, 8, 8, iconArrowDown8);
      OLED::drawFilledRect(x + (wordW - 8) / 2, 23, x + (wordW - 8) / 2 + 7, 23, true);
    }
  }

  // Key row, with the supply in the middle: "20V 90W", marked when it is weak
  if (dogDrawHold(DOG_T("HALTEN: TEMPERATUR", "HOLD: TEMPERATURE"), true, DOG_HOLD_FULL)) {
    return;
  }
  char           supply[12];
  const int32_t  limit = getX10WattageLimits();
  char          *end   = dogFormat(supply, (getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0) + 5) / 10);
  *end++               = 'V';
  *end++               = ' ';
  end                  = dogFormat(end, limit > 0 ? limit / 10 : 0);
  *end++               = 'W';
  *end++               = (limit > 0 && limit <= DOG_WEAK_SUPPLY_X10W) ? '!' : 0;
  *end                 = 0;
  dogDrawKeys({iconPlay8, DOG_T("LÖTEN", "SOLDER")}, {iconMenu, DOG_T("MENÜ", "MENU")}, supply);
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
  dogDrawKeys({iconNext, DOG_T("WEITER", "NEXT")}, {iconBack, DOG_T("ZURÜCK", "BACK")}, nullptr);
}
#endif

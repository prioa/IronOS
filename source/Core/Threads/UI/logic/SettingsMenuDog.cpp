/*
 * TS101_DOG_UI: the settings menu, one list: every setting, then the readings, four rows at a time. Name on the left, value on the right.
 * Rear: a click is the next row, holding goes back to the home screen. Front: a click moves a switch or a choice on at once, or
 * starts changing a number; holding goes up the list. While changing, - and + apply at once; after 2 s without a button, or
 * holding the rear button, it is back on the row. There is nothing to confirm.
 * The position lives in this file, not in the scratch state: the raw debug values are a mode of their own.
 */
#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
#ifdef TS101_DOG_UI

enum class Fmt : uint8_t { Action, OnOff, Stage, Temp, BoostTemp, SleepTemp, SleepTime, Minutes, Autostart, Lock, Tip, DcCutoff, PdMode, PdTimeout, Watts, Unit, Rotation, Plain, Animation, Logo, Pulse };

struct DogMenuItem {
  const char     *de, *en;
  SettingsOptions option;
  Fmt             format;
  void (*action)(void); // runs instead of changing the value
};

// Blocking screens kept from the stock menu, in settingsGUI.cpp
void dogSetCalibrateCJC(void);
void dogSetCalibrateVIN(void);
void dogSetResetSettings(void);

#define OPT(name) SettingsOptions::name
#define NO_OPTION SettingsOptionsLength
// What is changed most often first
static const DogMenuItem items[] = {
    {"EIGENES ZIEL", "OWN TARGET", OPT(SolderingTemp), Fmt::Temp, nullptr},
    {"SPERRE", "LOCK", OPT(LockingMode), Fmt::Lock, nullptr},
    {"BOOST-ZIEL", "BOOST TARGET", OPT(BoostTemp), Fmt::BoostTemp, nullptr},
    {"SCHLAF NACH", "SLEEP AFTER", OPT(SleepTime), Fmt::SleepTime, nullptr},
    {"SCHLAF-TEMP", "SLEEP TEMP", OPT(SleepTemp), Fmt::SleepTemp, nullptr},
    {"AUS NACH", "OFF AFTER", OPT(ShutdownTime), Fmt::Minutes, nullptr},
    {"BEWEGUNG", "MOTION", OPT(Sensitivity), Fmt::Stage, nullptr},
    {"HELLIGKEIT", "BRIGHTNESS", OPT(OLEDBrightness), Fmt::Plain, nullptr},
    {"DREHUNG", "ROTATION", OPT(OrientationMode), Fmt::Rotation, nullptr},
    {"EINHEIT", "UNIT", OPT(TemperatureInF), Fmt::Unit, nullptr},
    {"INVERTIEREN", "INVERT", OPT(OLEDInversion), Fmt::OnOff, nullptr},
    {"DETAILANSICHT", "DETAIL VIEW", OPT(DogSolderView), Fmt::OnOff, nullptr},
    {"TASTEN TAUSCHEN", "SWAP BUTTONS", OPT(ReverseButtonTempChangeEnabled), Fmt::OnOff, nullptr},
    {"ANIMATION", "ANIMATION", OPT(AnimationSpeed), Fmt::Animation, nullptr},
    {"LOGO", "LOGO", OPT(LOGOTime), Fmt::Logo, nullptr},
    {"AUTOSTART", "AUTOSTART", OPT(AutoStartMode), Fmt::Autostart, nullptr},
    {"SPITZE", "TIP", OPT(SolderingTipType), Fmt::Tip, nullptr},
    {"LEISTUNG", "POWER", OPT(PowerLimit), Fmt::Watts, nullptr},
    {"DC-GRENZE", "DC CUTOFF", OPT(MinDCVoltageCells), Fmt::DcCutoff, nullptr},
    {"PD-MODUS", "PD MODE", OPT(USBPDMode), Fmt::PdMode, nullptr},
    {"PD-TIMEOUT", "PD TIMEOUT", OPT(PDNegTimeout), Fmt::PdTimeout, nullptr},
    {"KEEP-AWAKE", "KEEP AWAKE", OPT(KeepAwakePulse), Fmt::Pulse, nullptr},
    {"TEMP KALIBR.", "CALIBRATE TEMP", OPT(CalibrateCJC), Fmt::OnOff, dogSetCalibrateCJC},
    {"VOLT KALIBR.", "CALIBRATE VOLTS", NO_OPTION, Fmt::Action, dogSetCalibrateVIN},
    {"ZURÜCKSETZEN", "RESET", NO_OPTION, Fmt::Action, dogSetResetSettings},
};
#define ITEM_COUNT ((uint8_t)(sizeof(items) / sizeof(items[0])))
#define ROW_COUNT  (ITEM_COUNT + DOG_INFO_ROWS) // the readings follow the settings
#define DEBUG_ROW  (ROW_COUNT - 1)              // the last reading opens the raw debug values
#define HOLD_REPEAT (TICKS_100MS * 3 / 2)
#define UP_HOLD     (TICKS_100MS * 5) // past the 400 ms of a click, so the press can't also count as one
#define BLINK_TICKS (TICKS_100MS * 2)

// Where we are, kept across the raw debug values
static uint8_t    position  = 0;     // the marked row
static uint8_t    top       = 0;     // the first row on screen
static bool       editing   = false; // the number on the marked row is being changed
static bool       changed   = false;
static TickType_t blinkFrom = 0;     // the value inverts once, from this tick

static inline const char *T(const char *de, const char *en) { return dogLangDE() ? de : en; }

void dogMenuBlink() { blinkFrom = xTaskGetTickCount() | 1; }

static bool isNumber(Fmt format) {
  switch (format) {
  case Fmt::Stage:
  case Fmt::Temp:
  case Fmt::BoostTemp:
  case Fmt::SleepTemp:
  case Fmt::SleepTime:
  case Fmt::Minutes:
  case Fmt::PdTimeout:
  case Fmt::Watts:
  case Fmt::Logo:
  case Fmt::Pulse:
    return true;
  default:
    return false;
  }
}

static void withUnit(char *out, uint16_t value, const char *unit) { strcpy(dogFormat(out, value), unit); }

static void withTenths(char *out, uint16_t x10, const char *unit) {
  char *end = dogFormat(out, x10 / 10);
  *end++    = '.';
  *end++    = '0' + x10 % 10;
  strcpy(end, unit);
}

static void formatValue(const DogMenuItem &item, char *out) {
  const uint16_t v   = item.option < SettingsOptionsLength ? getSettingValue(item.option) : 0;
  const char    *off = T("AUS", "OFF");
  // These show one word, or "off" for 0
  if (!v && (item.format == Fmt::Stage || item.format == Fmt::BoostTemp || item.format == Fmt::SleepTime || item.format == Fmt::Minutes || item.format == Fmt::PdTimeout || item.format == Fmt::Watts
             || item.format == Fmt::Pulse || item.format == Fmt::OnOff || item.format == Fmt::Logo)) {
    strcpy(out, off);
    return;
  }
  switch (item.format) {
  case Fmt::OnOff:
    strcpy(out, T("AN", "ON"));
    break;
  case Fmt::Stage:
  case Fmt::Plain:
    dogFormat(out, v);
    break;
  case Fmt::Temp:
  case Fmt::SleepTemp:
  case Fmt::BoostTemp:
    withUnit(out, v, "°");
    break;
  case Fmt::SleepTime:
    if (v < 6) {
      withUnit(out, v * 10, " S");
    } else {
      withUnit(out, v - 5, " MIN");
    }
    break;
  case Fmt::Minutes:
    withUnit(out, v, " MIN");
    break;
  case Fmt::Autostart: {
    static const char *const de[] = {"AUS", "LÖTEN", "SCHLAF", "AUS+WACH"};
    static const char *const en[] = {"OFF", "SOLDER", "SLEEP", "OFF+WAKE"};
    strcpy(out, T(de[v & 3], en[v & 3]));
    break;
  }
  case Fmt::Lock: {
    static const char *const de[] = {"AUS", "NUR BOOST", "ALLES"};
    static const char *const en[] = {"OFF", "BOOST ONLY", "ALL"};
    strcpy(out, T(de[v < 3 ? v : 0], en[v < 3 ? v : 0]));
    break;
  }
  case Fmt::Tip:
    strcpy(out, dogTipName());
    break;
  case Fmt::DcCutoff:
    if (!v) {
      strcpy(out, "DC");
    } else {
      withUnit(out, 2 + v, "S");
    }
    break;
  case Fmt::PdMode: {
    static const char *const de[] = {"FEST", "STANDARD", "SICHER"};
    static const char *const en[] = {"FIXED", "DEFAULT", "SAFE"};
    strcpy(out, T(de[v < 3 ? v : 0], en[v < 3 ? v : 0]));
    break;
  }
  case Fmt::PdTimeout:
    withTenths(out, v, " S");
    break;
  case Fmt::Watts:
    withUnit(out, v, " W");
    break;
  case Fmt::Unit:
    strcpy(out, v ? "°F" : "°C");
    break;
  case Fmt::Rotation: {
    static const char *const de[] = {"RECHTS", "LINKS", "AUTO"};
    static const char *const en[] = {"RIGHT", "LEFT", "AUTO"};
    strcpy(out, T(de[v < 3 ? v : 2], en[v < 3 ? v : 2]));
    break;
  }
  case Fmt::Animation: {
    static const char *const de[] = {"AUS", "LANGSAM", "MITTEL", "SCHNELL"};
    static const char *const en[] = {"OFF", "SLOW", "MEDIUM", "FAST"};
    strcpy(out, T(de[v & 3], en[v & 3]));
    break;
  }
  case Fmt::Logo:
    if (v == logoMode_t::ONETIME) {
      strcpy(out, T("EINMAL", "ONCE"));
    } else if (v == logoMode_t::INFINITY) {
      strcpy(out, T("IMMER", "ALWAYS"));
    } else {
      withUnit(out, v, " S");
    }
    break;
  case Fmt::Pulse:
    withTenths(out, v, " W");
    break;
  default:
    out[0] = 0;
    break;
  }
}

// "AUS,250-450°": what a number may be, as the settings and stepValue allow it
static void formatSpan(char *out, bool canBeOff, uint16_t lo, uint16_t hi, const char *unit) {
  if (canBeOff) {
    strcpy(out, T("AUS,", "OFF,"));
    out += 4;
  }
  out    = dogFormat(out, lo);
  *out++ = '-';
  withUnit(out, hi, unit);
}

static void formatRange(const DogMenuItem &item, char *out) {
  const bool inF = getSettingValue(SettingsOptions::TemperatureInF);
  switch (item.format) {
  case Fmt::Temp:
    formatSpan(out, false, inF ? MIN_TEMP_F : MIN_TEMP_C, inF ? MAX_TEMP_F : MAX_TEMP_C, "°");
    break;
  case Fmt::BoostTemp:
    formatSpan(out, true, inF ? MIN_BOOST_TEMP_F : MIN_BOOST_TEMP_C, inF ? MAX_TEMP_F : MAX_TEMP_C, "°");
    break;
  case Fmt::SleepTemp:
    formatSpan(out, false, inF ? 60 : 10, inF ? 580 : 300, "°");
    break;
  case Fmt::SleepTime:
    strcpy(out, T("AUS,10 S-10 MIN", "OFF,10 S-10 MIN"));
    break;
  case Fmt::Minutes:
    formatSpan(out, true, 1, 60, " MIN");
    break;
  case Fmt::Stage:
    formatSpan(out, true, 1, 9, "");
    break;
  case Fmt::PdTimeout:
    strcpy(out, T("AUS,0.1-5.0 S", "OFF,0.1-5.0 S"));
    break;
  case Fmt::Watts:
    formatSpan(out, true, POWER_LIMIT_STEPS, MAX_POWER_LIMIT, " W");
    break;
  case Fmt::Logo:
    strcpy(out, T("AUS BIS IMMER", "OFF TO ALWAYS"));
    break;
  case Fmt::Pulse:
    strcpy(out, T("AUS,0.1-9.9 W", "OFF,0.1-9.9 W"));
    break;
  default:
    out[0] = 0;
    break;
  }
}

static void convertTemp(SettingsOptions option) {
  uint16_t temp = getSettingValue(option);
  temp          = getSettingValue(SettingsOptions::TemperatureInF) ? ((temp * 9) + 160) / 5 : ((temp - 32) * 5) / 9;
  setSettingValue(option, (temp / 10) * 10);
}

// One step of the value; the steps that depend on the unit are the same as in the stock menu
static void stepValue(const DogMenuItem &item, bool up) {
  const bool inF = getSettingValue(SettingsOptions::TemperatureInF);
  switch (item.format) {
  case Fmt::Temp: {
    // To the next multiple of 5 in that direction, and it stays at the ends
    const int16_t lo = inF ? MIN_TEMP_F : MIN_TEMP_C, hi = inF ? MAX_TEMP_F : MAX_TEMP_C, now = getSettingValue(item.option);
    const int16_t next = up ? (now / 5 + 1) * 5 : ((now - 1) / 5) * 5;
    setSettingValue(item.option, next < lo ? lo : (next > hi ? hi : next));
    return;
  }
  case Fmt::BoostTemp: {
    const int16_t lo = inF ? MIN_BOOST_TEMP_F : MIN_BOOST_TEMP_C, hi = inF ? MAX_TEMP_F : MAX_TEMP_C, step = inF ? 20 : 10;
    int16_t       v = getSettingValue(item.option);
    v               = up ? (v == 0 ? lo : (v + step > hi ? 0 : v + step)) : (v == 0 ? hi : (v - step < lo ? 0 : v - step));
    setSettingValue(item.option, v);
    return;
  }
  case Fmt::SleepTemp: {
    const int16_t lo = inF ? 60 : 10, hi = inF ? 580 : 300, step = inF ? 20 : 10;
    int16_t       v = getSettingValue(item.option);
    v               = up ? (v + step > hi ? lo : v + step) : (v - step < lo ? hi : v - step);
    setSettingValue(item.option, v);
    return;
  }
  default:
    break;
  }
  if (up) {
    nextSettingValue(item.option);
  } else {
    prevSettingValue(item.option);
  }
  // What the stock menu does on top of changing the number
  switch (item.format) {
  case Fmt::Unit:
    convertTemp(SettingsOptions::BoostTemp);
    convertTemp(SettingsOptions::SolderingTemp);
    convertTemp(SettingsOptions::SleepTemp);
    break;
  case Fmt::Rotation:
    if (getSettingValue(item.option) == orientationMode_t::RIGHT) {
      OLED::setRotation(false);
    } else if (getSettingValue(item.option) == orientationMode_t::LEFT) {
      OLED::setRotation(true);
    }
    break;
  case Fmt::Plain:
    OLED::setBrightness(getSettingValue(item.option));
    break;
  case Fmt::OnOff:
    if (item.option == SettingsOptions::OLEDInversion) {
      OLED::setInverseDisplay(getSettingValue(item.option));
    }
    break;
  case Fmt::Lock:
    if (!getSettingValue(item.option)) {
      dogLockArmed = false; // the gesture that undoes it does nothing now, so an armed lock would stay for good
    }
    break;
  case Fmt::Animation:
    setSettingValue(SettingsOptions::AnimationLoop, getSettingValue(item.option) ? 1 : 0); // the speed and the loop are one point now
    break;
  default:
    break;
  }
}

// ---- Drawing ----
#define ROWS        4
#define ROW_HEIGHT  8
#define NAME_X      2
#define VALUE_RIGHT 122 // values end here
#define MARK_RIGHT  124 // the inverted mark ends here, the scroll bar is on columns 126-127

// One row of the list, without the mark. Returns the column where its value starts, for inverting it alone
static int16_t drawRow(uint8_t index, int16_t y0) {
  const int16_t base = y0 + 7;
  char          value[20];
  if (index >= ITEM_COUNT) {
    const char   *label = dogInfoRow(index - ITEM_COUNT, value);
    const int16_t width = dogTextWidth(dogFontSmall, value);
    if (dogTextWidth(dogFontSmall, label) + 6 + width <= VALUE_RIGHT - NAME_X) {
      dogSmall(NAME_X, label, base); // a long development version takes the whole row
    }
    if (index == DEBUG_ROW) {
      dogBlit(VALUE_RIGHT - 7, y0, 8, 8, iconNext);
    } else {
      dogSmallRight(VALUE_RIGHT, value, base);
    }
    return VALUE_RIGHT - width + 1;
  }
  const DogMenuItem &item = items[index];
  dogSmall(NAME_X, T(item.de, item.en), base);
  if (item.format == Fmt::Action) {
    dogBlit(VALUE_RIGHT - 7, y0, 8, 8, iconNext);
    return VALUE_RIGHT - 7;
  }
  formatValue(item, value);
  dogSmallRight(VALUE_RIGHT, value, base);
  return VALUE_RIGHT - dogTextWidth(dogFontSmall, value) + 1;
}

static void drawScrollBar(uint8_t pos) {
  OLED::drawFilledRect(OLED_WIDTH - 1, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, false);
  const uint8_t y = pos * (OLED_HEIGHT - 6) / (ROW_COUNT - 1);
  OLED::drawFilledRect(OLED_WIDTH - 2, y, OLED_WIDTH - 2, y + 5, false);
}

static void scrollTo(uint8_t pos) {
  if (pos < top) {
    top = pos;
  } else if (pos >= top + ROWS) {
    top = pos - ROWS + 1;
  }
}

void dogDrawMenuList(uint8_t pos) {
  scrollTo(pos);
  for (uint8_t row = 0; row < ROWS; row++) {
    const uint8_t index = top + row;
    const int16_t y0    = row * ROW_HEIGHT;
    const int16_t start = drawRow(index, y0);
    if (index == pos) {
      if (blinkFrom && (xTaskGetTickCount() | 1) - blinkFrom < BLINK_TICKS) {
        OLED::invertRect(start - 1, y0, VALUE_RIGHT + 1, y0 + ROW_HEIGHT - 1); // inverted again below: the value blinks
      }
      OLED::invertRect(0, y0, MARK_RIGHT, y0 + ROW_HEIGHT - 1);
    }
  }
  drawScrollBar(pos);
}

// While changing a number only its value is inverted, and the last row says what it may be; the first, if the number is on the last
void dogDrawMenuValue(uint8_t pos) {
  scrollTo(pos);
  const uint8_t rangeRow = pos - top == ROWS - 1 ? 0 : ROWS - 1;
  for (uint8_t row = 0; row < ROWS; row++) {
    const int16_t y0 = row * ROW_HEIGHT;
    if (row == rangeRow) {
      char range[20];
      formatRange(items[pos], range);
      dogSmallRight(VALUE_RIGHT, range, y0 + 7);
      continue;
    }
    const int16_t start = drawRow(top + row, y0);
    if (top + row == pos) {
      OLED::invertRect(start - 1, y0, VALUE_RIGHT + 1, y0 + ROW_HEIGHT - 1);
    }
  }
  drawScrollBar(pos);
}

#ifdef BUTTON_TEST
// Every row fits in the language of this build; returns the first that does not, or nullptr
const char *dogMenuRowOverflow() {
  static const uint16_t samples[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 50, 60, 99, 100, 120, 180, 300, 450, 850};
  const int16_t         width     = VALUE_RIGHT - NAME_X + 1;
  char                  text[24];
  for (uint8_t i = 0; i < ITEM_COUNT; i++) {
    const DogMenuItem &item = items[i];
    const char        *name = T(item.de, item.en);
    formatRange(item, text);
    if (dogTextWidth(dogFontSmall, text) > width) {
      return name;
    }
    if (item.option >= SettingsOptionsLength) {
      if (dogTextWidth(dogFontSmall, name) + 12 > width) {
        return name;
      }
      continue;
    }
    const uint16_t saved = getSettingValue(item.option);
    for (uint16_t v : samples) {
      setSettingValue(item.option, v);
      formatValue(item, text);
      if (dogTextWidth(dogFontSmall, name) + 6 + dogTextWidth(dogFontSmall, text) > width) {
        setSettingValue(item.option, saved);
        return name;
      }
    }
    setSettingValue(item.option, saved);
  }
  for (uint8_t i = 0; i < DOG_INFO_ROWS - 2; i++) { // the version may be long, and debug has no value
    const char *label = dogInfoRow(i, text);
    if (dogTextWidth(dogFontSmall, label) + 6 + dogTextWidth(dogFontSmall, text) > width) {
      return label;
    }
  }
  return nullptr;
}
#endif

// ---- Logic ----
static void leaveValue() {
  editing = false;
  if (changed) {
    changed = false;
    saveSettings(); // pulling the plug costs no setting; the heater is off in the menu, so the flash write is harmless
  }
}

static OperatingMode editValue(const ButtonState buttons, guiContext *cxt) {
  const DogMenuItem &item = items[position];
  if (buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    leaveValue();
    dogDrawMenuList(position);
    return OperatingMode::SettingsMenu;
  }
  if (dogHoldDone(true, DOG_HOLD_FULL)) {
    // Faster +: repeats while held, stops at the top rather than wrapping round to the bottom
    if (xTaskGetTickCount() - cxt->scratch_state.state3 >= HOLD_REPEAT && !isLastSettingValue(item.option)) {
      cxt->scratch_state.state3 = xTaskGetTickCount();
      stepValue(item, true);
      changed = true;
    }
  } else if (buttons == BUTTON_F_SHORT || buttons == BUTTON_B_SHORT) {
    stepValue(item, buttons == BUTTON_F_SHORT);
    changed = true;
  } else if (xTaskGetTickCount() - lastButtonTime > DOG_VALUE_TIMEOUT) {
    leaveValue();
    dogDrawMenuList(position);
    return OperatingMode::SettingsMenu;
  }
  dogDrawMenuValue(position);
  return OperatingMode::SettingsMenu;
}

OperatingMode gui_SettingsMenu(const ButtonState buttons, guiContext *cxt) {
  // A fresh visit from the home screen starts at the top; coming back from the raw debug values does not
  if (!cxt->scratch_state.state1) {
    cxt->scratch_state.state1 = 1;
    if (cxt->previousMode == OperatingMode::HomeScreen) {
      position = top = 0;
      editing        = false;
      blinkFrom      = 0;
    }
  }
  if (editing) {
    return editValue(buttons, cxt);
  }
  dogDrawMenuList(position);
  if (buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::HomeScreen;
  }
  if (dogHoldDone(true, UP_HOLD)) {
    // Up the list, repeating while held: the other way round than the rear button's click
    if (xTaskGetTickCount() - cxt->scratch_state.state3 >= HOLD_REPEAT) {
      cxt->scratch_state.state3 = xTaskGetTickCount();
      position                  = (position + ROW_COUNT - 1) % ROW_COUNT;
    }
    return OperatingMode::SettingsMenu;
  }
  if (buttons == BUTTON_B_SHORT) {
    position = (position + 1) % ROW_COUNT;
  } else if (buttons == BUTTON_F_SHORT) {
    if (position == DEBUG_ROW) {
      cxt->transitionMode = TransitionAnimation::Right;
      return OperatingMode::DebugMenuReadout;
    }
    if (position < ITEM_COUNT) {
      const DogMenuItem &item = items[position];
      if (item.action) {
        item.action();
        saveSettings();
        dogMenuBlink();
      } else if (isNumber(item.format)) {
        editing = true;
        changed = false;
      } else {
        stepValue(item, true);
        saveSettings();
        dogMenuBlink();
      }
    }
  }
  return OperatingMode::SettingsMenu;
}
#endif

/*
 * TS101_DOG_UI: the settings menu, one entry per screen. Top left the section with a row of dots for the position, below it the
 * name, top right the value; at the bottom what comes next, or while changing a number the range it may take.
 * Rear: a click is the next entry, holding goes up a level. Front: a click opens a section, moves a switch or a choice of up
 * to five on at once, or starts changing a number. While changing, - and + apply at once; after 2 s without a button, or
 * holding the rear button, it is back on the entry. There is nothing to confirm.
 * The position lives in this file, not in the scratch state: the raw debug values are a mode of their own.
 */
#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ts101_v2_solder.hpp"
#include "ui_drawing.hpp"
#ifdef TS101_DOG_UI

enum class Fmt : uint8_t { Action, OnOff, Stage, Temp, BoostTemp, SleepTemp, SleepTime, Minutes, Autostart, Lock, Tip, DcCutoff, PdMode, PdTimeout, Watts, Unit, Rotation, Plain, Animation, Logo, Pulse, Section, Info, SensorMode };

struct DogMenuItem {
  const char     *de, *en;
  SettingsOptions option; // Fmt::Section: the section it opens; Fmt::Info: the page
  Fmt             format;
  void (*action)(void);   // runs instead of changing the value
};

// Blocking screens kept from the stock menu, in settingsGUI.cpp
void dogSetCalibrateCJC(void);
void dogSetCalibrateVIN(void);
void dogSetResetSettings(void);

#define OPT(name) SettingsOptions::name
#define NO_OPTION SettingsOptionsLength
#define SECTION(n) ((SettingsOptions)(n))
static const DogMenuItem rootItems[] = {
    {"Eigenes Ziel", "Own target", OPT(SolderingTemp), Fmt::Temp, nullptr},
    {"Löten", "Iron", SECTION(1), Fmt::Section, nullptr},
    {"Schlaf", "Sleep", SECTION(2), Fmt::Section, nullptr},
    {"Anzeige", "Display", SECTION(3), Fmt::Section, nullptr},
    {"Sperre", "Lock", OPT(LockingMode), Fmt::Lock, nullptr},
    {"Info", "Info", SECTION(DOG_MENU_SECTION_INFO), Fmt::Section, nullptr},
    {"Zurücksetzen", "Reset", NO_OPTION, Fmt::Action, dogSetResetSettings},
};
static const DogMenuItem solderItems[] = {
    {"Autostart", "Autostart", OPT(AutoStartMode), Fmt::Autostart, nullptr},
    {"Spitze", "Tip", OPT(SolderingTipType), Fmt::Tip, nullptr},
    {"Leistung", "Power", OPT(PowerLimit), Fmt::Watts, nullptr},
    {"DC-Grenze", "DC cutoff", OPT(MinDCVoltageCells), Fmt::DcCutoff, nullptr},
    {"PD-Modus", "PD mode", OPT(USBPDMode), Fmt::PdMode, nullptr},
    {"PD-Timeout", "PD timeout", OPT(PDNegTimeout), Fmt::PdTimeout, nullptr},
    {"Keep-Awake", "Keep awake", OPT(KeepAwakePulse), Fmt::Pulse, nullptr},
    {"Temp kalibr.", "Calibr. temp", OPT(CalibrateCJC), Fmt::OnOff, dogSetCalibrateCJC},
    {"Volt kalibr.", "Calibr. volts", NO_OPTION, Fmt::Action, dogSetCalibrateVIN},
};
static const DogMenuItem sleepItems[] = {
    {"Schlaf nach", "Sleep after", OPT(SleepTime), Fmt::SleepTime, nullptr},
    {"Schlaf-Temp", "Sleep temp", OPT(SleepTemp), Fmt::SleepTemp, nullptr},
    {"Aus nach", "Off after", OPT(ShutdownTime), Fmt::Minutes, nullptr},
    {"Bewegung", "Motion", OPT(Sensitivity), Fmt::Stage, nullptr},
};
static const DogMenuItem displayItems[] = {
    {"Drehung", "Rotation", OPT(OrientationMode), Fmt::Rotation, nullptr},
    {"Helligkeit", "Brightness", OPT(OLEDBrightness), Fmt::Plain, nullptr},
    {"Invertieren", "Invert", OPT(OLEDInversion), Fmt::OnOff, nullptr},
    {"Einheit", "Unit", OPT(TemperatureInF), Fmt::Unit, nullptr},
    {"Animation", "Animation", OPT(AnimationSpeed), Fmt::Animation, nullptr},
    {"Logo", "Logo", OPT(LOGOTime), Fmt::Logo, nullptr},
    {"Sensoren", "Sensors", OPT(DogSensorMode), Fmt::SensorMode, nullptr},
    {"Tastentausch", "Swap buttons", OPT(ReverseButtonTempChangeEnabled), Fmt::OnOff, nullptr},
};
static const DogMenuItem infoItems[DOG_INFO_PAGES] = {
    {"Strom", "Power", SECTION(0), Fmt::Info, nullptr},
    {"Spitze", "Tip", SECTION(1), Fmt::Info, nullptr},
    {"Griff + Lage", "Grip + side", SECTION(2), Fmt::Info, nullptr},
    {"Statistik", "Statistics", SECTION(3), Fmt::Info, nullptr},
    {"System", "System", SECTION(4), Fmt::Info, nullptr},
};

struct DogMenuSection {
  const DogMenuItem *items;
  uint8_t            count;
};
#define ITEMS(list) {list, sizeof(list) / sizeof(list[0])}
static const DogMenuSection sections[] = {ITEMS(rootItems), ITEMS(solderItems), ITEMS(sleepItems), ITEMS(displayItems), ITEMS(infoItems)};
#define HOLD_REPEAT (TICKS_100MS * 3 / 2)
#define BLINK_TICKS (TICKS_100MS * 2)

// Where we are, kept across the raw debug values
static uint8_t    openSection = 0;     // 0 is the top level
static uint8_t    position    = 0;     // in that section
static bool       editing     = false; // the number of the current entry is being changed
static bool       changed     = false;
static TickType_t blinkFrom   = 0;     // the value inverts once, from this tick

static inline const char *T(const char *de, const char *en) { return dogLangDE() ? de : en; }
static inline const char *nameOf(const DogMenuItem &item) { return T(item.de, item.en); }

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

// The value, in the large font
static void formatValue(const DogMenuItem &item, char *out) {
  const uint16_t v   = item.option < SettingsOptionsLength ? getSettingValue(item.option) : 0;
  const char    *off = T("Aus", "Off");
  // These show one word, or "off" for 0
  if (!v && (item.format == Fmt::Stage || item.format == Fmt::BoostTemp || item.format == Fmt::SleepTime || item.format == Fmt::Minutes || item.format == Fmt::PdTimeout || item.format == Fmt::Watts
             || item.format == Fmt::Pulse || item.format == Fmt::OnOff || item.format == Fmt::Logo)) {
    strcpy(out, off);
    return;
  }
  switch (item.format) {
  case Fmt::OnOff:
    strcpy(out, T("An", "On"));
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
      withUnit(out, v - 5, " Min");
    }
    break;
  case Fmt::Minutes:
    withUnit(out, v, " Min");
    break;
  case Fmt::Autostart: {
    static const char *const de[] = {"Aus", "Löten", "Schlaf", "Wach"};
    static const char *const en[] = {"Off", "Solder", "Sleep", "Wake"};
    strcpy(out, T(de[v & 3], en[v & 3]));
    break;
  }
  case Fmt::Lock: {
    static const char *const de[] = {"Aus", "Boost", "Alles"};
    static const char *const en[] = {"Off", "Boost", "All"};
    strcpy(out, T(de[v < 3 ? v : 0], en[v < 3 ? v : 0]));
    break;
  }
  case Fmt::Tip: {
    static const char *const names[] = {"Auto", "TS100", "Pine", "PTS200"}; // the order of tipType_t
    strcpy(out, v < 4 ? names[v] : "?");
    break;
  }
  case Fmt::DcCutoff:
    if (!v) {
      strcpy(out, "DC");
    } else {
      withUnit(out, 2 + v, "S");
    }
    break;
  case Fmt::PdMode: {
    static const char *const de[] = {"Fest", "Normal", "Sicher"};
    static const char *const en[] = {"Fixed", "Normal", "Safe"};
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
  case Fmt::SensorMode:
    strcpy(out, v ? T("Wechseln", "Switch") : T("Schütteln", "Shake"));
    break;
  case Fmt::Rotation: {
    static const char *const de[] = {"Rechts", "Links", "Auto"};
    static const char *const en[] = {"Right", "Left", "Auto"};
    strcpy(out, T(de[v < 3 ? v : 2], en[v < 3 ? v : 2]));
    break;
  }
  case Fmt::Animation: {
    static const char *const de[] = {"Aus", "Langs.", "Mittel", "Schnell"};
    static const char *const en[] = {"Off", "Slow", "Medium", "Fast"};
    strcpy(out, T(de[v & 3], en[v & 3]));
    break;
  }
  case Fmt::Logo:
    if (v == logoMode_t::ONETIME) {
      strcpy(out, T("Einmal", "Once"));
    } else if (v == logoMode_t::INFINITY) {
      strcpy(out, T("Immer", "Always"));
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

// ---- Drawing (interface v2) ----
//   Schlaf nach                 10       A the entry                  D the value: a number in helvB10, a word in helvB08
//   SCHLAF 1/4                  MIN      B section and position       E the unit ("SOLL" for temperatures)
//   v SCHLAF-TEMP             [v] [±]    C what comes next            F next entry, what the front button does
// While changing a number: the value inverted, row 2 the range, row 3 a bar up to the value, - and +
#define BLINK_TICKS_V2 BLINK_TICKS

// The capitals of a name: ä ö ü become Ä Ö Ü
static void toCapitals(char *out, const char *text) {
  for (; *text; text++) {
    uint8_t c = *text;
    if (c >= 'a' && c <= 'z') {
      c -= 'a' - 'A';
    } else if (c == 0xA4 || c == 0xB6 || c == 0xBC) {
      c -= 0x20; // the second UTF-8 byte of ä ö ü to Ä Ö Ü
    }
    *out++ = c;
  }
  *out = 0;
}

// The entry of the top level that opens this section
static uint8_t parentOf(uint8_t section) {
  uint8_t i = 0;
  while (rootItems[i].format != Fmt::Section || (uint8_t)rootItems[i].option != section) {
    i++;
  }
  return i;
}

static void formatSectionTitle(char *out, uint8_t section) { toCapitals(out, section ? nameOf(rootItems[parentOf(section)]) : T("Menü", "Menu")); }

// Row 2: "SCHLAF 1/4"
static void drawPosition(uint8_t section, uint8_t pos) {
  char  text[32];
  formatSectionTitle(text, section);
  char *p = text + strlen(text);
  *p++    = ' ';
  p       = dogFormat(p, pos + 1);
  *p++    = '/';
  dogFormat(p, sections[section].count);
  gridValues(0, text);
}

// Row 3, capitals from row 23: a small triangle and the next entry
static void drawNext(uint8_t section, uint8_t pos) {
  char text[24];
  toCapitals(text, nameOf(sections[section].items[(pos + 1) % sections[section].count]));
  gridClip(0, GRID_LEFT_END);
  for (uint8_t i = 0; i < 3; i++) {
    gridFill(i, 26 + i, 4 - i, 26 + i); // 5 wide, 3 high, on the middle of the capitals
  }
  dogText(dogFontName, 7, 31, text); // 2 px after the triangle
  gridClipAll();
}

// A number splits into its digits for zone D and its unit for zone E; false for a word ("Aus", "Links")
static bool splitNumber(const DogMenuItem &item, const char *value, char *digits, char *unit) {
  if (!isNumber(item.format) || value[0] < '0' || value[0] > '9') {
    return false;
  }
  while ((*value >= '0' && *value <= '9') || *value == '.') {
    *digits++ = *value++;
  }
  *digits = 0;
  while (*value == ' ') {
    value++;
  }
  if (item.format == Fmt::Temp || item.format == Fmt::SleepTemp || item.format == Fmt::BoostTemp) {
    strcpy(unit, T("SOLL", "SET"));
  } else {
    toCapitals(unit, value);
  }
  return true;
}

static void drawValue(const DogMenuItem &item, bool inverted) {
  if (item.format == Fmt::Section || item.format == Fmt::Action || item.format == Fmt::Info) {
    return; // a section, or something to run: zones D and E stay empty
  }
  char value[16], digits[8], unit[8];
  formatValue(item, value);
  int16_t x;
  if (splitNumber(item, value, digits, unit)) {
    const bool degrees = item.format == Fmt::Temp || item.format == Fmt::SleepTemp || item.format == Fmt::BoostTemp;
    if (degrees) {
      gridTemp(getSettingValue(item.option));
      x = OLED_WIDTH - dogTextWidth(dogFontNum, digits) - 1 - dogV2Ring.width;
    } else {
      x = OLED_WIDTH - dogTextWidth(dogFontNum, digits);
      dogText(dogFontNum, x, 11, digits);
    }
    gridTextRight(dogFont3x5, OLED_WIDTH - 1, GRID_TARGET_BASE, unit);
  } else {
    // A switch or choice in helvB08; a long word runs left into the name row where the name leaves it room
    x = OLED_WIDTH - dogTextWidth(dogFontName, value);
    dogText(dogFontName, x, GRID_NAME_BASE, value);
  }
  if (inverted) {
    OLED::invertRect(x - 1 < GRID_RIGHT_X ? x - 1 : GRID_RIGHT_X, 0, OLED_WIDTH - 1, 10);
  }
}

static const DogIcon *frontIcon(const DogMenuItem &item) {
  switch (item.format) {
  case Fmt::Section:
  case Fmt::Action:
    return &iconV2Play;
  case Fmt::Info:
    return (uint8_t)item.option == DOG_INFO_PAGES - 1 ? &iconV2Play : nullptr; // only the last page leads on
  default:
    return isNumber(item.format) ? &iconV2Change : &iconV2Toggle;
  }
}

void dogDrawMenuList(uint8_t section, uint8_t pos) {
  const DogMenuItem &item = sections[section].items[pos];
  gridName(nameOf(item));
  if (item.format == Fmt::Info) {
    dogDrawInfoRows((uint8_t)item.option); // its two readings on rows 2 and 3
  } else {
    drawPosition(section, pos);
    drawNext(section, pos);
    drawValue(item, blinkFrom && (xTaskGetTickCount() | 1) - blinkFrom < BLINK_TICKS);
  }
  gridKeys(&iconV2Next, frontIcon(item));
}

// Where the value is between the ends it can take: 0-93 columns of the bar
static int16_t barColumns(const DogMenuItem &item) {
  const bool inF = getSettingValue(SettingsOptions::TemperatureInF);
  int32_t    lo = settingsConstants[(int)item.option].min, hi = settingsConstants[(int)item.option].max;
  if (item.format == Fmt::Temp) {
    lo = inF ? MIN_TEMP_F : MIN_TEMP_C, hi = inF ? MAX_TEMP_F : MAX_TEMP_C;
  } else if (item.format == Fmt::SleepTemp) {
    lo = inF ? 60 : 10, hi = inF ? 580 : 300;
  }
  const int32_t v = getSettingValue(item.option);
  if (hi <= lo || v <= lo) {
    return 0;
  }
  return v >= hi ? GRID_LEFT_END + 1 : (v - lo) * (GRID_LEFT_END + 1) / (hi - lo);
}

void dogDrawMenuValue(uint8_t section, uint8_t pos) {
  const DogMenuItem &item = sections[section].items[pos];
  gridName(nameOf(item));
  char range[24];
  formatRange(item, range);
  gridValues(0, range);
  // The bar on rows 25-30: its outline, filled up to the value
  gridClip(0, GRID_LEFT_END);
  gridFill(0, 25, GRID_LEFT_END, 25);
  gridFill(0, 30, GRID_LEFT_END, 30);
  gridFill(0, 25, 0, 30);
  gridFill(GRID_LEFT_END, 25, GRID_LEFT_END, 30);
  const int16_t filled = barColumns(item);
  if (filled > 0) {
    gridFill(0, 25, filled - 1, 30);
  }
  gridClipAll();
  drawValue(item, true);
  gridKeys(&iconV2Minus, &iconV2Plus);
}

#ifdef BUTTON_TEST
// Every entry fits its zones in the language of this build; returns the first that does not, or nullptr
const char *dogMenuRowOverflow() {
  static const uint16_t samples[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 50, 60, 99, 100, 120, 180, 300, 450, 850};
  char                  text[32];
  for (uint8_t sct = 0; sct < sizeof(sections) / sizeof(sections[0]); sct++) {
    for (uint8_t i = 0; i < sections[sct].count; i++) {
      const DogMenuItem &item = sections[sct].items[i];
      if (dogTextWidth(dogFontName, nameOf(item)) > GRID_NAME_END + 1) {
        return nameOf(item); // the name row
      }
      toCapitals(text, nameOf(item));
      if (7 + dogTextWidth(dogFontName, text) > GRID_LEFT_END + 1) {
        return nameOf(item); // row 3, as the next entry
      }
      formatRange(item, text);
      if (dogTextWidth(dogFontName, text) > GRID_LEFT_END + 1) {
        return nameOf(item);
      }
      if (item.option < SettingsOptionsLength && item.format != Fmt::Section && item.format != Fmt::Info) {
        const uint16_t saved = getSettingValue(item.option);
        for (uint16_t v : samples) {
          // The settings only take values their limits allow; these are the ones this format can show
          if (item.format == Fmt::Logo && v > 6) continue;
          if (item.format == Fmt::SleepTime && v > 15) continue;
          if (item.format == Fmt::Minutes && v > 60) continue;
          if (item.format == Fmt::Watts && v > MAX_POWER_LIMIT) continue;
          if (item.format == Fmt::DcCutoff && v > 4) continue;
          setSettingValue(item.option, v);
          char value[16], digits[8], unit[8];
          formatValue(item, value);
          const bool fits = splitNumber(item, value, digits, unit)
                                ? dogTextWidth(dogFontNum, digits) + 5 <= OLED_WIDTH - GRID_RIGHT_X && dogTextWidth(dogFont3x5, unit) <= OLED_WIDTH - GRID_RIGHT_X
                                : dogTextWidth(dogFontName, nameOf(item)) + 4 + dogTextWidth(dogFontName, value) <= OLED_WIDTH;
          if (!fits) {
            setSettingValue(item.option, saved);
            return nameOf(item);
          }
        }
        setSettingValue(item.option, saved);
      }
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
  const DogMenuItem &item = sections[openSection].items[position];
  if (buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    leaveValue();
    dogDrawMenuList(openSection, position);
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
    dogDrawMenuList(openSection, position);
    return OperatingMode::SettingsMenu;
  }
  dogDrawMenuValue(openSection, position);
  return OperatingMode::SettingsMenu;
}

static OperatingMode goBack(guiContext *cxt) {
  cxt->transitionMode = TransitionAnimation::Left;
  if (!openSection) {
    return OperatingMode::HomeScreen;
  }
  position    = parentOf(openSection); // back on the entry it was opened from
  openSection = 0;
  return OperatingMode::SettingsMenu;
}

OperatingMode gui_SettingsMenu(const ButtonState buttons, guiContext *cxt) {
  // A fresh visit from the home screen starts at the top; coming back from the raw debug values does not
  if (!cxt->scratch_state.state1) {
    cxt->scratch_state.state1 = 1;
    if (cxt->previousMode == OperatingMode::HomeScreen) {
      openSection = position = 0;
      editing                = false;
      blinkFrom              = 0;
    }
  }
  if (editing) {
    return editValue(buttons, cxt);
  }
  dogDrawMenuList(openSection, position);
  if (buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    return goBack(cxt);
  }
  if (buttons == BUTTON_B_SHORT) {
    position = (position + 1) % sections[openSection].count; // nothing slides: the entry just changes
  } else if (buttons == BUTTON_F_SHORT) {
    const DogMenuItem &item = sections[openSection].items[position];
    if (item.format == Fmt::Section) {
      openSection         = (uint8_t)item.option;
      position            = 0;
      cxt->transitionMode = TransitionAnimation::Left;
    } else if (item.format == Fmt::Info) {
      if ((uint8_t)item.option == DOG_INFO_PAGES - 1) {
        cxt->transitionMode = TransitionAnimation::Left;
        return OperatingMode::DebugMenuReadout;
      }
    } else if (item.action) {
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
  return OperatingMode::SettingsMenu;
}
#endif

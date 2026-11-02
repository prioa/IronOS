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
#include "ui_drawing.hpp"
#ifdef TS101_DOG_UI

enum class Fmt : uint8_t { Action, OnOff, Stage, Temp, BoostTemp, SleepTemp, SleepTime, Minutes, Autostart, Lock, Tip, DcCutoff, PdMode, PdTimeout, Watts, Unit, Rotation, Plain, Animation, Logo, Pulse, Section, Info, HomeView };

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
    {"Startbild", "Home view", OPT(DogHomeVariant), Fmt::HomeView, nullptr},
    {"Sperre", "Lock", OPT(LockingMode), Fmt::Lock, nullptr},
    {"Info", "Info", SECTION(DOG_MENU_SECTION_INFO), Fmt::Section, nullptr},
    {"Zurücksetzen", "Reset", NO_OPTION, Fmt::Action, dogSetResetSettings},
};
static const DogMenuItem solderItems[] = {
    {"Boost-Ziel", "Boost target", OPT(BoostTemp), Fmt::BoostTemp, nullptr},
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
    {"Detailansicht", "Detail view", OPT(DogSolderView), Fmt::OnOff, nullptr},
    {"Tasten tausch.", "Swap buttons", OPT(ReverseButtonTempChangeEnabled), Fmt::OnOff, nullptr},
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
  case Fmt::HomeView: {
    static const char *const de[] = {"Hund", "Symbole", "Schäfer", "Abkühlen", "Kolben"};
    static const char *const en[] = {"Dog", "Icons", "Shepherd", "Cooling", "Iron"};
    strcpy(out, T(de[v < DOG_HOME_VIEWS ? v : 0], en[v < DOG_HOME_VIEWS ? v : 0]));
    break;
  }
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

// ---- Drawing ----
#define TOP_BASE   7
#define NAME_BASE  20 // the name's letters start on row 10
#define NEXT_BASE  32 // the small row at the bottom starts on row 25
#define DOT_PITCH  5
#define BOXES_FROM 96 // the rows from 17 down stop short of the button boxes here
extern const uint8_t dogKeyNothing[1];
const uint8_t dogKeyNothing[1] = {0};

static inline int16_t stageX() { return OLED::getRotation() ? 0 : 35; }

// The small font only has capitals; a name is kept once, in its large form
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

// The two boxes at the buttons: an icon of 11x11, nullptr for - and +, dogKeyNothing for an empty box
static void drawBoxes(const uint8_t *front, const uint8_t *rear) {
  const uint8_t x          = OLED::getRotation() ? BOXES_FROM : 0;
  const bool    frontRight = dogFrontOnRight();
  for (uint8_t i = 0; i < 2; i++) {
    const bool     isFront = (i == 1) == frontRight;
    const uint8_t *icon    = isFront ? front : rear;
    dogKeyBox(x + 17 * i, isFront, icon, 2, 2, icon == dogKeyNothing ? 0 : 11, 16);
  }
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

static void drawTopRow(uint8_t section, uint8_t pos) {
  char title[24];
  formatSectionTitle(title, section);
  dogSmall(0, title, TOP_BASE);
  const int16_t x0 = dogTextWidth(dogFontSmall, title) + 3;
  for (uint8_t i = 0; i < sections[section].count; i++) {
    const int16_t x = x0 + i * DOT_PITCH;
    if (i == pos) {
      OLED::drawFilledRect(x, 2, x + 2, 4, false);
    } else {
      OLED::drawFilledRect(x + 1, 3, x + 1, 3, false);
    }
  }
}

// What comes next: a small triangle and its name
static void drawNext(uint8_t section, uint8_t pos) {
  char text[24];
  toCapitals(text, nameOf(sections[section].items[(pos + 1) % sections[section].count]));
  dogBlit(stageX(), 27, 5, 8, iconMiniDown);
  dogSmall(stageX() + 8, text, NEXT_BASE);
}

static void drawValue(const DogMenuItem &item, bool inverted) {
  if (item.format == Fmt::Section || item.format == Fmt::Action) {
    dogBlit(OLED_WIDTH - 5, 0, 5, 16, iconMiniRight);
    return;
  }
  char value[16];
  formatValue(item, value);
  const int16_t x = OLED_WIDTH - dogTextWidth(dogFontLabel, value);
  dogText(dogFontLabel, x, 10, value);
  if (inverted) {
    OLED::invertRect(x > 0 ? x - 1 : 0, 0, OLED_WIDTH - 1, 10);
  }
}

static const uint8_t *frontIcon(const DogMenuItem &item) {
  switch (item.format) {
  case Fmt::Section:
  case Fmt::Action:
    return iconKeyOpen;
  case Fmt::Info:
    return (uint8_t)item.option == DOG_INFO_PAGES - 1 ? iconKeyOpen : dogKeyNothing; // only the last page leads on
  default:
    return isNumber(item.format) ? iconKeyEdit : iconKeyCycle;
  }
}

void dogDrawMenuList(uint8_t section, uint8_t pos) {
  const DogMenuItem &item = sections[section].items[pos];
  drawTopRow(section, pos);
  if (item.format == Fmt::Info) {
    char title[24];
    toCapitals(title, nameOf(item));
    dogSmallRight(OLED_WIDTH - 1, title, TOP_BASE);
    dogDrawInfoRows((uint8_t)item.option);
  } else {
    dogText(dogFontLabel, stageX(), NAME_BASE, nameOf(item));
    drawValue(item, blinkFrom && (xTaskGetTickCount() | 1) - blinkFrom < BLINK_TICKS);
  }
  drawNext(section, pos);
  drawBoxes(frontIcon(item), iconKeyDown);
}

void dogDrawMenuValue(uint8_t section, uint8_t pos) {
  const DogMenuItem &item = sections[section].items[pos];
  drawTopRow(section, pos);
  dogText(dogFontLabel, stageX(), NAME_BASE, nameOf(item));
  drawValue(item, true);
  char range[20];
  formatRange(item, range);
  dogSmall(stageX(), range, NEXT_BASE);
  drawBoxes(nullptr, nullptr);
}

#ifdef BUTTON_TEST
// Every entry fits next to its neighbours in the language of this build; returns the first that does not, or nullptr
const char *dogMenuRowOverflow() {
  static const uint16_t samples[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 50, 60, 99, 100, 120, 180, 300, 450, 850};
  const int16_t         limit     = BOXES_FROM - 2;
  char                  text[24];
  for (uint8_t sct = 0; sct < sizeof(sections) / sizeof(sections[0]); sct++) {
    formatSectionTitle(text, sct);
    const int16_t dotsEnd = dogTextWidth(dogFontSmall, text) + 3 + sections[sct].count * DOT_PITCH;
    for (uint8_t i = 0; i < sections[sct].count; i++) {
      const DogMenuItem &item = sections[sct].items[i];
      toCapitals(text, nameOf(item));
      if (item.format == Fmt::Info) {
        if (dotsEnd + 2 + dogTextWidth(dogFontSmall, text) > OLED_WIDTH) {
          return nameOf(item);
        }
        continue;
      }
      if (dogTextWidth(dogFontLabel, nameOf(item)) > limit || 8 + dogTextWidth(dogFontSmall, text) > limit) {
        return nameOf(item);
      }
      formatRange(item, text);
      if (dogTextWidth(dogFontSmall, text) > limit) {
        return nameOf(item);
      }
      if (item.option < SettingsOptionsLength) {
        const uint16_t saved = getSettingValue(item.option);
        for (uint16_t v : samples) {
          // The settings only take values their limits allow; these are the ones this format can show
          if (item.format == Fmt::Logo && v > 6) continue;
          if (item.format == Fmt::HomeView && v >= DOG_HOME_VIEWS) continue;
          if (item.format == Fmt::SleepTime && v > 15) continue;
          if (item.format == Fmt::Minutes && v > 60) continue;
          if (item.format == Fmt::Watts && v > MAX_POWER_LIMIT) continue;
          if (item.format == Fmt::DcCutoff && v > 4) continue;
          setSettingValue(item.option, v);
          formatValue(item, text);
          if (dotsEnd + 2 + dogTextWidth(dogFontLabel, text) > OLED_WIDTH) {
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

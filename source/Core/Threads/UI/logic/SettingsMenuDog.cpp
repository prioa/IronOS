/*
 * TS101_DOG_UI: the flat settings menu, as lists of three rows: name on the left, value on the right. Seven points, one
 * level below them, and every value saves itself when it is left. The rear button always goes back, click or hold. The
 * front button moves on with a click; held, it opens a list, flips a switch or makes the value in the row editable, where a
 * click raises it and wraps round at the top.
 * The position lives in this file, not in the scratch state: the temperature view and the info pages are modes of their own.
 */
#include "OperatingModes.h"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
#ifdef TS101_DOG_UI

enum class Fmt : uint8_t { Action, OnOff, Stage, Temp, BoostTemp, SleepTemp, SleepTime, Minutes, Autostart, Step, Lock, Tip, DcCutoff, PdMode, PdTimeout, Watts, Unit, Rotation, Plain, Animation, Logo, Pulse };

struct DogMenuItem {
  const char     *de, *en;
  const char     *helpDe, *helpEn;
  SettingsOptions option;
  Fmt             format;
  void (*action)(void); // Fmt::Action: runs instead of showing a value
};

// Blocking screens kept from the stock menu, in settingsGUI.cpp
void dogSetCalibrateCJC(void);
void dogSetCalibrateVIN(void);
void dogSetResetSettings(void);

#define OPT(name) SettingsOptions::name
static const DogMenuItem solderItems[] = {
    {"BOOST", "BOOST", "ZIEL, SOLANGE VORNE GEHALTEN WIRD", "TARGET WHILE THE FRONT BUTTON IS HELD", OPT(BoostTemp), Fmt::BoostTemp, nullptr},
    {"AUTOSTART", "AUTOSTART", "WAS BEIM EINSTECKEN PASSIERT", "WHAT HAPPENS WHEN POWERED ON", OPT(AutoStartMode), Fmt::Autostart, nullptr},
    {"SCHRITT", "STEP", "SO VIEL ÄNDERT EIN KLICK DIE TEMPERATUR", "HOW MUCH ONE CLICK CHANGES THE TEMPERATURE", OPT(TempChangeLongStep), Fmt::Step, nullptr},
    {"SPITZE", "TIP", "WELCHE SPITZE STECKT, AUTO ERKENNT SIE SELBST", "WHICH TIP IS FITTED, AUTO DETECTS IT", OPT(SolderingTipType), Fmt::Tip, nullptr},
    {"SPERRE", "LOCK", "BEIDE TASTEN HALTEN SPERRT BEIM LÖTEN", "HOLD BOTH BUTTONS TO LOCK WHILE SOLDERING", OPT(LockingMode), Fmt::Lock, nullptr},
};
static const DogMenuItem sleepItems[] = {
    {"BEWEGUNG", "MOTION", "WIE EMPFINDLICH DER KOLBEN BEWEGUNG BEMERKT", "HOW EASILY THE IRON NOTICES MOVEMENT", OPT(Sensitivity), Fmt::Stage, nullptr},
    {"SCHLAF NACH", "SLEEP AFTER", "ZEIT OHNE BEWEGUNG, BIS DER KOLBEN EINSCHLÄFT", "TIME WITHOUT MOVEMENT UNTIL THE IRON SLEEPS", OPT(SleepTime), Fmt::SleepTime, nullptr},
    {"SCHLAF-TEMP", "SLEEP TEMP", "SO WARM BLEIBT DIE SPITZE IM SCHLAF", "HOW WARM THE TIP STAYS WHILE ASLEEP", OPT(SleepTemp), Fmt::SleepTemp, nullptr},
    {"AUS NACH", "OFF AFTER", "ZEIT OHNE BEWEGUNG, BIS DIE HEIZUNG AUSGEHT", "TIME WITHOUT MOVEMENT UNTIL THE HEATER STOPS", OPT(ShutdownTime), Fmt::Minutes, nullptr},
};
static const DogMenuItem powerItems[] = {
    {"DC-GRENZE", "DC CUTOFF", "AB DIESER AKKUSPANNUNG SCHALTET ER AB", "BATTERY VOLTAGE WHERE THE IRON STOPS", OPT(MinDCVoltageCells), Fmt::DcCutoff, nullptr},
    {"PD-MODUS", "PD MODE", "WIE VIEL LEISTUNG VOM NETZTEIL ANGEFORDERT WIRD", "HOW MUCH POWER IS REQUESTED FROM THE SUPPLY", OPT(USBPDMode), Fmt::PdMode, nullptr},
    {"PD-TIMEOUT", "PD TIMEOUT", "WARTEZEIT AUF DAS PD-NETZTEIL BEIM START", "HOW LONG TO WAIT FOR A PD SUPPLY AT START", OPT(PDNegTimeout), Fmt::PdTimeout, nullptr},
    {"LEISTUNG", "POWER", "HÖCHSTE HEIZLEISTUNG", "HIGHEST HEATING POWER", OPT(PowerLimit), Fmt::Watts, nullptr},
};
static const DogMenuItem displayItems[] = {
    {"EINHEIT", "UNIT", "GRAD CELSIUS ODER FAHRENHEIT", "DEGREES CELSIUS OR FAHRENHEIT", OPT(TemperatureInF), Fmt::Unit, nullptr},
    {"DREHUNG", "ROTATION", "BILD FEST ODER NACH DER LAGE", "FIXED SCREEN OR FOLLOWING THE IRON", OPT(OrientationMode), Fmt::Rotation, nullptr},
    {"HELLIGKEIT", "BRIGHTNESS", "HELLIGKEIT DES DISPLAYS", "BRIGHTNESS OF THE SCREEN", OPT(OLEDBrightness), Fmt::Plain, nullptr},
    {"INVERTIEREN", "INVERT", "HELL UND DUNKEL TAUSCHEN", "SWAP LIGHT AND DARK", OPT(OLEDInversion), Fmt::OnOff, nullptr},
    {"ANIMATION", "ANIMATION", "TEMPO DER MENÜ-SYMBOLE", "SPEED OF THE MENU ICONS", OPT(AnimationSpeed), Fmt::Animation, nullptr},
    {"LOGO", "LOGO", "WIE LANGE DAS BOOT-LOGO LÄUFT", "HOW LONG THE BOOT LOGO RUNS", OPT(LOGOTime), Fmt::Logo, nullptr},
};
static const DogMenuItem systemItems[] = {
    {"TEMP KALIBR.", "CALIBRATE TEMP", "BEIM NÄCHSTEN START, MIT KALTER SPITZE", "ON THE NEXT START, WITH A COLD TIP", OPT(CalibrateCJC), Fmt::OnOff, dogSetCalibrateCJC},
    {"VOLT KALIBR.", "CALIBRATE VOLTS", "ANZEIGE AN EIN MESSGERÄT ANGLEICHEN", "MATCH THE READING TO A METER", SettingsOptionsLength, Fmt::Action, dogSetCalibrateVIN},
    {"KEEP-AWAKE", "KEEP AWAKE", "HÄLT POWERBANKS WACH", "KEEPS POWER BANKS AWAKE", OPT(KeepAwakePulse), Fmt::Pulse, nullptr},
    {"TASTEN TAUSCHEN", "SWAP BUTTONS", "VORNE UND HINTEN TAUSCHEN IHRE ROLLEN", "FRONT AND REAR SWAP THEIR ROLES", OPT(ReverseButtonTempChangeEnabled), Fmt::OnOff, nullptr},
    {"ZURÜCKSETZEN", "RESET", "ALLE EINSTELLUNGEN AUF WERKSZUSTAND", "ALL SETTINGS BACK TO DEFAULTS", SettingsOptionsLength, Fmt::Action, dogSetResetSettings},
};

struct DogMenuSection {
  const char        *de, *en;
  const DogMenuItem *items;
  uint8_t            count;
};
#define ITEMS(list) list, sizeof(list) / sizeof(list[0])
static const DogMenuSection sections[] = {
    {"TEMPERATUR", "TEMPERATURE", nullptr, 0}, // opens the temperature view
    {"INFO", "INFO", nullptr, 0},              // opens the info pages
    {"LÖTEN", "SOLDER", ITEMS(solderItems)},
    {"SCHLAF", "SLEEP", ITEMS(sleepItems)},
    {"STROM", "POWER", ITEMS(powerItems)},
    {"ANZEIGE", "DISPLAY", ITEMS(displayItems)},
    {"SYSTEM", "SYSTEM", ITEMS(systemItems)},
};
static const uint8_t SECTION_COUNT = sizeof(sections) / sizeof(sections[0]);
#define NO_SECTION DOG_MENU_ROOT
#define SAVED_TICKS TICKS_SECOND
#define HELP_TICKS  (TICKS_SECOND * 3)
#define HOLD_REPEAT (TICKS_100MS * 3 / 2)

// Where we are, kept across the temperature view and the info pages
static uint8_t    openSection = NO_SECTION; // the section whose items are listed
static uint8_t    position    = 0;          // in the list
static bool       editing     = false;      // the value of the current item is being changed
static bool       changed     = false;
static TickType_t savedAt     = 0;          // "saved" shows in the key row for a moment
static bool       helpShowing = false;      // the next click only brings the key row back

static inline const char *T(const char *de, const char *en) { return dogLangDE() ? de : en; }

static void formatValue(const DogMenuItem &item, char *out) {
  const uint16_t v = item.option < SettingsOptionsLength ? getSettingValue(item.option) : 0;
  const char *off = T("AUS", "OFF");
  char       *end;
  switch (item.format) {
  case Fmt::OnOff:
    strcpy(out, v ? T("AN", "ON") : off);
    break;
  case Fmt::Stage:
    if (!v) {
      strcpy(out, off);
      break;
    }
    dogFormat(out, v);
    break;
  case Fmt::Temp:
  case Fmt::SleepTemp:
  case Fmt::Step:
    end = dogFormat(out, v);
    strcpy(end, "°");
    break;
  case Fmt::BoostTemp:
    if (!v) {
      strcpy(out, off);
      break;
    }
    end = dogFormat(out, v);
    strcpy(end, "°");
    break;
  case Fmt::SleepTime:
    if (!v) {
      strcpy(out, off);
    } else if (v < 6) {
      end = dogFormat(out, v * 10);
      strcpy(end, " S");
    } else {
      end = dogFormat(out, v - 5);
      strcpy(end, " MIN");
    }
    break;
  case Fmt::Minutes:
    if (!v) {
      strcpy(out, off);
      break;
    }
    end = dogFormat(out, v);
    strcpy(end, " MIN");
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
      break;
    }
    end = dogFormat(out, 2 + v);
    strcpy(end, "S");
    break;
  case Fmt::PdMode: {
    static const char *const de[] = {"FEST", "STANDARD", "SICHER"};
    static const char *const en[] = {"FIXED", "DEFAULT", "SAFE"};
    strcpy(out, T(de[v < 3 ? v : 0], en[v < 3 ? v : 0]));
    break;
  }
  case Fmt::PdTimeout:
    if (!v) {
      strcpy(out, off);
      break;
    }
    end    = dogFormat(out, v / 10);
    *end++ = '.';
    *end++ = '0' + v % 10;
    strcpy(end, " S");
    break;
  case Fmt::Watts:
    if (!v) {
      strcpy(out, off);
      break;
    }
    end = dogFormat(out, v);
    strcpy(end, " W");
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
    if (v == logoMode_t::SKIP) {
      strcpy(out, off);
    } else if (v == logoMode_t::ONETIME) {
      strcpy(out, T("EINMAL", "ONCE"));
    } else if (v == logoMode_t::INFINITY) {
      strcpy(out, T("IMMER", "ALWAYS"));
    } else {
      end = dogFormat(out, v);
      strcpy(end, " S");
    }
    break;
  case Fmt::Pulse:
    if (!v) {
      strcpy(out, off);
      break;
    }
    end    = dogFormat(out, v / 10);
    *end++ = '.';
    *end++ = '0' + v % 10;
    strcpy(end, " W");
    break;
  case Fmt::Plain:
    dogFormat(out, v);
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
  case Fmt::Animation:
    setSettingValue(SettingsOptions::AnimationLoop, getSettingValue(item.option) ? 1 : 0); // the speed and the loop are one point now
    break;
  default:
    break;
  }
}

// ---- Drawing ----
#define ROWS        3
#define ROW_HEIGHT  8
#define VALUE_RIGHT 124 // values end here, the scroll bar is on column 127
static uint8_t top = 0; // the first row on screen

static bool isSwitch(const DogMenuItem &item) { return item.format == Fmt::OnOff || item.format == Fmt::Unit; }

// 13x7 switch: off is an outline with the knob on the left, on is filled with the knob on the right
static void drawSwitch(int16_t x1, int16_t y, bool on) {
  const int16_t x0 = x1 - 12;
  OLED::drawFilledRect(x0 + 1, y, x1 - 1, y, false);
  OLED::drawFilledRect(x0 + 1, y + 6, x1 - 1, y + 6, false);
  OLED::drawFilledRect(x0, y + 1, x0, y + 5, false);
  OLED::drawFilledRect(x1, y + 1, x1, y + 5, false);
  if (on) {
    OLED::drawFilledRect(x0 + 1, y + 1, x1 - 1, y + 5, false);
    OLED::drawFilledRect(x1 - 5, y + 2, x1 - 2, y + 4, true);
  } else {
    OLED::drawFilledRect(x0 + 2, y + 2, x0 + 5, y + 4, false);
  }
}

static uint8_t listCount(uint8_t section) { return section == NO_SECTION ? SECTION_COUNT : sections[section].count; }

static void drawRow(uint8_t section, uint8_t index, uint8_t row, bool selected, bool editingValue) {
  const int16_t base = row * ROW_HEIGHT + 7;
  const int16_t y0   = row * ROW_HEIGHT;
  if (section == NO_SECTION) {
    dogSmall(2, T(sections[index].de, sections[index].en), base);
    if (index == 0) {
      char  text[8];
      char *end = dogFormat(text, getSettingValue(SettingsOptions::SolderingTemp));
      strcpy(end, "°");
      dogSmallRight(VALUE_RIGHT, text, base);
    } else {
      dogBlit(VALUE_RIGHT - 7, y0, 8, 8, iconNext);
    }
  } else {
    const DogMenuItem &item = sections[section].items[index];
    dogSmall(2, T(item.de, item.en), base);
    if (item.format == Fmt::Action) {
      dogBlit(VALUE_RIGHT - 7, y0, 8, 8, iconNext);
    } else if (isSwitch(item) && item.format == Fmt::OnOff) {
      drawSwitch(VALUE_RIGHT, y0 + 1, getSettingValue(item.option));
    } else {
      char value[16];
      formatValue(item, value);
      dogSmallRight(VALUE_RIGHT, value, base);
      if (editingValue) {
        // Only the value is inverted while it changes, and it blinks so the row reads as "being changed"
        if ((xTaskGetTickCount() / (TICKS_100MS * 4)) % 3) {
          OLED::invertRect(VALUE_RIGHT - dogTextWidth(dogFontSmall, value) - 2, y0 + 1, VALUE_RIGHT + 1, y0 + 7);
        }
        return;
      }
    }
  }
  if (selected) {
    OLED::invertRect(0, y0 + 1, VALUE_RIGHT + 1, y0 + 7);
  }
}

static void drawList(uint8_t section, uint8_t pos, bool editingValue) {
  const uint8_t count = listCount(section);
  if (pos < top) {
    top = pos;
  } else if (pos >= top + ROWS) {
    top = pos - ROWS + 1;
  }
  for (uint8_t row = 0; row < ROWS && top + row < count; row++) {
    drawRow(section, top + row, row, top + row == pos, editingValue && top + row == pos);
  }
  // Scroll bar: where the three rows are in the whole list
  const uint8_t span = 23;
  const uint8_t len  = count > ROWS ? span * ROWS / count : span;
  const uint8_t y    = count > ROWS ? (span - len) * top / (count - ROWS) : 0;
  OLED::drawFilledRect(OLED_WIDTH - 1, y, OLED_WIDTH - 1, y + len - 1, false);
}

static const char *listTitle(uint8_t section) { return section == NO_SECTION ? T("MENÜ", "MENU") : T(sections[section].de, sections[section].en); }

// The key row: back and next as icons, the name of the list between them; or the help text, or "saved"
static void drawListKeys(uint8_t section, const DogMenuItem *item) {
  const char *holdDe = "HALTEN: ÖFFNEN", *holdEn = "HOLD: OPEN";
  if (item && item->format != Fmt::Action) {
    holdDe = isSwitch(*item) ? "HALTEN: UMSCHALTEN" : "HALTEN: ÄNDERN";
    holdEn = isSwitch(*item) ? "HOLD: SWITCH" : "HOLD: CHANGE";
  }
  if (dogDrawHold(T("HALTEN: ZURÜCK", "HOLD: BACK"), false, DOG_HOLD_FULL) || dogDrawHold(T(holdDe, holdEn), true, DOG_HOLD_FULL)) {
    return;
  }
  const TickType_t now = xTaskGetTickCount();
  helpShowing          = item && now - lastButtonTime > HELP_TICKS && !dogPressed(true) && !dogPressed(false);
  if (savedAt && now - savedAt < SAVED_TICKS) {
    const char   *saved = T("GESPEICHERT", "SAVED");
    const uint8_t w     = dogTextWidth(dogFontSmall, saved) + 9;
    const int16_t x     = (OLED_WIDTH - w) / 2;
    OLED::drawArea(x, 24, 8, 8, iconCheck);
    dogSmall(x + 9, saved);
  } else if (helpShowing) {
    // The explanation runs between the icons until a button is pressed
    const char   *text  = T(item->helpDe, item->helpEn);
    const int16_t width = dogTextWidth(dogFontSmall, text) + 32;
    dogSmall(OLED_WIDTH - 10 - (int16_t)(((now - lastButtonTime - HELP_TICKS) / (TICKS_100MS / 2)) % (width + OLED_WIDTH - 20)), text);
    OLED::drawFilledRect(0, 23, 9, 31, true);
    OLED::drawFilledRect(OLED_WIDTH - 10, 23, OLED_WIDTH - 1, 31, true);
  } else {
    dogSmallCentre(10, OLED_WIDTH - 11, listTitle(section));
  }
  dogDrawCorners(iconNext, iconBack);
}

void dogDrawMenuList(uint8_t section, uint8_t pos) {
  drawList(section, pos, false);
  drawListKeys(section, section == NO_SECTION ? nullptr : &sections[section].items[pos]);
}

void dogDrawMenuValue(uint8_t section, uint8_t pos) {
  drawList(section, pos, true);
  if (dogDrawHold(T("HALTEN: FERTIG", "HOLD: DONE"), false, DOG_HOLD_FULL)) {
    return;
  }
  dogDrawCorners(iconPlus, iconBack);
  // Runs down over the 2 s until the value saves itself
  const TickType_t idle = xTaskGetTickCount() - lastButtonTime;
  const uint8_t    fill = idle >= DOG_ADJUST_TIMEOUT ? 0 : (44 * (DOG_ADJUST_TIMEOUT - idle)) / DOG_ADJUST_TIMEOUT;
  OLED::drawFilledRect(40, 26, 88, 26, false);
  OLED::drawFilledRect(40, 29, 88, 29, false);
  OLED::drawFilledRect(40, 26, 40, 29, false);
  OLED::drawFilledRect(88, 26, 88, 29, false);
  if (fill) {
    OLED::drawFilledRect(42, 27, 42 + fill, 28, false);
  }
}

#ifdef BUTTON_TEST
// Every name and the widest value its setting can show fit in one row; returns the first row that does not, or nullptr
const char *dogMenuRowOverflow() {
  static const uint16_t samples[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 50, 99, 100, 120, 180, 300, 450, 850};
  for (uint8_t lang = 0; lang < 2; lang++) {
    for (uint8_t sct = 2; sct < SECTION_COUNT; sct++) {
      for (uint8_t i = 0; i < sections[sct].count; i++) {
        const DogMenuItem &item  = sections[sct].items[i];
        const char        *label = lang ? item.en : item.de;
        uint8_t            widest = 13;
        if (item.format != Fmt::OnOff && item.format != Fmt::Action) {
          const uint16_t saved = getSettingValue(item.option);
          for (uint16_t v : samples) {
            setSettingValue(item.option, v);
            char value[16];
            formatValue(item, value);
            const uint8_t w = dogTextWidth(dogFontSmall, value);
            widest          = w > widest ? w : widest;
          }
          setSettingValue(item.option, saved);
        }
        if (2 + dogTextWidth(dogFontSmall, label) + 5 + widest > VALUE_RIGHT) {
          return label;
        }
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
    savedAt = xTaskGetTickCount() | 1;
  }
}

static OperatingMode editValue(const ButtonState buttons, guiContext *cxt) {
  const DogMenuItem &item = sections[openSection].items[position];
  dogDrawMenuValue(openSection, position);
  if (buttons == BUTTON_B_SHORT || buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    leaveValue();
    return OperatingMode::SettingsMenu;
  }
  if (dogHoldDone(true, DOG_HOLD_FULL)) {
    // Faster +: repeats while held, stops at the top rather than wrapping round to the bottom
    if (xTaskGetTickCount() - cxt->scratch_state.state3 >= HOLD_REPEAT && !isLastSettingValue(item.option)) {
      cxt->scratch_state.state3 = xTaskGetTickCount();
      stepValue(item, true);
      changed = true;
    }
    return OperatingMode::SettingsMenu;
  }
  if (buttons == BUTTON_F_SHORT) {
    stepValue(item, true);
    changed = true;
  }
  if (xTaskGetTickCount() - lastButtonTime > DOG_ADJUST_TIMEOUT) {
    leaveValue();
  }
  return OperatingMode::SettingsMenu;
}

static OperatingMode leaveList(guiContext *cxt) {
  if (openSection == NO_SECTION) {
    cxt->transitionMode = TransitionAnimation::Left;
    return OperatingMode::HomeScreen;
  }
  position            = openSection; // back on the point we came from
  openSection         = NO_SECTION;
  top                 = position >= ROWS ? position - ROWS + 1 : 0;
  cxt->transitionMode = TransitionAnimation::Left;
  return OperatingMode::SettingsMenu;
}

OperatingMode gui_SettingsMenu(const ButtonState buttonsIn, guiContext *cxt) {
  // A fresh visit from the home screen starts at the top; coming back from the temperature view or the info pages does not
  if (!cxt->scratch_state.state1) {
    cxt->scratch_state.state1 = 1;
    if (cxt->previousMode == OperatingMode::HomeScreen) {
      openSection = NO_SECTION;
      position    = 0;
      top         = 0;
      editing     = false;
      savedAt     = 0;
    }
  }
  if (editing) {
    return editValue(buttonsIn, cxt);
  }
  dogDrawMenuList(openSection, position);

  ButtonState buttons = buttonsIn;
  if (helpShowing && (buttons == BUTTON_F_SHORT || buttons == BUTTON_B_SHORT)) {
    helpShowing = false;
    buttons     = BUTTON_NONE; // the key row wasn't showing, so this click can't have meant what it does
  }
  if (buttons == BUTTON_B_SHORT || buttons == BUTTON_BOTH || dogHoldDone(false, DOG_HOLD_FULL)) {
    dogPressConsumed();
    return leaveList(cxt);
  }
  const uint8_t count = openSection == NO_SECTION ? SECTION_COUNT : sections[openSection].count;
  if (buttons == BUTTON_F_SHORT) {
    position            = (position + 1) % count;
    cxt->transitionMode = TransitionAnimation::Down;
  } else if (dogHoldDone(true, DOG_HOLD_FULL)) {
    dogPressConsumed();
    if (openSection == NO_SECTION) {
      if (position == 0) {
        cxt->transitionMode = TransitionAnimation::Left;
        return OperatingMode::TemperatureAdjust;
      }
      if (position == 1) {
        cxt->transitionMode = TransitionAnimation::Left;
        return OperatingMode::DebugMenuReadout;
      }
      openSection         = position;
      position            = 0;
      top                 = 0;
      cxt->transitionMode = TransitionAnimation::Left;
    } else {
      const DogMenuItem &item = sections[openSection].items[position];
      if (item.action) {
        item.action();
        saveSettings();
      } else if (isSwitch(item)) {
        stepValue(item, true);
        saveSettings();
        savedAt = xTaskGetTickCount() | 1;
      } else {
        editing = true;
        changed = false;
      }
    }
  }
  return OperatingMode::SettingsMenu;
}
#endif

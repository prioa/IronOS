// Host test of the TS101_DOG_UI button logic: the real modes, getButtonState and the key row, driven through the pins
// over simulated time. Checks rule 2 (holding the rear button leaves) on every screen and upstream issue #1663.
#define private public
#include "OLED.hpp"
#undef private
#include "OperatingModes.h"
#include "preview_state.hpp"
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
#include <stdio.h>

void testSetPins(bool front, bool rear);
void testSetHandle(int16_t x10C);
void testSetTip(uint16_t c);
const char *dogMenuRowOverflow();
extern volatile TemperatureType_t currentTempTargetDegC;
extern TickType_t                 lastMovementTime;

TickType_t     previewTicks = 1000000;
static uint8_t frame[FRAMEBUFFER_START + OLED_WIDTH * OLED_HEIGHT / 8];
OperatingMode  mode = OperatingMode::HomeScreen;
guiContext     cxt;
static int     failures = 0;
static bool    animated = false; // a screen transition was asked for since the last check

static const char *name(OperatingMode m) {
  switch (m) {
  case OperatingMode::HomeScreen: return "Start";
  case OperatingMode::Soldering: return "Löten";
  case OperatingMode::Sleeping: return "Schlaf";
  case OperatingMode::SettingsMenu: return "Menü";
  case OperatingMode::DebugMenuReadout: return "Info";
  case OperatingMode::TemperatureAdjust: return "Temperatur";
  default: return "anderer";
  }
}

// One pass of the GUI thread, like guiHandleDraw + guiRenderLoop without the drawing out
static void step(bool front, bool rear, uint32_t ms = 30) {
  for (uint32_t t = 0; t < ms; t += 30) {
    previewTicks += 30;
    testSetPins(front, rear);
    OLED::clearScreen();
    ButtonState   buttons = dogButtonsPoll(getButtonState());
    OperatingMode next    = mode;
    switch (mode) {
    case OperatingMode::HomeScreen: next = drawHomeScreen(buttons, &cxt); break;
    case OperatingMode::Soldering: cxt.scratch_state.state4 = 0; next = gui_solderingMode(buttons, &cxt); break;
    case OperatingMode::Sleeping: next = gui_SolderingSleepingMode(buttons, &cxt); break;
    case OperatingMode::SettingsMenu: next = gui_SettingsMenu(buttons, &cxt); break;
    case OperatingMode::DebugMenuReadout: next = showDebugMenu(buttons, &cxt); break;
    case OperatingMode::TemperatureAdjust: next = gui_solderingTempAdjust(buttons, &cxt); break;
    default: break;
    }
    if (next != mode) {
      cxt.viewEnterTime  = previewTicks;
      cxt.previousMode   = mode;
      memset(&cxt.scratch_state, 0, sizeof(cxt.scratch_state));
      mode = next;
    }
    animated |= cxt.transitionMode != TransitionAnimation::None;
    cxt.transitionMode = TransitionAnimation::None;
  }
}
static void idle(uint32_t ms) { step(false, false, ms); }
static void click(bool front) { step(front, !front, 120); idle(150); }
static void hold(bool front, uint32_t ms) { step(front, !front, ms); idle(150); }

static void expect(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok    " : "FEHLER", what);
  failures += !ok;
}
static void expectMode(OperatingMode m, const char *what) {
  char text[200];
  snprintf(text, sizeof text, "%s (ist: %s)", what, name(mode));
  expect(mode == m, text);
}
static uint16_t setPoint() { return getSettingValue(SettingsOptions::SolderingTemp); }
static void     moved() { lastMovementTime = previewTicks; }

static void toHome() {
  for (int i = 0; i < 6 && mode != OperatingMode::HomeScreen; i++) {
    hold(false, 1300);
  }
  idle(300);
}
static void startSoldering() {
  toHome();
  click(true);
  moved();
  idle(300);
}

int main() {
  OLED::setFramebuffer(frame);
  previewSetup();
  setSettingValue(SettingsOptions::SolderingTemp, 320);
  setSettingValue(SettingsOptions::Sensitivity, 6);
  setSettingValue(SettingsOptions::SleepTime, 6); // 1 minute
  setSettingValue(SettingsOptions::TempChangeLongStep, 10);
  setSettingValue(SettingsOptions::BoostTemp, 420);
  setSettingValue(SettingsOptions::LockingMode, 0);
  moved();
  idle(500);

  puts("== Start");
  hold(false, 800);
  expectMode(OperatingMode::DebugMenuReadout, "Halten hinten am Start öffnet Info");
  hold(false, 800);
  expectMode(OperatingMode::HomeScreen, "Halten hinten führt zurück zum Start");
  click(true);
  expectMode(OperatingMode::Soldering, "Klick vorne startet das Löten");
  moved();

  puts("== Löten, Regel 2 und Abbruch");
  hold(false, 450);
  expectMode(OperatingMode::Soldering, "Halten hinten 0,45 s und loslassen bricht ab, Heizung bleibt an");
  expect(currentTempTargetDegC == 320, "Heizung zielt weiter auf 320");
  hold(false, 600);
  expectMode(OperatingMode::HomeScreen, "Halten hinten 0,6 s stoppt");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  idle(500);
  expectMode(OperatingMode::HomeScreen, "Loslassen nach dem Stopp startet nichts");

  puts("== Löten, beide Tasten stoppen sofort");
  startSoldering();
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten: nach 60 ms Stopp, ohne loszulassen");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  idle(300);
  expectMode(OperatingMode::HomeScreen, "Loslassen danach startet nichts");
  startSoldering();
  step(true, false, 1000);
  expect(currentTempTargetDegC == 420, "Boost läuft");
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten stoppen auch im Boost");
  idle(300);
  startSoldering();
  click(true);
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten stoppen auch bei offener Temperatur");
  idle(300);

  puts("== Sperre am Start");
  setSettingValue(SettingsOptions::LockingMode, 0);
  step(true, true, 800);
  idle(300);
  expect(!dogLockArmed, "SPERRE aus im Menü: beide halten am Start tut nichts");
  setSettingValue(SettingsOptions::LockingMode, 2);
  step(true, true, 800);
  idle(300);
  expect(dogLockArmed, "SPERRE an: beide halten am Start sperrt");
  expectMode(OperatingMode::HomeScreen, "und bleibt am Start");
  idle(1600);
  startSoldering();
  const uint16_t lockedSet = setPoint();
  click(true);
  click(false);
  hold(false, 800);
  expectMode(OperatingMode::Soldering, "gesperrt: Tasten stoppen und öffnen nichts");
  expect(setPoint() == lockedSet, "gesperrt: Solltemperatur bleibt");
  idle(1200);
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten stoppen auch gesperrt");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  idle(300);
  step(true, true, 800);
  idle(1800);
  expect(!dogLockArmed, "am Start beide halten löst die Sperre");
  setSettingValue(SettingsOptions::LockingMode, 0);

  puts("== Löten, Klicks ändern das Heizen nicht");
  startSoldering();
  const uint16_t before = setPoint();
  click(true);
  expect(setPoint() == before, "Klick vorne öffnet die Temperatur und ändert nichts");
  expect(currentTempTargetDegC == before, "Heizung läuft weiter, während die Temperatur offen ist");
  click(true);
  click(true);
  expect(setPoint() == before + 10, "zwei weitere Klicks vorne: +10 in 5er-Schritten");
  click(false);
  expect(setPoint() == before + 5, "Klick hinten: -5");
  expect(currentTempTargetDegC == before + 5, "Heizung folgt dem neuen Sollwert");
  hold(true, 3000);
  expect(setPoint() == 350, "Halten vorne 3 s springt genau ein Preset weiter (325 auf 350)");
  hold(false, 1000);
  expectMode(OperatingMode::Soldering, "Halten hinten schließt die Temperatur, das Löten läuft weiter");
  const uint16_t openedByRear = setPoint();
  click(false);
  expect(setPoint() == openedByRear, "Klick hinten beim Löten öffnet die Temperatur und ändert nichts");
  click(false);
  expect(setPoint() == openedByRear - 5, "der nächste Klick hinten ist -5, sie ist also offen");
  idle(2500);
  click(false);
  expect(setPoint() == openedByRear - 5, "die Temperatur schließt nach 2 s von selbst");
  idle(2500);

  puts("== Boost");
  setSettingValue(SettingsOptions::SolderingTemp, 320);
  startSoldering();
  step(true, false, 1500);
  expect(currentTempTargetDegC == 420, "Halten vorne boostet auf 420");
  expect(dogBoostSecondsLeft() <= 30 && dogBoostSecondsLeft() >= 28, "Restzeit läuft von 30 s");
  moved();
  step(true, false, 20000);
  expect(currentTempTargetDegC == 420, "nach 21,5 s läuft er noch");
  moved();
  step(true, false, 9000);
  expect(currentTempTargetDegC == 320, "nach 30,5 s ist Schluss, Taste noch unten");
  moved();
  step(true, false, 3000);
  expect(currentTempTargetDegC == 320, "er startet nicht neu, solange die Taste unten bleibt");
  expectMode(OperatingMode::Soldering, "das Löten läuft weiter");
  idle(1000);
  moved();
  step(true, false, 1500);
  expect(currentTempTargetDegC == 420, "nach dem Loslassen geht er wieder");

  puts("== #1663: direkt nach dem Boost");
  idle(60);
  expect(currentTempTargetDegC == 320, "Loslassen beendet den Boost");
  click(true);
  click(true);
  expect(setPoint() == 320, "Klicks kurz nach dem Boost ändern nichts");
  idle(800);
  click(true);
  click(true);
  expect(setPoint() == 325, "später öffnet ein Klick wieder die Anzeige und der zweite erhöht");
  idle(2500);
  setSettingValue(SettingsOptions::SolderingTemp, 320);

  puts("== Temperatur vom Start aus");
  toHome();
  hold(true, 800);
  expectMode(OperatingMode::TemperatureAdjust, "Halten vorne am Start öffnet die Temperatur");
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten: fertig, zurück zum Start");

  puts("== Menü: hinten nächste Zeile, vorne öffnet, hinten halten zurück");
  click(false);
  expectMode(OperatingMode::SettingsMenu, "Klick hinten am Start öffnet das Menü");
  animated = false;
  click(false);
  expect(!animated, "Klick hinten im Menü: nur die Markierung wandert, keine Animation");
  expectMode(OperatingMode::SettingsMenu, "und bleibt im Menü");
  click(true);
  expectMode(OperatingMode::DebugMenuReadout, "Klick vorne auf Info öffnet die Info-Seiten");

  puts("== Info: vorne vor, hinten zurück, hinten halten raus");
  for (int i = 0; i < 4; i++) {
    click(true);
  }
  hold(true, 800);
  click(false);
  expectMode(OperatingMode::DebugMenuReadout, "System: Halten vorne öffnet Debug, Klick hinten blättert dort");
  hold(false, 800);
  expectMode(OperatingMode::DebugMenuReadout, "Halten hinten: zurück auf die Seite");
  click(false);
  expectMode(OperatingMode::DebugMenuReadout, "Klick hinten: vorige Seite, bleibt in Info");
  hold(false, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten verlässt die Info-Seiten zum Menü");
  hold(true, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten vorne in der Liste tut nichts");

  for (int i = 0; i < 6; i++) {
    click(false); // Löten ... System, dann wieder Temperatur
  }
  click(true);
  expectMode(OperatingMode::TemperatureAdjust, "Menü, Temperatur: Klick vorne öffnet die Temperatur");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten führt zurück ins Menü");
  click(false);
  click(false);
  click(false);
  animated = false;
  click(true); // Schlaf
  expect(animated, "Öffnen eines Bereichs schiebt weiterhin");
  click(false);
  const uint16_t sleepBefore = getSettingValue(SettingsOptions::SleepTime);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Klick hinten in der Liste ändert nichts");
  click(true); // Schlaf nach: ändern
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Klick vorne öffnet den Wert, ohne ihn zu ändern");
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "Wert: Klick vorne erhöht");
  click(false);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Wert: Klick hinten verringert");
  click(true);
  hold(false, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten: Wert fertig");
  click(false);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "danach ist Klick hinten wieder nächste Zeile");
  click(false);
  click(false);
  click(false); // vier Punkte: zurück auf Schlaf nach
  click(true);
  click(true);
  const uint16_t autoSaved = getSettingValue(SettingsOptions::SleepTime);
  idle(4000);
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == autoSaved + 1, "nach 4 s ist der Wert noch offen");
  idle(5000);
  click(false);
  expect(getSettingValue(SettingsOptions::SleepTime) == autoSaved + 1, "nach 5 s speichert er selbst, Klick hinten ist wieder nächste Zeile");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten im Bereich: eine Ebene zurück");
  click(false);
  click(false); // Anzeige
  click(true);
  click(false);
  click(false);
  click(false); // Invertieren
  const uint16_t inverted = getSettingValue(SettingsOptions::OLEDInversion);
  click(true);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == !inverted, "Schalter: Klick vorne schaltet sofort um");
  expectMode(OperatingMode::SettingsMenu, "und bleibt in der Liste");
  click(true);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == inverted, "nochmal Klick schaltet zurück");
  click(false);
  click(false);
  click(false); // Start mit Hund
  click(true);
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == 1, "Menü: Start mit Hund schaltet");
  click(true);
  click(false); // Detailansicht
  click(true);
  expect(getSettingValue(SettingsOptions::DogSolderView) == 1, "Menü: Detailansicht schaltet");
  click(true);
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten im Bereich: eine Ebene zurück");
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten im Menü: zurück zum Start");
  setSettingValue(SettingsOptions::SleepTime, 6);

  puts("== Schlaf");
  auto toSleep = [] {
    startSoldering();
    lastMovementTime = previewTicks - 70000;
    lastButtonTime   = previewTicks - 70000;
    idle(300);
  };
  toSleep();
  expectMode(OperatingMode::Sleeping, "nach einer Minute ohne Bewegung schläft der Kolben");
  const uint16_t sleepSet = setPoint();
  hold(false, 800);
  expectMode(OperatingMode::HomeScreen, "Halten hinten im Schlaf: zum Start");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  toSleep();
  step(false, true, 200);
  moved();
  step(false, true, 500);
  idle(300);
  expectMode(OperatingMode::HomeScreen, "auch wenn er sich beim Halten bewegt: zum Start");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  toSleep();
  click(false);
  idle(100);
  expectMode(OperatingMode::Soldering, "Klick hinten weckt");
  click(false);
  expect(setPoint() == sleepSet, "der Klick nach dem Wecken öffnet die Temperatur, der weckende tat nichts");
  idle(2500);
  toSleep();
  click(true);
  idle(100);
  expectMode(OperatingMode::Soldering, "Klick vorne weckt");
  expect(setPoint() == sleepSet, "und ändert nichts");
  toSleep();
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten im Schlaf: zum Start");
  idle(300);

  puts("== Griff zu heiß");
  startSoldering();
  testSetHandle(720);
  idle(2500);
  expectMode(OperatingMode::HomeScreen, "Griff über 70 °C für 2 s: zurück zum Start");
  expect(currentTempTargetDegC == 0, "Heizung aus");
  click(true);
  expectMode(OperatingMode::HomeScreen, "solange der Griff heiß ist, startet Klick vorne nicht");
  expect(dogAlert == DogAlert::Handle, "die Warnung bleibt stehen");
  testSetHandle(400);
  click(false);
  expect(dogAlert == DogAlert::None, "abgekühlt: ein Klick schließt die Warnung");
  expectMode(OperatingMode::HomeScreen, "und öffnet dabei nicht das Menü");
  testSetHandle(310);

  puts("== Menüzeilen");
  setSettingValue(SettingsOptions::SleepTime, 6);
  const char *tooWide = dogMenuRowOverflow();
  expect(tooWide == nullptr, tooWide ? tooWide : "jeder Name passt mit dem breitesten Wert in eine Zeile (DE und EN)");

  printf("\n%d Fehler\n", failures);
  return failures ? 1 : 0;
}

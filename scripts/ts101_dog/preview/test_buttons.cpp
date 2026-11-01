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
  hold(false, 2000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten tut am Start nichts");
  click(true);
  expectMode(OperatingMode::Soldering, "Klick vorne startet das Löten");
  moved();

  puts("== Löten, Regel 2 und Abbruch");
  hold(false, 500);
  expectMode(OperatingMode::Soldering, "Halten hinten 0,5 s und loslassen bricht ab, Heizung bleibt an");
  expect(currentTempTargetDegC == 320, "Heizung zielt weiter auf 320");
  hold(false, 1300);
  expectMode(OperatingMode::HomeScreen, "Halten hinten 1,3 s stoppt");
  idle(500);
  expectMode(OperatingMode::HomeScreen, "Loslassen nach dem Stopp startet nichts");

  puts("== Löten, Klicks ändern das Heizen nicht");
  startSoldering();
  const uint16_t before = setPoint();
  click(true);
  expect(setPoint() == before, "Klick vorne ändert die Solltemperatur nicht");
  expect(currentTempTargetDegC == before, "Heizung läuft weiter, während die Temperaturanzeige offen ist");
  click(true);
  click(true);
  expect(setPoint() == before + 20, "zwei weitere Klicks vorne: +20");
  click(false);
  expect(setPoint() == before + 10, "Klick hinten: -10");
  expect(currentTempTargetDegC == before + 10, "Heizung folgt dem neuen Sollwert");
  hold(true, 3000);
  expect(setPoint() == 350, "Halten vorne 3 s springt genau ein Preset weiter (330 auf 350)");
  hold(false, 1000);
  expectMode(OperatingMode::Soldering, "Halten hinten schließt die Temperaturanzeige, das Löten läuft weiter");
  click(false);
  expect(getSettingValue(SettingsOptions::DogSolderView) == 1, "danach wechselt Klick hinten wieder die Ansicht");
  click(true);
  idle(2500);
  click(false);
  expect(getSettingValue(SettingsOptions::DogSolderView) == 0, "die Temperaturanzeige schließt nach 2 s von selbst");

  puts("== #1663: direkt nach dem Boost");
  setSettingValue(SettingsOptions::SolderingTemp, 320);
  startSoldering();
  step(true, false, 1500);
  expect(currentTempTargetDegC == 420, "Halten vorne boostet auf 420");
  idle(60);
  expect(currentTempTargetDegC == 320, "Loslassen beendet den Boost");
  click(true);
  click(true);
  expect(setPoint() == 320, "Klicks kurz nach dem Boost ändern nichts");
  idle(800);
  click(true);
  click(true);
  expect(setPoint() == 330, "später öffnet ein Klick wieder die Anzeige und der zweite erhöht");
  idle(2500);

  puts("== Temperatur vom Start aus");
  toHome();
  hold(true, 800);
  expectMode(OperatingMode::TemperatureAdjust, "Halten vorne am Start öffnet die Temperatur");
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten: fertig, zurück zum Start");

  puts("== Menü: hinten immer zurück, vorne blättern, vorne halten öffnet");
  click(false);
  expectMode(OperatingMode::SettingsMenu, "Klick hinten am Start öffnet das Menü");
  animated = false;
  click(true);
  expect(!animated, "Klick vorne im Menü: nur die Markierung wandert, keine Bildschirm-Animation");
  click(false);
  click(false);
  click(true);
  hold(true, 800);
  expectMode(OperatingMode::DebugMenuReadout, "Klick vorne zu Info, Halten vorne öffnet die Info-Seiten");
  for (int i = 0; i < 4; i++) {
    click(true);
  }
  hold(true, 800);
  click(false);
  expectMode(OperatingMode::DebugMenuReadout, "System: Halten vorne öffnet Debug, Klick hinten führt zurück zur Seite");
  click(false);
  expectMode(OperatingMode::SettingsMenu, "Klick hinten verlässt die Info-Seiten zum Menü");
  hold(true, 800);
  expectMode(OperatingMode::DebugMenuReadout, "das Menü steht danach wieder auf Info");
  click(false);
  for (int i = 0; i < 6; i++) {
    click(true); // Löten ... System, dann wieder Temperatur
  }
  hold(true, 800);
  expectMode(OperatingMode::TemperatureAdjust, "Menü, Temperatur: Halten vorne öffnet die Temperatur");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten führt zurück ins Menü");
  click(true);
  click(true);
  click(true);
  animated = false;
  hold(true, 800); // Schlaf
  expect(animated, "Öffnen eines Bereichs schiebt weiterhin");
  click(true);
  const uint16_t sleepBefore = getSettingValue(SettingsOptions::SleepTime);
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Klick vorne in der Liste blättert und ändert nichts");
  click(true);
  click(true);
  click(true); // vier Punkte: einmal herum zurück auf Schlaf nach
  hold(true, 800); // Schlaf nach: ändern
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Halten vorne öffnet den Wert, ohne ihn zu ändern");
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "Wert: Klick vorne erhöht");
  click(false);
  expectMode(OperatingMode::SettingsMenu, "Klick hinten: Wert fertig");
  click(false);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "Klick hinten nach dem Wert verringert nichts");
  expectMode(OperatingMode::SettingsMenu, "Klick hinten verlässt den Bereich");
  click(false);
  expectMode(OperatingMode::HomeScreen, "Klick hinten verlässt das Menü");
  click(false);
  for (int i = 0; i < 5; i++) {
    click(true); // Anzeige
  }
  hold(true, 800);
  click(true);
  click(true);
  click(true); // Invertieren
  const uint16_t inverted = getSettingValue(SettingsOptions::OLEDInversion);
  hold(true, 800);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == !inverted, "Schalter: Halten vorne schaltet sofort um");
  expectMode(OperatingMode::SettingsMenu, "und bleibt in der Liste");
  hold(true, 800);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == inverted, "nochmal Halten schaltet zurück");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten im Bereich: eine Ebene zurück");
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten im Menü: zurück zum Start");
  setSettingValue(SettingsOptions::SleepTime, 6);

  puts("== Schlaf");
  startSoldering();
  lastMovementTime = previewTicks - 70000;
  lastButtonTime   = previewTicks - 70000;
  idle(300);
  expectMode(OperatingMode::Sleeping, "nach einer Minute ohne Bewegung schläft der Kolben");
  hold(false, 2000);
  expectMode(OperatingMode::Soldering, "Halten hinten weckt nur und stoppt nicht");
  lastMovementTime = previewTicks - 70000;
  lastButtonTime   = previewTicks - 70000;
  idle(300);
  click(true);
  click(false);
  expect(getSettingValue(SettingsOptions::DogSolderView) == 1, "nach dem Aufwecken mit Klick vorne wechselt Klick hinten die Ansicht");
  click(false);

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

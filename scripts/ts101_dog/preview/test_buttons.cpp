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
extern volatile TemperatureType_t currentTempTargetDegC;
extern TickType_t                 lastMovementTime;

TickType_t     previewTicks = 1000000;
static uint8_t frame[FRAMEBUFFER_START + OLED_WIDTH * OLED_HEIGHT / 8];
OperatingMode  mode = OperatingMode::HomeScreen;
guiContext     cxt;
static int     failures = 0;

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

  puts("== Menü");
  click(false);
  expectMode(OperatingMode::SettingsMenu, "Klick hinten am Start öffnet das Menü");
  click(false);
  click(true);
  expectMode(OperatingMode::DebugMenuReadout, "Menü, Info: Klick vorne öffnet die Info-Seiten");
  for (int i = 0; i < 4; i++) {
    click(false);
  }
  click(true);
  hold(false, 1000);
  expectMode(OperatingMode::DebugMenuReadout, "Halten hinten verlässt die Debug-Werte zur System-Seite");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten verlässt die Info-Seiten zum Menü");
  click(true);
  expectMode(OperatingMode::DebugMenuReadout, "das Menü steht danach wieder auf Info");
  hold(false, 1000);
  for (int i = 0; i < 7; i++) {
    click(false); // Löten ... System, Zurück, dann wieder Temperatur
  }
  click(true);
  expectMode(OperatingMode::TemperatureAdjust, "Menü, Temperatur öffnet die Temperatur");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten führt zurück ins Menü");
  click(false);
  click(false);
  click(false);
  click(true); // Schlaf
  click(false);
  click(true); // Schlaf nach: ändern
  const uint16_t sleepBefore = getSettingValue(SettingsOptions::SleepTime);
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "Wert: Klick vorne erhöht");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten verlässt den Wert");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten verlässt den Bereich");
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "Halten hinten verlässt das Menü");
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

  printf("\n%d Fehler\n", failures);
  return failures ? 1 : 0;
}

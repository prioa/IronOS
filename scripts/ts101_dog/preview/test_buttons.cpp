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
  const uint16_t view = getSettingValue(SettingsOptions::DogHomeVariant);
  click(false);
  expectMode(OperatingMode::HomeScreen, "Klick hinten am Start bleibt auf dem Start");
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == (view + 1) % DOG_HOME_VIEWS, "und zeigt die nächste Ansicht");
  for (int i = 1; i < DOG_HOME_VIEWS; i++) {
    click(false);
  }
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == view, "nach fünf Klicks wieder die erste");
  hold(false, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten am Start öffnet das Menü");
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == view, "und schaltet dabei keine Ansicht weiter");
  toHome();
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

  puts("== Sperre am Start");
  setSettingValue(SettingsOptions::LockingMode, 0);
  step(true, true, 800);
  idle(300);
  expect(!dogLockArmed, "SPERRE aus im Menü: beide halten am Start sperrt nicht");
  expectMode(OperatingMode::HomeScreen, "und bleibt am Start");
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

  puts("== Löten, ein Klick wechselt das Lot sofort");
  setSettingValue(SettingsOptions::SolderingTemp, 320);
  startSoldering();
  click(true);
  expect(setPoint() == 350, "320: Klick vorne sofort aufs nächste Lot, 350");
  expect(currentTempTargetDegC == 350, "Heizung folgt sofort, ohne Bestätigen");
  expectMode(OperatingMode::Soldering, "bleibt im Löten");
  expect(dogLotChangedAt != 0, "Temperatur und Legierung zeigen sich");
  click(true);
  expect(setPoint() == 370, "noch ein Klick: 370");
  click(false);
  expect(setPoint() == 350, "Klick hinten: ein Lot kälter, 350");
  expect(currentTempTargetDegC == 350, "Heizung folgt dem neuen Sollwert");
  click(false);
  click(false);
  expect(setPoint() == 240, "zwei Klicks hinten: 320, dann 240");
  click(false);
  expect(setPoint() == 240, "unten bleibt es bei 240, kein Umspringen");
  click(true);
  click(true);
  click(true);
  click(true);
  expect(setPoint() == 400, "vier Klicks vorne: 400");
  click(true);
  expect(setPoint() == 400, "oben bleibt es bei 400, kein Umspringen");
  idle(2500);
  dogLotChangedAt = 0;
  click(true);
  expect(dogLotChangedAt == 0, "am Ende passiert auch auf dem Screen nichts");
  setSettingValue(SettingsOptions::SolderingTemp, 335);
  click(true);
  expect(setPoint() == 350, "eigener Wert 335: Klick vorne auf das nächsthöhere Lot");
  setSettingValue(SettingsOptions::SolderingTemp, 335);
  click(false);
  expect(setPoint() == 320, "eigener Wert 335: Klick hinten auf das nächsttiefere Lot");
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
  expect(setPoint() == 350, "später wechselt ein Klick wieder das Lot");
  idle(2500);
  setSettingValue(SettingsOptions::SolderingTemp, 320);

  puts("== Keine Temperatur-Ansicht mehr");
  toHome();
  const uint16_t atStart = setPoint();
  hold(true, 800);
  expectMode(OperatingMode::HomeScreen, "Halten vorne am Start tut nichts");
  expect(setPoint() == atStart, "und ändert nichts");

  puts("== Menü: hinten nächster Eintrag, vorne je nach Art, hinten halten zurück");
  auto toEntry = [](int n) {
    for (int i = 0; i < n; i++) {
      click(false);
    }
  };
  setSettingValue(SettingsOptions::SolderingTemp, 335);
  hold(false, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten hinten am Start öffnet das Menü");
  animated = false;
  toEntry(8);
  expect(!animated, "acht Klicks hinten: nichts schiebt");
  expectMode(OperatingMode::SettingsMenu, "und wieder auf dem ersten Eintrag, kein Ende");
  hold(true, 800);
  expectMode(OperatingMode::SettingsMenu, "Halten vorne auf einem Eintrag tut nichts");

  puts("== Zahl: Eigenes Ziel");
  click(true);
  expect(setPoint() == 335, "Klick vorne öffnet das Ändern, ändert aber noch nichts");
  click(true);
  expect(setPoint() == 340, "vorne: +5, gilt sofort");
  click(false);
  expect(setPoint() == 335, "hinten: -5, gilt sofort");
  click(false);
  expect(setPoint() == 330, "nochmal -5, hinten blättert hier nicht");
  idle(1500);
  click(true);
  expect(setPoint() == 335, "nach 1,5 s noch offen");
  idle(2500);
  click(false);
  expect(setPoint() == 335, "nach 2 s ohne Taste zurück auf dem Eintrag: hinten ist wieder nächster Eintrag");
  expectMode(OperatingMode::SettingsMenu, "bleibt im Menü");
  toEntry(7); // wieder Eigenes Ziel
  click(true);
  hold(false, 800);
  click(true);
  idle(100);
  const uint16_t afterHold = setPoint();
  expect(afterHold == 335, "hinten halten: sofort zurück, ohne -5");
  idle(2500);
  setSettingValue(SettingsOptions::SolderingTemp, MAX_TEMP_C);
  click(true);
  click(true);
  click(true);
  expect(setPoint() == MAX_TEMP_C, "oben bleibt es stehen");
  hold(false, 800);
  setSettingValue(SettingsOptions::SolderingTemp, 320);

  puts("== Auswahl: Sperre, Klick vorne sofort nächster Wert");
  toEntry(4); // Startbild
  const uint16_t homeView = getSettingValue(SettingsOptions::DogHomeVariant);
  click(true);
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == (homeView + 1) % DOG_HOME_VIEWS, "Startbild: vorne sofort die nächste Ansicht");
  for (int i = 1; i < DOG_HOME_VIEWS; i++) {
    click(true);
  }
  expect(getSettingValue(SettingsOptions::DogHomeVariant) == homeView, "nach der letzten wieder die erste");
  toEntry(1); // Sperre
  setSettingValue(SettingsOptions::LockingMode, 0);
  click(true);
  expect(getSettingValue(SettingsOptions::LockingMode) == 1, "vorne: Nur Boost, sofort");
  click(true);
  expect(getSettingValue(SettingsOptions::LockingMode) == 2, "vorne: Alles");
  dogLockArmed = true;
  click(true);
  expect(getSettingValue(SettingsOptions::LockingMode) == 0, "vorne: wieder Aus");
  expect(!dogLockArmed, "auf Aus: eine scharfe Sperre geht mit aus, sie ließe sich sonst nie mehr lösen");
  toEntry(3); // zurück auf Eigenes Ziel

  puts("== Untermenü: Schlaf");
  toEntry(2);
  animated = false;
  click(true);
  expect(animated, "Öffnen schiebt");
  const uint16_t sleepBefore = getSettingValue(SettingsOptions::SleepTime);
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore, "Schlaf nach: öffnet das Ändern");
  click(true);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "vorne erhöht");
  hold(false, 800);
  toEntry(4);
  expect(getSettingValue(SettingsOptions::SleepTime) == sleepBefore + 1, "vier Klicks hinten: wieder am Anfang, Wert steht");
  hold(false, 1000);
  expectMode(OperatingMode::SettingsMenu, "hinten halten: eine Ebene zurück");
  click(true);
  hold(false, 1000);

  puts("== Anzeige");
  click(false); // vom Schlaf auf Anzeige
  click(true);
  toEntry(2); // Invertieren
  const uint16_t inverted = getSettingValue(SettingsOptions::OLEDInversion);
  click(true);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == !inverted, "Schalter: vorne schaltet sofort um");
  click(true);
  expect(getSettingValue(SettingsOptions::OLEDInversion) == inverted, "nochmal schaltet zurück");
  toEntry(4); // Detailansicht
  click(true);
  expect(getSettingValue(SettingsOptions::DogSolderView) == 1, "Detailansicht schaltet");
  click(true);
  hold(false, 1000);

  puts("== Info");
  toEntry(3); // Info
  click(true);
  click(true);
  expectMode(OperatingMode::SettingsMenu, "Info: vorne auf einer normalen Seite tut nichts");
  toEntry(4); // System
  click(true);
  expectMode(OperatingMode::DebugMenuReadout, "System: vorne öffnet die Rohwerte");
  click(false);
  click(true);
  expectMode(OperatingMode::DebugMenuReadout, "Klicks blättern dort");
  hold(false, 800);
  expectMode(OperatingMode::SettingsMenu, "hinten halten: zurück auf die Seite");
  hold(false, 1000);
  hold(false, 1000);
  expectMode(OperatingMode::HomeScreen, "hinten halten auf oberster Ebene: zum Start");
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
  expect(dogSleepWakeFrame != 0 || mode == OperatingMode::Soldering, "Klick hinten: der Hund schaut auf");
  expect(currentTempTargetDegC == setPoint() || mode == OperatingMode::Soldering, "dabei heizt er schon aufs Lot");
  idle(500);
  expectMode(OperatingMode::Soldering, "Klick hinten weckt");
  expect(setPoint() == sleepSet, "der weckende Klick wechselt das Lot nicht");
  idle(2500);
  toSleep();
  click(true);
  idle(500);
  expectMode(OperatingMode::Soldering, "Klick vorne weckt");
  expect(setPoint() == sleepSet, "und ändert nichts");
  toSleep();
  step(true, true, 60);
  expectMode(OperatingMode::HomeScreen, "beide Tasten im Schlaf: zum Start");
  idle(300);

  puts("== Löt-Screen nach Lotwechsel, Schlaf und Boost");
  setSettingValue(SettingsOptions::SolderingTemp, 320);
  testSetTip(320);
  startSoldering();
  expect(dogSolderTimerStart != 0 && !dogWokeHeating, "am Ziel: Löt-Screen, die Lötzeit läuft");
  click(true);
  testSetTip(330);
  idle(100);
  expect(dogSolderTimerStart != 0 && !dogWokeHeating, "Nachheizen nach dem Lotwechsel: bleibt auf dem Löt-Screen");
  lastMovementTime = previewTicks - 70000;
  lastButtonTime   = previewTicks - 70000;
  idle(300);
  expectMode(OperatingMode::Sleeping, "schläft");
  testSetTip(150);
  moved();
  idle(60);
  expectMode(OperatingMode::Sleeping, "Bewegung: erst schaut der Hund auf");
  expect(dogSleepWakeFrame >= 1, "Bild 1 von 3");
  idle(500);
  expectMode(OperatingMode::Soldering, "Bewegung weckt");
  expect(dogWokeHeating, "nach dem Schlaf wieder der Heizen-Screen");
  moved();
  step(true, false, 1000);
  expect(currentTempTargetDegC == 420, "Boost beim Heizen nach dem Schlaf");
  idle(200);
  expect(!dogWokeHeating, "nach dem Boost der Löt-Screen");
  step(true, true, 60);
  idle(300);
  testSetTip(30);
  startSoldering();
  expect(dogSolderTimerStart == 0, "neue Sitzung, kalt: Heizen-Screen");
  step(true, false, 1000);
  idle(200);
  expect(dogSolderTimerStart != 0, "Boost vor dem Erreichen: danach Löt-Screen");
  testSetTip(350);
  moved();
  idle(100);
  expect(!dogWokeHeating, "am Ziel bleibt es der Löt-Screen");
  step(true, true, 60);
  idle(300);
  testSetTip(320);

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
  expect(tooWide == nullptr, tooWide ? tooWide : "jeder Name, Bereich und der breiteste Wert passen");

  printf("\n%d Fehler\n", failures);
  return failures ? 1 : 0;
}

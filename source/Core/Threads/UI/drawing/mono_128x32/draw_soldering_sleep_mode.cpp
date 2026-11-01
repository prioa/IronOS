#include "ui_drawing.hpp"

#ifdef OLED_128x32
#ifdef TS101_DOG_UI
#include "ts101_dog_sprites.h"

// Sleeping German Shepherd on the left, "z Z Z" drifting up from its head, tip temperature on the right
static void drawSleepingDog() {
  OLED::drawArea(0, 8, DOG_LYING_WIDTH, DOG_LYING_HEIGHT, dogLying);
  uint8_t step = (xTaskGetTickCount() / (TICKS_100MS * 6)) % DOG_ZZZ_STEPS;
  OLED::drawArea(52, 0, DOG_ZZZ_WIDTH, 8, dogZzz[step]);
}

void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  drawSleepingDog();
  OLED::setCursor(80, 0);
  OLED::printNumber(tipTemp, 3, FontStyle::SMALL);
  OLED::printSymbolDeg(FontStyle::SMALL);
  OLED::setCursor(80, 16);
  printVoltage();
  OLED::print(SmallSymbolVolts, FontStyle::SMALL);
  OLED::refresh();
}

void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) {
  OLED::clearScreen();
  drawSleepingDog();
  OLED::setCursor(78, 4); // 3 large digits + degree symbol = 48px, flush right; y=4 centres the 24px font
  OLED::printNumber(tipTemp, 3, FontStyle::LARGE);
  OLED::printSymbolDeg(FontStyle::EXTRAS);
  OLED::refresh();
}

#else
void ui_draw_soldering_detailed_sleep(TemperatureType_t tipTemp) {

  OLED::clearScreen();
  OLED::setCursor(0, 0);
  OLED::print(translatedString(Tr->SleepingAdvancedString), FontStyle::SMALL);
  OLED::setCursor(0, 16);
  OLED::print(translatedString(Tr->SleepingTipAdvancedString), FontStyle::SMALL);
  OLED::printNumber(tipTemp, 3, FontStyle::SMALL);
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    OLED::print(SmallSymbolDegF, FontStyle::SMALL);
  } else {
    OLED::print(SmallSymbolDegC, FontStyle::SMALL);
  }

  OLED::print(SmallSymbolSpace, FontStyle::SMALL);
  printVoltage();
  OLED::print(SmallSymbolVolts, FontStyle::SMALL);

  OLED::refresh();
}

void ui_draw_soldering_basic_sleep(TemperatureType_t tipTemp) {

  OLED::clearScreen();
  // Centre "Zzz NNN°" on the panel: 48px sleep symbol + 36px (3 digits) + 12px degree = 96px,
  // and y=4 centres the 24px large font vertically.
  OLED::setCursor(16, 4);

  OLED::print(LargeSymbolSleep, FontStyle::LARGE);
  OLED::printNumber(tipTemp, 3, FontStyle::LARGE);
  OLED::printSymbolDeg(FontStyle::EXTRAS);

  OLED::refresh();
}
#endif /* TS101_DOG_UI */
#endif
#include "ts101_dog_ui.hpp"
#include "ui_drawing.hpp"
#ifdef OLED_128x32

void ui_draw_warning_undervoltage(void) {
#ifdef TS101_DOG_UI
  char  text[24];
  uint16_t volts = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  strcpy(text, DOG_T("NUR ", "ONLY "));
  char *end = dogFormat(text + strlen(text), volts / 10);
  *end++    = '.';
  *end++    = '0' + volts % 10;
  strcpy(end, DOG_T("V AM EINGANG", "V INPUT"));
  dogDrawNotice(iconWarning, DOG_T("STROM!", "POWER!"), text);
  OLED::refresh();
  GUIDelay();
  waitForButtonPress();
  return;
#endif
  OLED::clearScreen();
  OLED::setCursor(0, 0);
  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    OLED::print(translatedString(Tr->UndervoltageString), FontStyle::SMALL);
    OLED::setCursor(0, 16);
    OLED::print(translatedString(Tr->InputVoltageString), FontStyle::SMALL);
    printVoltage();
    OLED::print(SmallSymbolVolts, FontStyle::SMALL);
  } else {
    OLED::print(translatedString(Tr->UVLOWarningString), FontStyle::LARGE);
  }

  OLED::refresh();
  GUIDelay();
  waitForButtonPress();
}
#endif
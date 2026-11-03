#include "Buttons.hpp"
#include "OperatingModeUtilities.h"
#include "configuration.h"
#include "ui_drawing.hpp"
#ifdef TS101_DOG_UI
#include "ts101_dog_ui.hpp"
#endif
#ifdef POW_DC
extern volatile TemperatureType_t currentTempTargetDegC;
// returns true if undervoltage has occured
bool checkForUnderVoltage(void) {
  if (!getIsPoweredByDCIN()) {
    return false;
  }
  uint16_t v = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);

  // Dont check for first 2 seconds while the ADC stabilizes and the DMA fills
  // the buffer
  if (xTaskGetTickCount() > (TICKS_SECOND * 2)) {
    if ((v < lookupVoltageLevel())) {
      currentTempTargetDegC = 0;
#ifdef TS101_DOG_UI
      dogAlert = DogAlert::LowVoltage; // interface v2: the home screen says it
#else
      ui_draw_warning_undervoltage();
#endif
      return true;
    }
  }
  return false;
}
#endif
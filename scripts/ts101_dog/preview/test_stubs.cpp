// Hardware and RTOS calls the logic needs, for test_buttons only
#include "OperatingModes.h"
uint8_t heaterThermalRunawayCounter = 0;
void    setBuzzer(bool) {}
void    setStatusLED(const enum StatusLED) {}
void    ui_draw_debug_menu(const uint8_t) {}
bool    checkForUnderVoltage(void) { return false; }
void    GUIDelay() {}
bool USBPowerDelivery::fusbPresent() { return false; }
bool USBPowerDelivery::negotiationComplete() { return false; }

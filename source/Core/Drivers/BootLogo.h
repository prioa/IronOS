#ifndef DRIVERS_BOOTLOGO_H_
#define DRIVERS_BOOTLOGO_H_

// Wrapper for handling showing a bootlogo
#include "configuration.h"
#include <stdint.h>
#define OLD_LOGO_HEADER_VALUE 0xF00DAA55
class BootLogo {
public:
  static void handleShowingLogo(const uint8_t *ptrLogoArea);

private:
  static void showOldFormat(const uint8_t *ptrLogoArea);
  static void showNewFormat(const uint8_t *ptrLogoArea);
  static int  showNewFrame(const uint8_t *ptrLogoArea);
#ifdef TS101_DOG_UI
  static void showFullScreenFormat(const uint8_t *ptrLogoArea);
  static int  showFullScreenFrame(const uint8_t *ptrLogoArea);
#endif
};

#endif // DRIVERS_BOOTLOGO_H_
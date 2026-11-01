#include "BootLogo.h"
#include "BSP.h"
#include "Buttons.hpp"
#include "OLED.hpp"
#include "Settings.h"
#include "cmsis_os.h"

#define LOGO_PAGE_LENGTH 1024

void delay() {
  if (getSettingValue(SettingsOptions::LOGOTime) >= logoMode_t::ONETIME) {
    waitForButtonPress();
  } else {
    waitForButtonPressOrTimeout(TICKS_SECOND * getSettingValue(SettingsOptions::LOGOTime));
  }
}

void BootLogo::handleShowingLogo(const uint8_t *ptrLogoArea) {
  OLED::clearScreen();
  // Read the first few bytes and figure out what format we are looking at
  if (OLD_LOGO_HEADER_VALUE == *(reinterpret_cast<const uint32_t *>(ptrLogoArea))) {
    showOldFormat(ptrLogoArea);
  } else if (ptrLogoArea[0] == 0xAA) {
    showNewFormat(ptrLogoArea + 1);
  }
#ifdef TS101_DOG_UI
  else if (ptrLogoArea[0] == 0xAB) {
    showFullScreenFormat(ptrLogoArea + 1);
  }
#endif

  OLED::clearScreen();
}

void BootLogo::showOldFormat(const uint8_t *ptrLogoArea) {
#ifdef OLED_128x32
  // Draw in middle
  OLED::drawAreaSwapped(16, 8, 96, 16, (uint8_t *)(ptrLogoArea + 4));

#else
  OLED::drawAreaSwapped(0, 0, 96, 16, (uint8_t *)(ptrLogoArea + 4));

#endif
  OLED::refresh();
  // Delay here with static logo until a button is pressed or its been the amount of seconds set by the user
  delay();
}

void BootLogo::showNewFormat(const uint8_t *ptrLogoArea) {
  if (getSettingValue(SettingsOptions::LOGOTime) == logoMode_t::SKIP) {
    return;
  }

  // New logo format (a) fixes long standing byte swap quirk and (b) supports animation
  uint8_t interFrameDelay = ptrLogoArea[0];
  OLED::clearScreen();

  // Now draw in the frames
  int position = 1;
  while (getButtonState() == BUTTON_NONE) {
    int len = (showNewFrame(ptrLogoArea + position));
    OLED::refresh();
    position += len;

    if (interFrameDelay) {
      osDelay(interFrameDelay * 4);
    }

    // 1024 less the header type byte and the inter-frame-delay
    if (getSettingValue(SettingsOptions::LOGOTime) && (position >= 1022 || len == 0)) {
      // Animated logo stops here ...
      if (getSettingValue(SettingsOptions::LOGOTime) == logoMode_t::INFINITY) {
        // ... but if it's infinite logo setting then keep it rolling over again until a button is pressed
        osDelay(4 * TICKS_100MS);
        OLED::clearScreen();
        position = 1;
        continue;
      }
    } else {
      // Animation in progress so jumping to the next frame
      continue;
    }

    // Static logo case ends up right here, so delay until a button is pressed or its been the amount of seconds set by the user
    delay();
    return;
  }
}

int BootLogo::showNewFrame(const uint8_t *ptrLogoArea) {
  uint8_t length = ptrLogoArea[0];
  switch (length) {
  case 0:
    // End
    return 0;
    break;
  case 0xFE:
    return 1;
    break;
  case 0xFF:
// Full frame update
#ifdef OLED_128x32
    OLED::drawArea(16, 8, 96, 16, ptrLogoArea + 1);
#else
    OLED::drawArea(0, 0, 96, 16, ptrLogoArea + 1);
#endif
    length = 96;
    break;
  default:
    length /= 2;
    // Draw length patches
    for (int p = 0; p < length; p++) {
      uint8_t index = ptrLogoArea[1 + (p * 2)];
      uint8_t value = ptrLogoArea[2 + (p * 2)];
#ifdef OLED_128x32
      OLED::drawArea(16 + (index % 96), index >= 96 ? 16 : 8, 1, 8, &value);
#else
      OLED::drawArea(index % 96, index >= 96 ? 8 : 0, 1, 8, &value);
#endif
    }
  }

  OLED::refresh();
  return (length * 2) + 1;
}

#ifdef TS101_DOG_UI
/*
 * Full screen (128x32) animated logo, header byte 0xAB:
 *   [0xAB][inter frame delay x4ms][frame]...[0x00]
 * Each frame starts with a run count:
 *   0x00       end of animation
 *   0xFE       hold the previous frame for one more delay
 *   0xFF       full frame, 512 bytes (4 pages x 128 columns)
 *   1..0xFD    that many runs of [index hi][index lo][length][length bytes], index = page * 128 + column,
 *              a run never crosses into the next page
 */
void BootLogo::showFullScreenFormat(const uint8_t *ptrLogoArea) {
  if (getSettingValue(SettingsOptions::LOGOTime) == logoMode_t::SKIP) {
    return;
  }
  uint8_t interFrameDelay = ptrLogoArea[0];
  OLED::clearScreen();

  int position = 1;
  while (getButtonState() == BUTTON_NONE) {
    int len = showFullScreenFrame(ptrLogoArea + position);
    position += len;

    if (interFrameDelay) {
      osDelay(interFrameDelay * 4);
    }

    if (getSettingValue(SettingsOptions::LOGOTime) && (position >= (FLASH_LOGO_LENGTH - 2) || len == 0)) {
      if (getSettingValue(SettingsOptions::LOGOTime) == logoMode_t::INFINITY) {
        osDelay(4 * TICKS_100MS);
        OLED::clearScreen();
        position = 1;
        continue;
      }
    } else {
      continue;
    }

    delay();
    return;
  }
}

int BootLogo::showFullScreenFrame(const uint8_t *ptrLogoArea) {
  uint8_t runs = ptrLogoArea[0];
  if (runs == 0) {
    return 0;
  }
  if (runs == 0xFE) {
    return 1;
  }
  if (runs == 0xFF) {
    OLED::drawArea(0, 0, OLED_WIDTH, OLED_HEIGHT, ptrLogoArea + 1);
    OLED::refresh();
    return 1 + (OLED_WIDTH * OLED_HEIGHT / 8);
  }
  int position = 1;
  for (uint8_t r = 0; r < runs; r++) {
    uint16_t index  = (ptrLogoArea[position] << 8) | ptrLogoArea[position + 1];
    uint8_t  length = ptrLogoArea[position + 2];
    OLED::drawArea(index % OLED_WIDTH, (index / OLED_WIDTH) * 8, length, 8, ptrLogoArea + position + 3);
    position += 3 + length;
  }
  OLED::refresh();
  return position;
}
#endif

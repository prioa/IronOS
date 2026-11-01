// Host-side preview of the TS101 128x32 screens: draws with the real UI code and writes PBM images
#define private public // the preview needs the framebuffer
#include "OLED.hpp"
#undef private
#include "preview_state.hpp"
#include "ui_drawing.hpp"
#include <stdio.h>
#include <string>

TickType_t previewTicks = 0;
static uint8_t frame[FRAMEBUFFER_START + OLED_WIDTH * OLED_HEIGHT / 8];

static void save(const std::string &name) {
  FILE *f = fopen((name + ".pbm").c_str(), "w");
  fprintf(f, "P1\n%d %d\n", OLED_WIDTH, OLED_HEIGHT);
  const uint8_t *buf = frame + FRAMEBUFFER_START;
  for (int y = 0; y < OLED_HEIGHT; y++) {
    for (int x = 0; x < OLED_WIDTH; x++) {
      fprintf(f, "%d ", (buf[(y / 8) * OLED_WIDTH + x] >> (y % 8)) & 1);
    }
    fprintf(f, "\n");
  }
  fclose(f);
}

int main() {
  OLED::setFramebuffer(frame);
  previewSetup();
  for (const PreviewScene &s : previewScenes()) {
    previewTicks = s.ticks;
    previewApply(s);
    OLED::clearScreen();
    s.draw();
    save(s.name);
  }
  return 0;
}

#pragma once
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

struct PreviewScene {
  std::string           name;
  uint32_t              ticks;
  uint16_t              tipC, setC, x10W, vX10;
  bool                  detailed;
  std::function<void()> draw;
};
void                      previewSetup();
void                      previewApply(const PreviewScene &s);
std::vector<PreviewScene> previewScenes();

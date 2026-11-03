#ifndef UI_DRAWING_TS101_V2_SHAKE_HPP_
#define UI_DRAWING_TS101_V2_SHAKE_HPP_
#include "configuration.h"
#ifdef TS101_DOG_UI
#include "Defines.h"
#include "FreeRTOS.h"
#include <stdint.h>

void dogShakeFeed(int16_t x, int16_t y, int16_t z, TickType_t now); // every reading, from the movement thread
bool dogShakeTaken();                                                 // a shake was seen since the last call
void dogShakeReset();
#endif
#endif

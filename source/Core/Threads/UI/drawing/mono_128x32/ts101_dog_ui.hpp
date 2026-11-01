#ifndef UI_DRAWING_TS101_DOG_UI_HPP_
#define UI_DRAWING_TS101_DOG_UI_HPP_
#include "configuration.h"
#ifdef TS101_DOG_UI
#include "FreeRTOS.h"
#include "ts101_dog_sprites.h"
#include <stdint.h>

enum class DogPose { Sit, Wag, Pant, Bark, Sad };

// Soldering session state, kept across trips through sleep and temperature adjust (driven by Soldering.cpp / Sleep.cpp)
extern TickType_t dogSolderTimerStart; // tick the set point was first reached, 0 = still heating up
extern uint16_t   dogHeatupStartTempC; // tip temperature when heating up started
extern TickType_t dogPresetShownUntil; // the preset just picked is shown large until this tick
extern TickType_t dogBarkUntil;        // the dog barks "WUFF!" until this tick
extern TickType_t dogWakeUntil;        // the dog wags after waking up until this tick
extern TickType_t dogSleepEnterTime;   // when sleep started, for the lie down animation

void dogDrawSitting(int16_t x, DogPose pose);
void dogDrawWuff(int16_t x, int8_t y);                  // y must be on a page boundary
void dogPrintTime(int16_t x, int8_t y, const uint8_t *icon, uint16_t major, uint8_t minor); // [icon]major:minor, 6x8 font
void dogPrintSessionTime(int16_t x, int8_t y); // stopwatch icon + m:ss of this session (h:mm after 100 minutes)
void dogPrintTotalTime(int16_t x, int8_t y);   // sum icon + h:mm soldered over the lifetime of the iron
void dogPrintVoltage();                        // input voltage 20.1V at the cursor, 6x8 font

// Pose for the sitting dog: barking, wagging after waking, panting while heating hard, an occasional wag otherwise
DogPose dogSolderingPose(uint32_t x10Watt, int32_t x10Limit);

// Tip temperature history for the curve in the detailed soldering view
void dogCurveReset();
void dogCurveSample(uint16_t tipC);
void dogDrawCurve(int16_t x, uint8_t width, uint8_t height, uint16_t setC);

// Lifetime soldering time, counted while the set point is held and saved when back on the home screen
void dogCountSolderTime();
void dogSaveSolderTime();
#endif
#endif // UI_DRAWING_TS101_DOG_UI_HPP_

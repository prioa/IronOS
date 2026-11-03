#ifndef UI_DRAWING_TS101_V2_SOLDER_HPP_
#define UI_DRAWING_TS101_V2_SOLDER_HPP_
#include "ts101_v2_grid.hpp"
#ifdef TS101_DOG_UI

enum class DogSolderState : uint8_t { Heating, Ready, Cooling, Load, Boost, Locked };
DogSolderState dogSolderState(bool boost);
void           dogDrawSolderScreen(bool boost);
void           dogDrawSensorSide();               // the table of the sensors, zones A-C
void           dogSolderViewReset();              // entering soldering: the picked view, no wipe
void           dogSolderWarning(const char *text); // row 2 shows this warning on the next frame, the normal view comes forward
const char    *dogSolderName();                   // the solder the set point is on, or "Eigenes Ziel"
uint16_t       dogSolderTarget(bool boost);       // in the unit shown
uint16_t       dogBoostOffset();
uint8_t        dogPwmPercent();
char          *dogFormatX10(char *out, uint32_t x10, const char *unit); // "20.0V"
void           dogDrawHomeV2(uint16_t shownTemperature); // ts101_v2_home.cpp
void           dogHomeLockNotice(bool locked);           // the lock gesture: row 2 says what it did, for 1.5 s
bool           dogHomeCooling();
char          *dogFormatUnit(char *out, int32_t value, const char *unit); // "-12°"
#endif
#endif

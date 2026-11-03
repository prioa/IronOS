#ifndef UI_DRAWING_TS101_V2_GRID_HPP_
#define UI_DRAWING_TS101_V2_GRID_HPP_
/*
 * TS101_DOG_UI interface v2: the grid every screen is drawn on (128x32)
 *
 *   A name     x 0-93,  y 0-9    helvB08, capitals from y 0          D number  x 96-127, y 0-10   helvB10, ring behind it
 *   B row 2    x 0-92,  y 11-20  helvB08 capitals from y 12          E target  x 96-127, y 12-16  3x5, right aligned
 *   C history  x 0-92,  y 22-31  the curve, 1 px a sample            F keys    rear x 96-110, front x 113-127, y 18-31
 *
 * Nothing on the left reaches past x 92 (the name may use x 93): the zones clip what they draw.
 */
#include "configuration.h"
#ifdef TS101_DOG_UI
#include "OLED.hpp"
#include "ts101_dog_ui.hpp"
#include "ts101_v2_fonts.h"
#include <stdint.h>

#define GRID_LEFT_END 92 // the last column of zones B and C
#define GRID_NAME_END 93
#define GRID_RIGHT_X  96 // zones D-F
#define GRID_NAME_BASE  8  // capitals on rows 0-7
#define GRID_ROW2_BASE  20 // capitals on rows 12-19
#define GRID_ROW2_TOP   11
#define GRID_ROW2_BOT   20
#define GRID_CURVE_TOP  22
#define GRID_CURVE_BOT  31
#define GRID_TARGET_BASE 17 // the 3x5 font on rows 12-16
#define GRID_KEY_TOP    18
#define GRID_KEY_BOT    31
#define GRID_KEY_W      15
#define GRID_KEY_REAR_X  96
#define GRID_KEY_FRONT_X 113

// What dogBlit and the grid's rectangles may touch, in columns; everything outside is left alone
extern int16_t dogClipX0, dogClipX1;
inline void    gridClip(int16_t x0, int16_t x1) { dogClipX0 = x0, dogClipX1 = x1; }
inline void    gridClipAll() { gridClip(0, OLED_WIDTH - 1); }

void gridFill(int16_t x0, int16_t y0, int16_t x1, int16_t y1);   // lit, within the clip
void gridInvert(int16_t x0, int16_t y0, int16_t x1, int16_t y1); // within the clip
void gridIcon(int16_t x, int16_t y, const DogIcon &icon);
void gridDottedRow(int16_t x0, int16_t x1, int16_t y, uint8_t every);
void gridDottedColumn(int16_t x, int16_t y0, int16_t y1, uint8_t every);

// Text: base = the row below the letters. The 3x5 font: rows base-5 .. base-1
inline uint8_t gridWidth(const DogFont &font, const char *text) { return dogTextWidth(font, text); }
void           gridTextRight(const DogFont &font, int16_t x1, int16_t base, const char *text);
void           gridTextCentre(const DogFont &font, int16_t x0, int16_t x1, int16_t base, const char *text);

// ---- The zones ----
void gridName(const char *text);                       // A
void gridState(const char *label, const char *values); // B: the label inverted, the values 3 px behind it
void gridValues(int16_t x, const char *values);        // B: values only, from x
void gridWarn(const char *text);                       // B: the whole row inverted, text starting with "!"
// C: the curve of dogCurveSample's samples. With a target: the dotted set line on row 23 and 80 degrees below it, else 20-420 C
void gridCurve(bool withTarget, uint16_t targetC);
void gridTemp(uint16_t value); // D: the reading with its ring
// E: arrow and target. dir 1 heating, -1 cooling, 0 at the set point (the check mark). target 0: the arrow only
void gridTarget(int8_t dir, uint16_t target);
// F: the two keys, each icon centred in its box. The inside of a box inverts while its button is down, or when forced
void gridKeys(const DogIcon *rear, const DogIcon *front, bool invertRear = false, bool invertFront = false);
void gridWideKey(const char *text); // F: both buttons do the same, one box with a 3x5 word

#endif
#endif

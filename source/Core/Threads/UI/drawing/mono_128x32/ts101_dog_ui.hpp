#ifndef UI_DRAWING_TS101_DOG_UI_HPP_
#define UI_DRAWING_TS101_DOG_UI_HPP_
#include "configuration.h"
#ifdef TS101_DOG_UI
#include "FreeRTOS.h"
#include "ts101_dog_fonts.h"
#include "ts101_dog_sprites.h"
#include <stdint.h>

enum class DogPose { Sit, Wag, Pant, Bark };

// Soldering session state, kept across trips through sleep and temperature adjust (driven by Soldering.cpp / Sleep.cpp)
extern TickType_t dogSolderTimerStart; // tick the set point was first reached, 0 = still heating up
extern uint16_t   dogHeatupStartTempC; // tip temperature when heating up started
extern TickType_t dogPresetShownUntil; // the preset just picked is shown large until this tick
extern TickType_t dogBarkUntil;        // the dog barks "WUFF!" until this tick
extern TickType_t dogWakeUntil;        // the dog wags after waking up until this tick
extern TickType_t dogSleepEnterTime;   // when sleep started, for the lie down animation
extern uint32_t   dogLastSessionSeconds; // length of the last soldering session, for the home screen

void dogDrawHead(int16_t x, DogPose pose);
void dogDrawWuff(int16_t x, int8_t y);                  // y must be on a page boundary
void dogDrawRunning(int16_t x, int8_t y);               // running dog, any position; clipped at the screen edge
void dogDrawRunAcross();                                // the dog runs across the top 24 rows of the whole screen
void dogDrawFlame(int16_t x);                           // blinking flame in the top row, marks soldering mode
void dogPrintTime(int16_t x, int8_t y, const uint8_t *icon, uint16_t major, uint8_t minor); // [icon]major:minor, 6x8 font
void dogPrintSessionTime(int16_t x, int8_t y); // stopwatch icon + m:ss of this session (h:mm after 100 minutes)
void dogPrintTotalTime(int16_t x, int8_t y);   // sum icon + h:mm soldered over the lifetime of the iron
void dogPrintVoltage();                        // input voltage 20.1V at the cursor, 6x8 font

// Upper case 5x7 label font (A-Z, ÄÖÜ, digits, . : - / ! + % °), UTF-8 strings, 6 px per character
uint8_t dogLabelWidth(const char *text);
void    dogPrintLabel(int16_t x, int8_t y, const char *text); // y must be on a page boundary
uint8_t dogLabelBigWidth(const char *text);
void    dogPrintLabelBig(int16_t x, int8_t y, const char *text); // label font doubled to 10x14, any y

// OR a sprite (page format, height a multiple of 8) onto the screen at any position, keeping what is under it
void dogBlit(int16_t x, int8_t y, uint8_t width, uint8_t height, const uint8_t *data);
// Full screen notice: 24x24 icon on the left, one or two big lines, optional footer row under a rule
void dogDrawNotice(const uint8_t *icon, const char *line1, const char *line2, const char *footer);

// Full screen notice after picking a preset with a double click; returns true while it is showing
bool dogDrawPresetNotice();
// Called on the home screen: ends the soldering session and saves the lifetime soldering time
void dogSessionEnded();

// Pose for the dog at temperature: barking, wagging after waking, panting while heating hard, an occasional wag otherwise
DogPose dogSolderingPose(uint32_t x10Watt, int32_t x10Limit);

// ---- Text with the bitmap fonts, base = the row just below the letters ----
uint8_t dogTextWidth(const DogFont &font, const char *text);
void    dogText(const DogFont &font, int16_t x, int16_t base, const char *text);
// The small font, left aligned, centred between x0 and x1, right aligned at x1; the default base is the key row
#define DOG_KEY_BASE 30
void dogSmall(int16_t x, const char *text, int16_t base = DOG_KEY_BASE);
void dogSmallCentre(int16_t x0, int16_t x1, const char *text, int16_t base = DOG_KEY_BASE);
void dogSmallRight(int16_t x1, const char *text, int16_t base = DOG_KEY_BASE);
char   *dogFormat(char *out, uint16_t value); // decimal digits, returns the end of the string
// Large digits with the small degree ring, on rows 0-21
uint8_t dogBigWidth(uint16_t value);
void    dogBig(int16_t x, uint16_t value);
void    dogBigRight(int16_t x1, uint16_t value);

// ---- Key row (rows 23-31): each label stays next to its button, wherever the orientation puts it ----
struct DogKey {
  const uint8_t *icon;
  const char    *text;
};
#define DOG_HOLD_START (TICKS_100MS * 3 / 2) // shorter presses only invert the field; the bar starts after this
#define DOG_HOLD_FULL  (TICKS_100MS * 6)     // the bar is full and the action fires
#define DOG_HOLD_STOP  (TICKS_SECOND)        // the same for switching the heater off

void       dogButtonsPoll();                // once per frame, follows the buttons themselves (getButtonA/getButtonB)
bool       dogPressed(bool front);          // down on its own, and not left over from the previous screen
TickType_t dogHeldTicks(bool front);        // how long it has been down, 0 if not dogPressed
void       dogPressConsumed();              // the press did its job: it counts for nothing more until released
int16_t    dogFrontKeyX();                  // the column of the key icon on the front button's side
void       dogDrawKeys(DogKey front, DogKey rear, const char *centre);       // words and icons, inverted while pressed
void       dogDrawCorners(const uint8_t *frontIcon, const uint8_t *rearIcon); // icons only, inverted while pressed
// While held long enough, replaces the key row: text with a bar filling from the button's side. True if it drew
bool       dogDrawHold(const char *text, bool front, TickType_t full);
bool       dogHoldDone(bool front, TickType_t full); // held long enough for the action

// Tip temperature history for the curve in the detailed soldering view
void dogCurveReset();
void dogCurveSample(uint16_t tipC);
void dogDrawCurve(int16_t x, uint8_t width, uint8_t height, uint16_t setC);

// Lifetime soldering time, counted while the set point is held and saved when back on the home screen
void dogCountSolderTime();

// Back button in soldering mode: dog <-> curve; kept in RAM and saved with the soldering time
void dogNextSolderView();
// A setting changed while soldering: saved with the soldering time on the home screen, sparing the flash
void dogSettingsChanged();
#endif
#endif // UI_DRAWING_TS101_DOG_UI_HPP_

#ifndef UI_DRAWING_TS101_DOG_UI_HPP_
#define UI_DRAWING_TS101_DOG_UI_HPP_
#include "configuration.h"
#ifdef TS101_DOG_UI
#include "Buttons.hpp"
#include "FreeRTOS.h"
#include "ts101_dog_fonts.h"
#include "ts101_dog_sprites.h"
#include <stdint.h>

// Soldering session state, kept across trips through sleep and temperature adjust (driven by Soldering.cpp / Sleep.cpp)
extern TickType_t dogSolderTimerStart; // tick the set point was first reached, 0 = still heating up
extern uint16_t   dogHeatupStartTempC; // tip temperature when heating up started
extern TickType_t dogBarkUntil;        // the dog barks "WUFF!" until this tick
extern TickType_t dogSleepEnterTime;   // when sleep started, for the lie down animation
extern uint32_t   dogLastSessionSeconds; // length of the last soldering session, for the home screen

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

// Called on the home screen: ends the soldering session and saves the lifetime soldering time
void dogSessionEnded();

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

// Once per frame, follows the buttons themselves (getButtonA/getButtonB); hides the events of a consumed press
ButtonState dogButtonsPoll(ButtonState buttons);
bool       dogPressed(bool front);          // down on its own, and not left over from the previous screen
TickType_t dogHeldTicks(bool front);        // how long it has been down, 0 if not dogPressed
void       dogPressConsumed();              // the press did its job: it counts for nothing more until released
void       dogSetLocked(bool locked);       // button lock: no field inverts and no bar fills
int16_t    dogFrontKeyX();                  // the column of the key icon on the front button's side
void       dogDrawKeys(DogKey front, DogKey rear, const char *centre);       // words and icons, inverted while pressed
void       dogDrawCorners(const uint8_t *frontIcon, const uint8_t *rearIcon); // icons only, inverted while pressed
// While held long enough, replaces the key row: text with a bar filling from the button's side. True if it drew
bool       dogDrawHold(const char *text, bool front, TickType_t full);
bool       dogHoldDone(bool front, TickType_t full); // held long enough for the action

// ---- Temperature view: changes the set point; while soldering it lives inside the soldering mode, so the heater and every safety check keep running ----
void dogAdjustOpened();                         // restarts the view for a fresh press
bool dogAdjustButtons(ButtonState buttons);     // applies the buttons; true when the view should close
void dogDrawTemperatureView(bool heating);      // set point, scale, and the curve or the preset's use
#define DOG_ADJUST_TIMEOUT (TICKS_SECOND * 2)
#define DOG_BOOST_CLICK_GUARD (TICKS_100MS * 8) // clicks this soon after a boost are ignored (upstream issue #1663)

// Texts of the new screens: German in the DE build, English everywhere else (the fonts only have Latin letters)
extern const char dogLanguageCode[];
inline bool       dogLangDE() { return dogLanguageCode[0] == 'D' && dogLanguageCode[1] == 'E'; }
#define DOG_T(de, en) (dogLangDE() ? (de) : (en))

// Tip temperature history for the curve
void dogCurveReset();
void dogCurveSample(uint16_t tipC);
enum class DogCurve { Steady, Heat, Boost }; // the vertical range: round the set point, from room temperature, up to the boost target
// The last 18 s between the given columns and rows, newest on the right; dots = dotted set point line
void dogDrawCurve(int16_t x0, int16_t x1, uint8_t y0, uint8_t y1, uint16_t setC, DogCurve range, bool dots);

// Lifetime soldering time, counted while the set point is held and saved when back on the home screen
void dogCountSolderTime();

// Back button in soldering mode: dog <-> curve; kept in RAM and saved with the soldering time
void dogNextSolderView();
// A setting changed while soldering: saved with the soldering time on the home screen, sparing the flash
void dogSettingsChanged();
#endif
#endif // UI_DRAWING_TS101_DOG_UI_HPP_

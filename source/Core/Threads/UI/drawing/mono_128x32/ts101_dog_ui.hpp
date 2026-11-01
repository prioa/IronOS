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
extern TickType_t dogReachedUntil;     // the screen stays inverted until this tick: the set point was just reached
extern TickType_t dogSleepEnterTime;   // when sleep started, for the lie down animation
extern uint32_t   dogLastSessionSeconds; // length of the last soldering session, for the home screen
extern TickType_t dogLotChangedAt;     // a click changed the solder: its temperature and alloy show for 2 s
extern bool       dogWokeHeating;      // back from sleep and not at the set point yet: the dog runs again

char *dogFormatTime(char *out, uint16_t major, uint8_t minor); // major:mm

// OR a sprite (page format, height a multiple of 8) onto the screen at any position, keeping what is under it
void dogBlit(int16_t x, int8_t y, uint8_t width, uint8_t height, const uint8_t *data);
// Full screen notice: 24x24 icon on the left, big word, below it what it means
void dogDrawNotice(const uint8_t *icon, const char *word, const char *footer);
// Lock notices with the same look as warnUser: true when they should end
bool dogWarnLocked(bool locked, ButtonState buttons);

// Something stopped the heater and the home screen has to say why, until a button is pressed and it is safe again
enum class DogAlert : uint8_t { None, Handle, Thermal };
extern DogAlert dogAlert;
bool dogHandleTooHot(); // above DOG_HANDLE_LIMIT_X10C for long enough not to be a glitch

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

// ---- The two button boxes, 15x15 on rows 17-31: the left one at x, the other 17 px further. No icon draws - and + ----
void dogKeyBox(uint8_t x, bool front, const uint8_t *icon, uint8_t iconX, uint8_t iconY, uint8_t width, uint8_t height);
void dogKeyPair(uint8_t x, const uint8_t *front, const uint8_t *rear);

// ---- Holding a button ----
#define DOG_HOLD_START (TICKS_100MS * 3 / 2) // shorter presses only invert the field; the bar starts after this
#define DOG_HOLD_FULL  (TICKS_100MS * 6)     // the bar is full and the action fires
#define DOG_HOLD_STOP  (TICKS_SECOND / 2)    // the same for switching the heater off
#define DOG_BOOST_MAX  (TICKS_SECOND * 30)   // boost ends by itself after this, even with the button still down
#define DOG_VALUE_TIMEOUT (TICKS_SECOND * 2) // the value in the menu closes itself after this long without a button

// Once per frame, follows the buttons themselves (getButtonA/getButtonB); hides the events of a consumed press
ButtonState dogButtonsPoll(ButtonState buttons);
bool       dogBothDown();                   // both are down right now, also during a consumed press or a lock
bool       dogFrontOnRight();               // which side the front button's label goes on
bool       dogPressed(bool front);          // down on its own, and not left over from the previous screen
TickType_t dogHeldTicks(bool front);        // how long it has been down, 0 if not dogPressed
void       dogPressConsumed();              // the press did its job: it counts for nothing more until released
void       dogSetLocked(bool locked);       // button lock: no field inverts and no bar fills
bool       dogHoldDone(bool front, TickType_t full); // held long enough for the action

// The settings menu, drawn on its own for the preview. Sections: 0 the top level, 1 solder, 2 sleep, 3 display, 4 info
void dogDrawMenuList(uint8_t section, uint8_t pos);
void dogDrawMenuValue(uint8_t section, uint8_t pos);
void dogMenuBlink(); // the value just changed: it inverts once
#define DOG_MENU_SECTION_INFO 4

const char *dogTipName(); // the tip type picked in the menu, in the small font

// The two rows of an info page (menu -> info); the last page leads on to the raw debug values
#define DOG_INFO_PAGES 5
void dogDrawInfoRows(uint8_t page);

// ---- The solders: a click while soldering moves one up or down, nothing at the ends ----
void   dogJumpPreset(bool up);
int8_t dogPresetIndex(); // which solder the set point is on, -1 for none
#define DOG_BOOST_CLICK_GUARD (TICKS_100MS * 8) // clicks this soon after a boost are ignored (upstream issue #1663)

// Texts of the new screens: German in the DE build, English everywhere else (the fonts only have Latin letters)
extern const char dogLanguageCode[];
inline bool       dogLangDE() { return dogLanguageCode[0] == 'D' && dogLanguageCode[1] == 'E'; }
#define DOG_T(de, en) (dogLangDE() ? (de) : (en))

// ---- Home screen (ts101_dog_home.cpp) ----
void dogDrawHome(uint16_t shownTemperature); // the temperature as shown, in the unit of the setting
void dogHomeTick();                          // every GUI frame in every mode: the 10 minute curve and the trend arrow
void dogHomeHistoryReset();
void dogHomeMotionUpdate();                  // follows the sensor, 8 times a second however often it is called
void dogHomeMotionReset();
extern volatile int16_t dogAccelRaw[3];      // the latest reading, written by the movement thread
extern uint16_t         dogSessionMinutes;   // soldering minutes since power on, not saved
void dogCountSession();                      // a soldering session started; saved on the home screen
void dogCountBoost();                        // a boost started; saved on the home screen

// Tip temperature history for the curve
void dogCurveReset();
#define DOG_CURVE_SAMPLE_TICKS (TICKS_100MS * 2 + TICKS_100MS / 2) // 4 samples a second: 23 s on screen
void dogCurveSample(uint16_t tipC, TickType_t interval = DOG_CURVE_SAMPLE_TICKS);
// One sample a column from x0, the newest on the right once full; the range follows the samples and the set point,
// dotted set line every 3rd pixel. Writes each sample's row to rows and returns how many there are
uint8_t dogDrawCurve(int16_t x0, uint8_t width, uint8_t y0, uint8_t y1, uint16_t setC, uint8_t *rows);

// Lifetime soldering time, counted while soldering mode runs the heater and saved when back on the home screen
void dogCountSolderTime();

// Seconds of boost left, for the boost screen (set by Soldering.cpp)
uint8_t dogBoostSecondsLeft();
// Button lock, switched on the home screen for the soldering sessions that follow
extern bool dogLockArmed;
// A setting changed while soldering: saved with the soldering time on the home screen, sparing the flash
void dogSettingsChanged();
#endif
#endif // UI_DRAWING_TS101_DOG_UI_HPP_

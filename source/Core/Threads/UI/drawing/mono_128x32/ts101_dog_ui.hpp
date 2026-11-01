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
void dogDrawRunAcross();                                // the dog runs across the stage of the whole screen
void dogDrawFlame(int16_t x);                           // blinking flame in the top row, marks soldering mode
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
#define DOG_KEY_ROW 24 // the key row; the menu puts its keys in the top row (0) instead
void       dogDrawCorners(const uint8_t *frontIcon, const uint8_t *rearIcon, uint8_t row = DOG_KEY_ROW); // icons only, inverted while pressed
// While held long enough, replaces the key row: text with a bar filling from the button's side. True if it drew
bool       dogDrawHold(const char *text, bool front, TickType_t full, uint8_t row = DOG_KEY_ROW);
bool       dogHoldDone(bool front, TickType_t full); // held long enough for the action

// The settings menu, drawn on its own for the preview
void dogDrawMenuList(uint8_t section, uint8_t pos);
void dogDrawMenuValue(uint8_t section, uint8_t pos);
#define DOG_MENU_ROOT 0xFF

const char *dogTipName(); // the tip type picked in the menu, in the small font

// The info pages (menu -> info); page 4 leads on to the raw debug values
#define DOG_INFO_PAGES 5
void dogDrawInfoPage(uint8_t page);

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
#define DOG_CURVE_SAMPLE_TICKS (TICKS_100MS * 2 + TICKS_100MS / 2) // 4 samples a second: 18 s on screen while soldering
#define DOG_COOL_SAMPLE_TICKS  (TICKS_SECOND * 5 / 2)                // one every 2.5 s: 3 minutes of cooling on the home screen
void dogCurveSample(uint16_t tipC, TickType_t interval = DOG_CURVE_SAMPLE_TICKS);
// The vertical range: round the set point, from room temperature, up to the boost target, from room temperature up to the hottest sample
enum class DogCurve { Steady, Heat, Boost, Cool };
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

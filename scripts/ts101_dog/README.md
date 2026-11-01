# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`).
The screens live in `source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_*`; they replace the dense 128x32
soldering, sleep and simple home screens, which are compiled out with the flag set.

Two buttons, one grammar: front is more, on or open, rear is less, next or back, and holding the rear button goes back or stops.
Values apply at once, there is nothing to confirm. Both buttons together stop at once on every heating screen, also locked and in boost.

| Screen | Front click | Rear click | Front hold | Rear hold |
|---|---|---|---|---|
| Start | solder | menu | nothing | nothing |
| Soldering | one solder hotter | one solder colder | boost, 30 s at most | stop after 0.5 s |
| Menu, section | open | next entry | nothing | a level up, or to Start |
| Menu, switch or choice of up to 5 | next value, blinks once | next entry | nothing | a level up, or to Start |
| Menu, number | change it | next entry | nothing | a level up, or to Start |
| Changing a number | + | - | + faster | back to the entry (also 2 s without a button) |
| Sleep | wake | wake | wake | heater off, to Start |

The screens follow the "TS101 Gegenentwürfe" boards on a 128x32 grid; `preview/extract_gegenentwuerfe.py` unpacks those boards
into reference images for `preview/compare.py`.

- **Fonts**: Barlow Condensed SemiBold for the large digits (rows 0-19, small degree ring), the IronOS 6x8 font for the small row
  and the menu, `fonts/label8x10.txt` for the mixed-case labels ("Bleifrei", "Löten"): taken from the boards, with the letters
  they have no sample of drawn in the same style. Barlow SemiBold for the words on full screen notices. `make_fonts.py` turns
  them into `ts101_dog_fonts.h/.cpp`; the licences are in `fonts/` (SIL OFL 1.1).
- **Start** (`ts101_dog_home.cpp`, bitmaps in `art/home.txt`): the dog, statistics changing every 2 s, the tip temperature with a
  trend arrow (from 0.3 C/s, off below 0.15 C/s), the curve of the last 10 minutes and the button pair next to the buttons. Held
  the other way round the blocks swap sides and the front button's box moves to the left; nothing is mirrored. The number blinks
  while hot if `COOLING TEMP BLINK` is on. The history is fed on every GUI frame, so the curve goes on through soldering.
- **Dog**: follows the tilt of the iron's long axis, 8 updates a second: the head (rows 0-11) turns by half the tilt, at most 12
  degrees, and moves a pixel from 6 degrees; from 45 degrees towards the tip it also drops a pixel. Shaken, the whole dog hops.
  The sensor axis is `DOG_ACCEL_ALONG` / `DOG_ACCEL_ALONG_SIGN` at the top of `ts101_dog_home.cpp`, **not yet checked on the iron**.
- **Statistics**: `GESAMT` (lifetime hours with the heater on in soldering mode), `AKTUELL` (soldering minutes since power on, not
  saved), `VORGÄNGE`, `BOOSTS`. Counters are saved on the Start screen only: no flash write with the heater on. Disconnecting
  power while soldering loses what was counted since the last time on Start.
- **Lock**: holding both buttons on Start switches it on or off for the sessions that follow, with a notice. With `Sperre` on Aus in the
  menu the gesture locks nothing and the notice says so; setting `Sperre` to Aus also drops an armed lock. To unlock: both buttons stop,
  then the same gesture on Start.
- **Heating**: the running dog on the curve, until the set point is reached the first time, and again after sleep.
- **Soldering**: the solder's name, watts and amps, or for 2 s after a change its temperature, alloy and position; the real tip temperature
  with an arrow at 2 degrees off, `ZIEL` or `BEREIT` below it, the curve with its dotted set line and the - and + boxes. No dog, also when
  heating up after a change of solder. A set point that is no solder shows as `Eigenes Ziel`. Holding the rear button shows `Halten: Stopp`.
- **Detail view** (menu switch `DETAILANSICHT`): source and volts, calculated watts and amps (~), handle, session time.
- **Boost**: inverted badge, the temperature, `NOCH n S`, the dog sprinting with speed lines, dust and a running ground. It ends after 30 s
  and needs a fresh press to start again; afterwards the soldering screen without the dog, also if it began during heating up. The
  legs are timed at 40 frames a second, the panel shows what the GUI loop gets to draw.
- **Presets** (`TS101_PRESETS_C`): 240 Sn42Bi58, 320 Sn63Pb37 and Sn60Pb40, 350 SAC305, 370 Sn99.3Cu0.7, 400 wires and large copper
  areas. Starting values for the tip, not datasheet figures: alloys that want the same tip temperature share one step.
- **Solders**: clicks right after a boost are ignored (upstream #1663). Changes made while soldering are saved on the home screen.
- **Menu** (`SettingsMenuDog.cpp`): one entry per screen: section and position dots on top, the name below, the value top right (a triangle
  on sections and actions), the next entry at the bottom, or a number's range while it changes. Top level: Eigenes Ziel, Löten, Schlaf,
  Anzeige, Sperre, Info, Zurücksetzen. Resetting and calibrating keep their own screens from the stock menu.
- **Info**: a section of the menu with five pages (power, tip, handle and side, statistics, system): the page's name small on top, because
  its two rows of label and value take the room of the large name. System opens the raw debug values.
- **Sleep**: the dog with closed eyes and three z rising, `SCHLAF`, `AUS IN n MIN` (empty if auto-off is disabled), the temperature as while
  soldering, `BEWEGEN WECKT`. The content moves 0, -1, -2, -1 px every 4 s against burning in. Waking shows three frames of the dog looking up,
  then heating with the running dog. Only letting go of the rear button wakes the iron, so a hold can finish as a stop even if it moves meanwhile.
- **Interruptions**: full screen with what is wrong and what it means: no tip, undervoltage, thermal runaway, handle too hot
  (`DOG_HANDLE_LIMIT_X10C`, 70 C for 2 s: the heater stops, and the iron won't start until the handle is cooler; the handle
  NTC table is extended up to 100 C for this), button lock.
- **Languages**: German texts in the DE build, English in every other build (the fonts only have Latin letters).
- **128x32 boot logo**: animated logo format `0xAB` (see `source/Core/Drivers/BootLogo.cpp`), stored in the 5K at
  95K-100K. The app is limited to 95K (`flash_size` in `source/Makefile`); every language fits (CJK ~92K).
  The animation is drawn upright, for holding the iron with the tip up: the dog with cap, sunglasses and bag walks in from
  the cable end and stands there smoking.

## Building

```sh
cd source
make -j8 model=TS101 firmware-DE
python3 ../scripts/ts101_dog/logo128.py Hexfile/TS101_DE.hex TS101_DE_HUND.hex preview.gif
```

`logo128.py` merges the firmware with the animated logo into one hex file, since the stock bootloader
only takes one file. `make_sprites.py` regenerates `ts101_dog_sprites.h/.cpp` after changing the bitmaps in `art/` or the drawings in
`gsd_extra.py`. The scripts need Pillow.

The `TS101 German Shepherd build` workflow (`.github/workflows/ts101-dog.yml`) builds the German and English
firmware with the logo on every push to `ts101-schaeferhund`.

## Previewing screens on the host

`preview/build.sh <out dir>` compiles the real drawing code against stubs for the RTOS and hardware
(`preview/stubs`), renders the scenes from `preview/preview_state.cpp` as `.pbm` files, and
`preview/sheet.py <out dir> sheet.png` puts them on one sheet, `preview/compare.py <out dir> <mock dir> out.png` next to
the mocks with the count of differing pixels. `preview/test_buttons.sh` runs the real button logic over simulated time
and checks rule 2 on every screen and the #1663 case. `preview/page.py <out dir> <out dir EN> out.html` puts every screen with
the rules and the button table on one page. `preview/check_home.py <out dir>` checks the home screen pixel by pixel (button
pair, dog at rest and when tilted, statistics limit, arrow, curve). Needs `clang++` and a built `source/Core/Gen`.

## Flashing from macOS

The stock DFU bootloader rarely accepts a file written through the macOS FSKit FAT driver.
`scripts/flash_ts101_macos.sh` mounts the drive with the legacy `msdosfs` kext and retries in the same
DFU session until it gets a `.RDY`:

```sh
sudo bash scripts/flash_ts101_macos.sh TS101_DE_HUND.hex TS101.HEX
```

# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`).
The screens live in `source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_*`; they replace the dense 128x32
soldering, sleep and simple home screens, which are compiled out with the flag set.

Two buttons, five rules: the front button (tip end) acts (starts, opens, raises), the rear button (cable end) leads
(moves on, lowers); holding the rear button leaves, on every screen; what a button does is written next to it and moves
with it when the screen turns; a click never changes the heater; the screen shows by itself what matters.
The screens use a 128x32 grid: stage on rows 0-22, row 23 for umlaut dots, key row on rows 24-31.

- **Fonts**: Barlow Condensed SemiBold for the large digits (small degree ring, no C), Silkscreen for the small row
  (umlaut dots set by hand), Barlow SemiBold for menu values. `make_fonts.py` turns `fonts/*.ttf` into
  `ts101_dog_fonts.h/.cpp`; the licences are in `fonts/` (SIL OFL 1.1).
- **Key row** (`ts101_dog_keys.cpp`): reads `getButtonA/getButtonB` directly. A pressed button inverts its field; held, the
  row fills from that button's side with what will happen, letting go cancels. A press that did its job (a stop, a wake-up)
  counts for nothing more until released.
- **Home**: dog head, the set point (HEISS and the tip while it is still hot), key row LÖTEN | supply | MENÜ.
  Front click solders, rear click opens the menu, front hold opens the temperature, rear hold does nothing.
- **Soldering**: heating up, the dog runs and the curve grows in the bottom row; at temperature no dog, but set point, input
  volts and handle temperature, a 27 px curve and the tip temperature; the bottom row changes by itself: WUFF! BEREIT,
  session time, watts with a load bar, sleep countdown. Rear click: the second view, a 72 px curve with watts, volts and
  calculated amps (~). Front hold: boost while held, the dog runs across. Rear hold: stop after a 1 s bar.
- **Temperature**: front click while soldering opens it, the heater stays on. Click +/- one step (setting step size,
  10 by default), front hold one preset further (300/350/400, `TS101_PRESETS_C`), never to the maximum; on a preset the
  use is shown instead of the curve. Closes after 2 s or with rear hold. Clicks right after a boost are ignored
  (upstream #1663). Changes made while soldering are saved on the home screen.
- **Menu** (`SettingsMenuDog.cpp`): Temperatur, Info, Löten, Schlaf, Strom, Anzeige, System, at most two levels, shown as
  lists of three rows (name left, value right, the picked row inverted, a scroll bar on the right edge). The rear button
  always goes back, click or hold; a front click moves to the next row, holding it opens a list, flips a switch (on/off
  settings and the unit) or makes the value in the row editable: then a front click raises it (wrapping round at the top),
  holding it raises it faster, the rear button is done. Each value saves itself when it is left. After 3 s a help text
  runs through the key row.
- **Info**: five pages (power, tip, handle and side, statistics, system), the same buttons as the menu; holding the front
  button on System shows the raw debug values.
- **Sleep**: the dog lies down; tip temperature, time asleep, HEBEN!. Any button wakes the iron and does nothing else.
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
and checks rule 2 on every screen and the #1663 case. Needs `clang++` and a built `source/Core/Gen`.

## Flashing from macOS

The stock DFU bootloader rarely accepts a file written through the macOS FSKit FAT driver.
`scripts/flash_ts101_macos.sh` mounts the drive with the legacy `msdosfs` kext and retries in the same
DFU session until it gets a `.RDY`:

```sh
sudo bash scripts/flash_ts101_macos.sh TS101_DE_HUND.hex TS101.HEX
```

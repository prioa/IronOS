# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`).
The screens live in `source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_*`; they replace the dense 128x32
soldering, sleep and simple home screens, which are compiled out with the flag set.

Two buttons, one grammar: a click is one step (front forwards or +, rear on or -), holding the front button acts, holding the
rear button leaves, on every screen. What a button does is written next to it and moves with it when the screen turns. A
click never changes the heater; both buttons together stop at once while soldering, also when locked.

| Screen | Front click | Rear click | Front hold | Rear hold |
|---|---|---|---|---|
| Start | solder | menu | temperature | info |
| Soldering | temperature | temperature | boost, 30 s at most | stop after 0.5 s |
| Temperature | +5° | -5° | next preset | done |
| Menu list | open or change | next row | nothing | back |
| Value | + | - | + faster | done |
| Info | next page | previous page | on System: debug | back |
| Sleep | wake | wake | wake | heater off, to Start |

The screens follow the "TS101 Gegenentwürfe" boards on a 128x32 grid; `preview/extract_gegenentwuerfe.py` unpacks those boards
into reference images for `preview/compare.py`.

- **Fonts**: Barlow Condensed SemiBold for the large digits (rows 0-19, small degree ring), the IronOS 6x8 font for the small row
  and the menu, `fonts/label8x10.txt` for the mixed-case labels ("Bleifrei", "Löten"): taken from the boards, with the letters
  they have no sample of drawn in the same style. Barlow SemiBold for the words on full screen notices. `make_fonts.py` turns
  them into `ts101_dog_fonts.h/.cpp`; the licences are in `fonts/` (SIL OFL 1.1).
- **Start**: the preset's name (or "Soll"), the supply (`PD 20V·65W`, marked with ! when weak), the set point, `Menü` and `Löten`
  next to their buttons. The menu switch `START MIT HUND` picks the variant with the dog head, which also runs and barks when the
  iron is picked up. While the tip is still hot: a warning triangle, the cooling curve down to a dotted 50 C and the tip.
  The lifetime soldering time is on the Info statistics page.
- **Lock**: holding both buttons on Start switches it on or off for the sessions that follow, with a notice, as long as `SPERRE`
  in the menu isn't off; that entry also sets what it locks. To unlock: both buttons stop, then the same gesture on Start.
- **Soldering**: heating up, the running dog with the power bar; with a weak supply no dog, the set point, a flat curve and an
  inverted row `Netzteil 9V·13W`. At temperature: check mark and `Bereit`, or `Soll 350` under load, the curve below the set line,
  the power bar. Reaching the set point inverts the screen for 200 ms. `SCHLAF IN n` takes the top line before sleep.
  Holding the rear button shows a block `Halten: Stopp` with a line filling from the rear side.
- **Detail view** (menu switch `DETAILANSICHT`): source and volts, calculated watts and amps (~), handle, session time.
- **Boost**: inverted `BOOST`, the target and the seconds left, no dog. It ends after 30 s and needs a fresh press to start again.
- **Temperature**: `Soll`, the set point, the preset's use, a scale from 100 to 400 C. Closes after 2 s or with rear hold. Clicks right
  after a boost are ignored (upstream #1663). Changes made while soldering are saved on the home screen.
- **Menu** (`SettingsMenuDog.cpp`): Temperatur, Info, Löten, Schlaf, Strom, Anzeige, System, at most two levels. Top row: next-row
  arrow, title with position (SCHLAF 2/4), OK; below it three rows. A value being changed shows -, a bar with the time left and +;
  it saves itself after 4.5 s without a button. Help text after 3 s uses the top row.
- **Info**: five pages (power, tip, handle and side, statistics, system); holding the front button on System shows the raw values.
- **Sleep**: the dog lies down; tip temperature, `AUS 7:19` until the heater goes off (empty if auto-off is disabled). Only letting go
  of the rear button wakes the iron, so a hold can finish as a stop even if the iron moves meanwhile.
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
the rules and the button table on one page. Needs `clang++` and a built `source/Core/Gen`.

## Flashing from macOS

The stock DFU bootloader rarely accepts a file written through the macOS FSKit FAT driver.
`scripts/flash_ts101_macos.sh` mounts the drive with the legacy `msdosfs` kext and retries in the same
DFU session until it gets a `.RDY`:

```sh
sudo bash scripts/flash_ts101_macos.sh TS101_DE_HUND.hex TS101.HEX
```

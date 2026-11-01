# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`).
The screens live in `source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_*`; they replace the dense 128x32
soldering, sleep and simple home screens, which are compiled out with the flag set.

- **Soldering, simple view**: large tip temperature, below it a heat-up bar with the set point, then the session
  time once the set point is reached (survives sleep and set point changes, resets on the home screen).
  While heating up a dog runs next to it; at temperature the dog head from the photo is there: barks "WUFF!" when the set point is reached, pants while the heater works
  hard, flicks an ear after waking up and every few seconds.
- **Soldering mode marker**: a blinking flame in the top corner in every soldering view; the other screens have none.
- **Boost**: the dog runs across the whole screen, below it the tip temperature and the boost target.
- **Soldering, curve view**: 18 s tip temperature curve (zoomed in round the set point, dotted set point line),
  large tip temperature, segmented power bar, bottom row watts | volts | amps.
- **Buttons while soldering**: front (+) click steps through the presets 300 / 350 / 400 °C
  (`TS101_PRESETS_C` / `TS101_PRESETS_F`) with a full screen notice that says what the preset is for (`TS101_PRESET_USES`); front hold = boost; back (-) click steps through
  the two views (dog, curve; setting `DogSolderView`); both buttons or back hold = home. The free set
  point adjustment is on the home screen (front hold), as upstream. Presets and the view are saved on the home screen, not on every click, to spare the flash.
- **Sleep**: the dog lies down and closes its eyes, "z Z Z" drifts up; tip temperature, input voltage and time asleep.
- **Home**: dog head from the photo, the set point large with a thermometer (the tip temperature with a small down arrow, blinking with it, while it is still hot),
  and a status bar with play (start soldering) and gear (settings) hints next to their buttons. Between them an item
  changes every 3 s: "+ LÖTEN - MENÜ", input volts, lifetime soldering time (Σ h:mm, setting `TotalSolderMinutes`,
  counted at temperature and saved on the home screen), last session, power limit, handle temperature, tip resistance,
  firmware version. Picking the iron up after it lay still makes the dog run across and bark. Without a tip a full screen
  "SPITZE FEHLT!" notice. The detailed home view adds the lifetime time as Σ h:mm.
- **Warnings**: a warning triangle next to warnings narrow enough to leave room for it.
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
`preview/sheet.py <out dir> sheet.png` puts them on one sheet. Needs `clang++` and a built `source/Core/Gen`.

## Flashing from macOS

The stock DFU bootloader rarely accepts a file written through the macOS FSKit FAT driver.
`scripts/flash_ts101_macos.sh` mounts the drive with the legacy `msdosfs` kext and retries in the same
DFU session until it gets a `.RDY`:

```sh
sudo bash scripts/flash_ts101_macos.sh TS101_DE_HUND.hex TS101.HEX
```

# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`).
The screens live in `source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_*`; they replace the dense 128x32
soldering, sleep and simple home screens, which are compiled out with the flag set.

- **Soldering, simple view**: large tip temperature, below it a heat-up bar with the set point, then the session
  time once the set point is reached (survives sleep and set point changes, resets on the home screen).
  Next to it a sitting German Shepherd: barks "WUFF!" when the set point is reached, pants while the heater works
  hard, wags after waking up and every few seconds.
- **Soldering, detailed view**: tip temperature with an 18 s curve of the tip temperature (zoomed in round the
  set point, dotted set point line), one info row: watts, input volts, set point, session time.
- **Presets**: double click the front (+) button while soldering to step through 300 / 350 / 400 °C
  (`TS101_PRESETS_C` / `TS101_PRESETS_F`). A single click still opens the set point screen, about 0.4 s later.
- **Sleep**: the dog lies down, falls asleep, "z Z Z" drifts up; tip temperature next to it.
- **Home**: start soldering pictogram (or the tip temperature while hot) and the dog, which barks when the iron
  is picked up after lying still and looks sad without a tip. The detailed home view adds the lifetime soldering
  time (Σ h:mm, setting `TotalSolderMinutes`, counted at temperature and saved when back on the home screen).
- **Warnings**: a sad dog next to warnings narrow enough to leave room for it.
- **128x32 boot logo**: animated logo format `0xAB` (see `source/Core/Drivers/BootLogo.cpp`), stored in the 5K at
  95K-100K. The app is limited to 95K (`flash_size` in `source/Makefile`); every language fits.

## Building

```sh
cd source
make -j8 model=TS101 firmware-DE
python3 ../scripts/ts101_dog/logo128.py Hexfile/TS101_DE.hex TS101_DE_HUND.hex preview.gif
```

`logo128.py` merges the firmware with the animated logo into one hex file, since the stock bootloader
only takes one file. `make_sprites.py` regenerates `ts101_dog_sprites.h/.cpp` after changing the drawings in
`gsd32.py`. The scripts need Pillow.

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

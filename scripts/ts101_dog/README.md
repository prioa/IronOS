# TS101 German Shepherd UI

Custom IronOS build for the Miniware TS101 (`TS101_DOG_UI` in `source/Core/BSP/Miniware/configuration.h`):

- **Sleeping dog**: sleep mode shows a lying German Shepherd with drifting "z Z Z" next to the tip temperature.
- **128x32 boot logo**: new animated logo format `0xAB` (see `source/Core/Drivers/BootLogo.cpp`), stored in
  the 5K at 95K-100K. The app is limited to 95K (`flash_size` in `source/Makefile`).
- **Heat-up bar**: bottom right of the simple soldering screen, until the set point is reached.
- **Soldering timer**: replaces the bar once the set point is reached; survives sleep and set point changes,
  resets on the home screen.
- **Presets**: double click the front (+) button while soldering to step through 300 / 350 / 400 °C
  (`TS101_PRESETS_C` / `TS101_PRESETS_F`). A single click still opens the set point screen, about 0.4 s later.

## Building

```sh
cd source
make -j8 model=TS101 firmware-DE
python3 ../scripts/ts101_dog/logo128.py Hexfile/TS101_DE.hex TS101_DE_HUND.hex preview.gif
```

`logo128.py` merges the firmware with the animated logo into one hex file, since the stock bootloader
only takes one file. `make_sprites.py` regenerates
`source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_sprites.h` after changing the drawings in `gsd32.py`.
The scripts need Pillow.

## Flashing from macOS

The stock DFU bootloader rarely accepts a file written through the macOS FSKit FAT driver.
`scripts/flash_ts101_macos.sh` mounts the drive with the legacy `msdosfs` kext and retries in the same
DFU session until it gets a `.RDY`:

```sh
sudo bash scripts/flash_ts101_macos.sh TS101_DE_HUND.hex TS101.HEX
```

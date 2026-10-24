# TamaPoke in VS Code (PlatformIO) - step-by-step guide

For the **ELEGOO 2.8" / ESP32-2432S028R ("CYD")** board. The round Waveshare board
is also included (`waveshare-round` environment).

> Status: the code has NEVER been compiled. The first build may produce errors; see
> step 8.

## Required extensions

Just one:

| Extension | ID | What for |
|---|---|---|
| **PlatformIO IDE** | `platformio.platformio-ide` | build, flash and serial monitor |

When you open the folder, VS Code will suggest installing it (it is in `.vscode/extensions.json`).
PlatformIO installs the compiler, the ESP32 core and the libraries (Arduino_GFX, U8g2) on its own.
Microsoft's C/C++ extension is installed automatically as a dependency (autocomplete).
Do **not** install Microsoft's "Arduino" extension: it clashes with PlatformIO.

## Prerequisites

- **VS Code** (latest version) and **Python 3.8+** on the PATH (PlatformIO needs it; on
  Windows, tick "Add Python to PATH" when installing it).
- **Git** installed (PlatformIO downloads the pioarduino platform and some libraries).
- A **data** USB cable (not charge-only) and, on Windows, the **CH340** driver (or CP210x
  depending on your unit) if the COM port does not show up.
- A **FAT32 microSD** card with the sprites (step 9).

## Steps

1. **Unzip** `TamaPoke-VSCode.zip` to a short path with no accented characters
   (e.g. `C:\dev\TamaPoke`).
2. **Open VS Code** → *File → Open Folder…* → choose the `TamaPoke` folder (the one that
   contains `platformio.ini`). If it asks "Do you trust the authors", choose *Yes*.
3. **Install PlatformIO IDE** when VS Code suggests it (or: *Extensions* → search for
   "PlatformIO IDE" → Install). Restart VS Code if asked.
4. **Wait for initialization.** The first time, PlatformIO downloads the platform
   (several hundred MB) and it can take 5-15 min. Watch the bottom bar; do not close VS Code.
   The blue/bottom bar will show the PlatformIO icons (✓ build, → upload, plug = monitor).
5. **Pick the environment:** in the status bar at the bottom, click the
   `Default (TamaPoke)` selector and choose **`env:cyd-rect`**.
6. **Build:** click the ✓ (Build) icon or press `Ctrl+Alt+B`, or in a terminal:
   `pio run -e cyd-rect`.
   It should end with `SUCCESS`. The first build is slow.
7. **Flash:** connect the board over USB and click → (Upload), or
   `pio run -e cyd-rect -t upload`. If it fails with "failed to connect": hold down the
   board's **BOOT** button while the upload starts, and check that the COM port is
   the right one (`pio device list`).
8. **Serial monitor:** plug icon, or `pio device monitor` (115200 baud). You will see
   lines like `CYD heap: libre=... mayor_bloque=...` and `cargado ... en streaming`.
   **If the build fails**, copy the text from the terminal; it is most likely
   a small fix.
9. **Prepare the SD with the sprites** (without them the pet uses fallback drawings):
   ```
   python tools/pack_pmd.py       # downloads and packs the sprites -> tools/sdcard/mons/
   python tools/make_thumbs.py    # gallery thumbnails
   ```
   Copy the `mons` folder to the root of the microSD (FAT32): you will end up with
   `/mons/p001.bin`, `/mons/thumbs.bin`, etc. (The sprites are CC BY-NC: personal use
   only.) Details in `README.md`, sprites section.
10. **Calibrate the touch:** build and flash `env:cyd-rect-touchdebug`, open the monitor,
    touch the four corners and adjust `RAW_MIN/RAW_MAX` and the
    `TOUCH_SWAP_XY / TOUCH_INVERT_X / TOUCH_INVERT_Y` flags in `touch_hal_cyd.cpp`.
    Then switch back to `env:cyd-rect`.

## If something goes wrong

| Symptom | Likely cause |
|---|---|
| `region 'iram0_0_seg'` / `app` overflows / "text section exceeds" | the binary does not fit in 4 MB; see *Flash size* in `CYD_PORT_NOTES.md` |
| The COM port does not show up | charge-only cable or missing CH340/CP210x driver |
| Blank or mirrored screen | panel pins/orientation (`boards/pin_config_cyd.h`) |
| Touch inverted or offset | calibration (step 10) |
| Flicker | the full framebuffer does not fit; it is the direct drawing mode (see notes) |
| `fatal error: ESP_I2S.h` | the core is not 3.x; do not change the `platform =` line in `platformio.ini` |

## Structure

- `platformio.ini` - `cyd-rect`, `cyd-rect-touchdebug`, `waveshare-round` environments
- `TamaPoke.ino` + `*.cpp/*.h` - firmware (in the root; `src_dir = .`)
- `boards/` - board selection, pins and 320x240 layout table
- `CYD_PORT_NOTES.md` - status, known limitations and extension points
- `.vscode/` - recommended extension and tasks (Terminal → Run Build Task = build CYD)

# CYD (ESP32-2432S028R / ELEGOO 2.8") port — status notes

**Status: second pass (streaming sprites + redesigned menus), still NOT compiled, NOT run on hardware.** The sandbox this was written in
could not reach the Arduino/Espressif toolchain hosts, so nothing here has been built. Static
checks done: preprocessor/brace/paren balance, no leftover Waveshare-only symbols outside
their guards, and every `SX()/SY()/GAL_*` macro reduces to the original literal on the
Waveshare board (so the original build should be unchanged — still unverified by a compile).

Select the board with `-D BOARD_CYD_RECT` (PlatformIO `-e cyd-rect`) or by uncommenting the
`#define` at the top of `boards/board_select.h` (Arduino IDE). No flag = original Waveshare.
Requires ESP32 Arduino core **3.x** (same as the original: `ESP_I2S.h`).

## Architecture

| Concern | Waveshare (round) | CYD (rect) |
|---|---|---|
| Display | CO5300 QSPI 466x466 | ILI9341 SPI (HSPI) 320x240 @ 40 MHz |
| Touch | CST9217 I2C, INT-gated (`touch_hal_waveshare.cpp`) | XPT2046, **bit-banged** (`touch_hal_cyd.cpp`) |
| Clock | PCF85063 RTC | NVS epoch + `millis()` (`rtcbat_cyd.cpp`) |
| Battery | AXP2101 | none (stubs: no battery, USB present) |
| Audio | ES8311 over I2S | PWM tones, same note tables (`audio_cyd.cpp`) |
| SD | SD_MMC 1-bit | SPI on pins 18/19/23, CS 5 |

Touch is bit-banged because the CYD spreads three SPI devices over two buses; a hardware SPI
touch driver would fight the SD card.

## Known problems, most important first

1. **Sprites stream from the SD (no PSRAM needed) — implemented, untested.** Without PSRAM
   (`!psramFound()`, decided at runtime so the Waveshare path is untouched) `PmdMon` keeps only
   the per-action headers in RAM and `frame()` reads ONE frame from the SD into a small
   reusable buffer (last frame cached); `SdThumbs` keeps just the offsets table and reads a
   thumbnail per `get()`. Cost: a sequential pass over the sprite file when a species loads
   (to find each action's lowest pixel row) — expect roughly 0.3 s on average and a few
   seconds for the largest files — plus one small SD read per animation frame. The file is
   opened per read (no retained handle), so USB `PUT` never conflicts. Watch the serial lines
   `cargado ... en streaming` and `miniaturas en streaming`. Check that animation is smooth at
   SPI-SD speed; if not, the next step is caching all frames of the current action.
2. **Full-screen framebuffer (150 KB) is split into bands.** Measured on hardware: free heap
   ~304 KB but largest block only ~110 KB, so a single `Arduino_Canvas` never fits and the
   old fallback (drawing straight to the panel) flickered. `BandCanvas` (`band_canvas.h`)
   allocates four 64-row bands separately; the scene is drawn once and `flush()` sends the
   bands in order. Watch for `CYD: framebuffer en bandas OK` on boot; if the bands do not
   fit it still falls back to direct drawing (`framebuffer NO cabe`).
3. **Menus were redesigned for 320x240 (untested on hardware).** Every menu/dialog box, its
   text sizes and its touch hit-areas come from ONE table in `boards/layout.h` (round board =
   the original literals, rect board = new values), so drawing and touch cannot drift apart:
   starter select, feed picker, release dialog, evolve/farewell choice, CTA buttons, clock
   setup, rename keyboard, stat card pages, celebration banner, training sack, minigame HUD.
   On the rect board `TS(n)` shrinks text sizes (7→5, 4-6→3, 3→2) and `ctext()/fitSize()`
   shrink further so text fits its box. Two small behaviour fixes apply to both boards: the feed
   picker clamps its item index (the exact right edge used to give index 4), and the sprite
   zoom floor is 1 on the rect board. Still to eyeball on real hardware: header/streak/battery
   placement, and anything not listed above (egg screen, bath).
4. **Touch is uncalibrated.** Orientation and raw range are a best guess. Build with
   `-DTOUCH_DEBUG`, tap the four corners, then adjust `RAW_MIN/RAW_MAX` and the
   `TOUCH_SWAP_XY / TOUCH_INVERT_X / TOUCH_INVERT_Y` flags in `touch_hal_cyd.cpp`.
5. **Clock:** survives reboot, but time while unpowered is not counted (no backup cell).
   `rtcEpoch()` persists to NVS every 10 min as a side effect (low flash wear).
6. **Flash size:** 4 MB flash with `huge_app` partition; the binary (sprite tables + CJK
   fonts) may still not fit. Unknown until a build is attempted.
7. **Pin table is from the community CYD pinout**, not from ELEGOO's datasheet for your exact
   unit. Verify, and check whether your unit has PSRAM (it changes problems 1-2).

## Extension points (battery / audio add-ons)
`rtcbat_cyd.cpp` and `audio_cyd.cpp` are the only files to replace when adding a DS3231 +
battery module or an I2S DAC; the headers (`rtcbat.h`, `audio.h`) do not change.

# CYD (ESP32-2432S028R / ELEGOO 2.8") port - status notes

**Status: builds with PlatformIO (`pio run -e cyd-rect`) and runs on a real ELEGOO unit**
(display, touch, SD sprites, speaker, BOOT button). Sprites stream from the SD and the menus
use a dedicated 320x240 layout.

Select the board with `-D BOARD_CYD_RECT` (PlatformIO `-e cyd-rect`) or by uncommenting the
`#define BOARD_CYD_RECT` at the top of `boards/board_select.h` (Arduino IDE). Building with no
board define stops with `#error`; the Waveshare uses `-D BOARD_WAVESHARE_ROUND`
(`-e waveshare-round`). Requires ESP32 Arduino core **3.x** (`ESP_I2S.h`); `platformio.ini`
pins the pioarduino release.

## Architecture

Pins, capability flags (`BOARD_HAS_PSRAM`, `BOARD_HAS_RTC_CHIP`, `BOARD_TOUCH_XPT2046`, ...),
display settings and the touch calibration are in `boards/board_cyd_rect.h`.

| Concern | Waveshare (round) | CYD (rect) |
|---|---|---|
| Display | CO5300 QSPI 466x466 | ILI9341 SPI (HSPI) 320x240 @ 40 MHz |
| Touch | CST9217 I2C, INT-gated (`touch_hal_waveshare.cpp`) | XPT2046, **bit-banged** (`touch_hal_cyd.cpp`) |
| Clock | PCF85063 RTC | NVS epoch + `millis()` (`rtcbat_cyd.cpp`) |
| Battery | AXP2101 | none (stubs: no battery, USB present) |
| Power key | AXP2101 PWR button | BOOT button (GPIO0) |
| Audio | ES8311 over I2S | PWM tones, same note tables (`audio_cyd.cpp`) |
| SD | SD_MMC 1-bit | SPI on pins 18/19/23, CS 5 |
| WiFi | yes | compiled out (`BOARD_HAS_WIFI 0`: no RAM for it next to the framebuffer) |

Touch is bit-banged because the CYD spreads three SPI devices over two buses; a hardware SPI
touch driver would fight the SD card.

## Known limitations, most important first

1. **Sprites stream from the SD (no PSRAM needed).** Without PSRAM (`!psramFound()`, decided
   at runtime so the Waveshare path is untouched) `PmdMon` keeps only the per-action headers
   in RAM and `frame()` reads ONE frame from the SD into a small reusable buffer (last frame
   cached); `SdThumbs` keeps just the offsets table and reads a thumbnail per `get()`. Cost: a
   sequential pass over the sprite file when a species loads (to find each action's lowest
   pixel row) - roughly 0.3 s on average and a few seconds for the largest files - plus one
   small SD read per animation frame. The file is opened per read (no retained handle), so USB
   `PUT` never conflicts. Watch the serial lines `loaded ... streaming` and
   `thumbnails streaming`. If animation is not smooth at SPI-SD speed, the next step is
   caching all frames of the current action.
2. **Full-screen framebuffer (150 KB) is split into bands.** Measured on hardware: free heap
   ~304 KB but largest block only ~110 KB, so a single `Arduino_Canvas` never fits and the
   old fallback (drawing straight to the panel) flickered. `BandCanvas` (`band_canvas.h`)
   allocates the frame as separate 32-row bands; the scene is drawn once and `flush()` sends
   the bands in order. Watch for `CYD: banded framebuffer OK, free heap` on boot; if the bands
   do not fit it still falls back to direct drawing
   (`framebuffer does NOT fit -> drawing directly (flicker)`).
3. **Menus are redesigned for 320x240.** Every menu/dialog box, its text sizes and its touch
   hit-areas come from ONE table per screen (`boards/layout_round_466.h` = the original
   literals, `boards/layout_rect_320x240.h` = new values, shared formulas in
   `boards/layout_common.h`), so drawing and touch cannot drift apart. On the rect board
   `TS(n)` shrinks text sizes (7->5, 4-6->3, 3->2) and `ctext()/fitSize()` shrink further so
   text fits its box. The sprite zoom floor is 1 on the rect board (`PMD_MIN_SCALE`).
4. **Touch calibration** was measured on the real unit. If your unit differs, build
   `-e cyd-rect-touchdebug`, tap the four corners, then adjust `TOUCH_RAW_*` and the
   `TOUCH_SWAP_XY / TOUCH_INVERT_X / TOUCH_INVERT_Y` flags in `boards/board_cyd_rect.h`.
5. **Clock:** survives reboot, but time while unpowered is not counted (no backup cell).
   `rtcPersistIfDue()` writes the epoch to NVS at most every 10 min (low flash wear), called
   from the loop's idle path; `rtcEpoch()` itself never writes.
6. **Flash size:** 4 MB flash with the `huge_app` partition (3 MB app, no OTA); the firmware
   fits with room to spare (about 0.9 MB).
7. **Pin table is from the community CYD pinout**; it matches the tested ELEGOO unit. Other
   clones may differ (check whether yours has PSRAM: it changes limitations 1-2).

## Extension points (battery / audio add-ons)
`rtcbat_cyd.cpp` and `audio_cyd.cpp` are the only files to replace when adding a DS3231 +
battery module or an I2S DAC; set the matching capability flags in `boards/board_cyd_rect.h`.
The headers (`rtcbat.h`, `audio.h`) do not change.

#pragma once

// ESP32-2432S028R, aka "Cheap Yellow Display" (CYD): the board ELEGOO (and many
// others) sell as "ESP32 2.8 Inch Touch Screen Display". Classic ESP32, 4 MB
// flash, NO PSRAM. Included only through boards/board_select.h.
// Reference pinout, stable across clones:
// https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
// Display, touch, SD, speaker and BOOT button run on a real ELEGOO unit with
// these pins; the RGB LED and the LDR are not used by the firmware.

#pragma region Capability flags (every board header defines all of them as 0 or 1)
#define BOARD_IS_ROUND 0
#define BOARD_DISPLAY_CO5300_QSPI 0
#define BOARD_DISPLAY_ILI9341_SPI 1
#define BOARD_TOUCH_CST9217 0
#define BOARD_TOUCH_XPT2046 1
#define BOARD_SD_SDMMC 0
#define BOARD_SD_SPI 1
#define BOARD_AUDIO_I2S_ES8311 0
#define BOARD_AUDIO_LEDC 1
#define BOARD_HAS_PMU 0
#define BOARD_HAS_RTC_CHIP 0
#define BOARD_HAS_BATTERY 0
// Not BOARD_HAS_PSRAM: the Arduino core tests that name with #ifdef, so even a 0 would count
#define BOARD_HAS_PSRAM_CHIP 0
#define BOARD_USES_BAND_CANVAS 1
// No WiFi: without PSRAM the WiFi stack (~50 KB at runtime, ~25 KB static)
// does not fit next to the 150 KB framebuffer, and game performance comes first
#define BOARD_HAS_WIFI 0
#define BOARD_USB_CDC_SERIAL 0  // USB-UART bridge chip: the UART TX drains on its own
#define BOARD_HAS_I2C_BUS 0
#define BOARD_HAS_BOOT_POWER_KEY 1
#pragma endregion

#pragma region Display: ILI9341 320x240 over 4-wire SPI on the HSPI bus
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1  // no dedicated reset pin (tied to EN)
#define TFT_BL 21   // backlight, active high, driven with PWM
// Arduino_ESP32SPI / Arduino_ILI9341 constructor arguments: pins 14/12/13/15 are
// exactly HSPI's defaults; rotation 1 = landscape; not an IPS panel
#define BOARD_DISPLAY_SPI_HOST HSPI
#define BOARD_DISPLAY_ROTATION 1
#define BOARD_DISPLAY_IPS false
#define BOARD_DISPLAY_SPEED_HZ 40000000  // ILI9341's reliable maximum
// Backlight PWM (per-pin ledc API of ESP32 Arduino core 3.x)
#define BOARD_BACKLIGHT_PWM_FREQUENCY_HZ 5000
#define BOARD_BACKLIGHT_PWM_RESOLUTION_BITS 8
#pragma endregion

#pragma region Resistive XPT2046 touch (its own pins, bit-banged in touch_hal_cyd.cpp)
#define TOUCH_CS 33
#define TOUCH_IRQ 36  // input-only GPIO, no internal pull-up; low while touched
#define TOUCH_CLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#pragma endregion

#pragma region XPT2046 touch calibration
// Calibrated on the real unit (landscape, rotation 1) with -DTOUCH_DEBUG, from
// the four bottom buttons (x=40/120/200/280, y=218) plus a tap at the center:
// the raw X channel follows the screen's Y and vice versa (hence SWAP), neither
// axis is mirrored. ~11.2 raw units per px horizontally, ~15.1 vertically; the
// ranges below are those lines extrapolated to the screen edges.
// If touches come out rotated/mirrored, change TOUCH_SWAP_XY/INVERT; if they are
// off at the edges, adjust TOUCH_RAW_*. To see the raw values, build the
// cyd-rect-touchdebug env and touch the four corners while watching serial.
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 1
#endif
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 0
#endif
// per-axis raw ranges, in screen axes (applied after the swap)
#define TOUCH_RAW_X_MIN 166
#define TOUCH_RAW_X_MAX 3726
#define TOUCH_RAW_Y_MIN 260
#define TOUCH_RAW_Y_MAX 3875
#pragma endregion

#pragma region microSD over SPI: default VSPI bus pins (the display is on HSPI)
#define SD_SCLK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
#pragma endregion

#pragma region Speaker: bare GPIO pin, no I2S codec; PWM tones (ledc, audio_cyd.cpp)
#define SPEAKER_PIN 26
#pragma endregion

#pragma region BOOT button: GPIO0, active low, screen on/off key (rtcbat_cyd.cpp pwrShortPressed)
#define BOOT_BUTTON_PIN 0
#pragma endregion

#pragma region RGB LED (active low) and LDR: present on the board, not used by the firmware
#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17
#define LDR_PIN 34
#pragma endregion

// No PMU (AXP2101), no external RTC (PCF85063) and no battery: see
// rtcbat_cyd.cpp for the NVS-backed soft clock and the battery stubs.

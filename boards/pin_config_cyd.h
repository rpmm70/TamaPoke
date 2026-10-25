#pragma once

// Pins for the ESP32-2432S028R, aka "Cheap Yellow Display" (CYD): the board
// ELEGOO (and many others) sell as "ESP32 2.8 Inch Touch Screen Display".
// Reference pinout, stable across clones:
// https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
// TODO(verify): check against the datasheet of the specific ELEGOO unit.

// ILI9341 320x240 display over SPI, NO PSRAM and no QSPI
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1  // no dedicated reset pin (tied to EN)
#define TFT_BL 21   // backlight, active high, supports PWM

// Resistive XPT2046 touch (its own SPI bus on this board, its own CS)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_CLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39

// microSD: default VSPI bus pins (the display is on HSPI)
#define SD_SCLK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// Speaker: bare GPIO pin, no I2S codec; driven with PWM tones (ledc)
#define SPEAKER_PIN 26

// BOOT button: GPIO0, active low. Screen on/off key (rtcbat_cyd.cpp pwrShortPressed)
#define BOOT_BUTTON_PIN 0

// RGB LED (active low) and LDR: not used yet
#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17
#define LDR_PIN 34

// No PMU (AXP2101), no external RTC (PCF85063) and no battery: see
// rtcbat_cyd.cpp for the NVS-backed fallback clock and the battery stubs.

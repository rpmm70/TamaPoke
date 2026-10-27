#pragma once

// The display of the selected board: data bus, panel driver and the frame
// buffer every screen draws into (gfx), plus brightness and background fills.
//   BOARD_DISPLAY_CO5300_QSPI: round AMOLED, full Arduino_Canvas framebuffer in PSRAM
//   BOARD_DISPLAY_ILI9341_SPI: rect TFT, BandCanvas framebuffer in internal RAM
//                              (falls back to drawing straight on the panel)

#pragma region Includes
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#pragma endregion

#pragma region Frame buffer
// Everything is drawn here and then shown with gfx->flush(). Typed as the base
// Arduino_GFX so the CYD can fall back to the panel itself (flush() is virtual).
extern Arduino_GFX *gfx;
#pragma endregion

#pragma region Public API
// Initializes the panel and the frame buffer, then sets initialBrightness.
// Call after pmuEnablePanel(). On BOARD_USES_BAND_CANVAS boards it also mounts
// the SD card (sdBegin) BEFORE allocating the bands, because mounting needs one
// ~14 KB contiguous block that the bands would take; other boards mount the SD
// later in setup().
void displayBegin(uint8_t initialBrightness);
// Panel brightness 0..255: the CO5300 controls it itself; the CYD drives the backlight PWM
void boardSetBrightness(uint8_t brightness);
// Full-screen background. On the round board it is a circle with the bezel's
// radius; on a rectangular one there is no bezel to mimic: fills everything.
void fillScreenBg(uint16_t color);
// Same pixels as gfx->fillScreen(RGB565_BLACK) followed by fillScreenBg(UI_BG_DAY):
// black outside the bezel circle on the round board, a single fill on the rect board.
void clearScreenToDayBackground();
#pragma endregion

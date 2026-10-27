#pragma once

// Selects the target board at compile time. Exactly one identity flag must be
// defined:
//   -D BOARD_WAVESHARE_ROUND  -> Waveshare ESP32-S3-Touch-AMOLED-1.75 (round, 466x466)
//   -D BOARD_CYD_RECT         -> ESP32-2432S028R / ELEGOO 2.8" (rectangular, 320x240)
// PlatformIO passes it per env (platformio.ini: waveshare-round, cyd-rect).
// After this header BOTH identity flags are defined as 0 or 1, and the board
// header has defined LCD_WIDTH/LCD_HEIGHT, the pins, every BOARD_* capability
// flag and the display settings.

#pragma region Arduino IDE board choice (it cannot easily pass -D: uncomment exactly one)
// #define BOARD_WAVESHARE_ROUND
// #define BOARD_CYD_RECT
#pragma endregion

#pragma region Validate and normalize the board identity flags
// normalize: '-D BOARD_X' gives 1 but an empty '#define BOARD_X' would make
// '#if BOARD_X' invalid, so both become exactly 1 / 0
#if defined(BOARD_WAVESHARE_ROUND) && defined(BOARD_CYD_RECT)
  #error "Two boards selected: define only one of BOARD_WAVESHARE_ROUND / BOARD_CYD_RECT"
#elif defined(BOARD_WAVESHARE_ROUND)
  #undef BOARD_WAVESHARE_ROUND
  #define BOARD_WAVESHARE_ROUND 1
  #define BOARD_CYD_RECT 0
#elif defined(BOARD_CYD_RECT)
  #undef BOARD_CYD_RECT
  #define BOARD_CYD_RECT 1
  #define BOARD_WAVESHARE_ROUND 0
#else
  #error "No board selected: build with -D BOARD_WAVESHARE_ROUND or -D BOARD_CYD_RECT (see platformio.ini)"
#endif
#pragma endregion

#pragma region Board header (pins, capability flags, display settings)
#if BOARD_WAVESHARE_ROUND
  #include "board_waveshare_round.h"
#elif BOARD_CYD_RECT
  #include "board_cyd_rect.h"
#endif
#pragma endregion


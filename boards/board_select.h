#pragma once

// Selects the target board at compile time.
//   -D BOARD_CYD_RECT   -> ESP32-2432S028R / ELEGOO 2.8" (320x240, rectangular)
//   (no flag)           -> Waveshare ESP32-S3-Touch-AMOLED-1.75 (round, 466x466),
//                          the original behavior: does not break existing builds.

// Arduino IDE (cannot easily pass -D): uncomment to build for the CYD.
// #define BOARD_CYD_RECT

#if defined(BOARD_CYD_RECT)
  // normalize: '-D BOARD_CYD_RECT' or an empty '#define' would make '#if BOARD_CYD_RECT' invalid
  #undef BOARD_CYD_RECT
  #define BOARD_CYD_RECT 1
  #define BOARD_WAVESHARE_ROUND 0
  #define BOARD_IS_ROUND 0
  #define BOARD_HAS_PMU 0
  #define BOARD_HAS_RTC_CHIP 0
  #define BOARD_HAS_I2S_AUDIO 0
  #define BOARD_HAS_BATTERY 0
  #include "pin_config_cyd.h"
#else
  #define BOARD_CYD_RECT 0
  #define BOARD_WAVESHARE_ROUND 1
  #define BOARD_IS_ROUND 1
  #define BOARD_HAS_PMU 1
  #define BOARD_HAS_RTC_CHIP 1
  #define BOARD_HAS_I2S_AUDIO 1
  #define BOARD_HAS_BATTERY 1
  #include "../pin_config.h"  // original Waveshare pins, not duplicated
#endif

#pragma once

// Internal to sdmon.cpp / sd_upload.cpp: the SD filesystem object for this
// board's bus. SDHOST is SD_MMC (SDMMC bus) or SD (SPI bus).

#pragma region Includes
#include "boards/board_select.h"
#include <FS.h>
#if BOARD_SD_SDMMC
  #include <SD_MMC.h>
  #define SDHOST SD_MMC
#elif BOARD_SD_SPI
  // no SDMMC wired: the microSD goes over SPI (VSPI by default)
  #include <SD.h>
  #include <SPI.h>
  #define SDHOST SD
#else
  #error "No SD bus: the board header must set BOARD_SD_SDMMC or BOARD_SD_SPI to 1"
#endif
#pragma endregion

#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-1.75: round 466x466 AMOLED, ESP32-S3 with
// 8 MB OPI PSRAM and 16 MB flash. Included only through boards/board_select.h.
// Pins: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 (libraries/Mylibrary/pin_config.h)

#pragma region Capability flags (every board header defines all of them as 0 or 1)
#define BOARD_IS_ROUND 1
#define BOARD_DISPLAY_CO5300_QSPI 1
#define BOARD_DISPLAY_ILI9341_SPI 0
#define BOARD_TOUCH_CST9217 1
#define BOARD_TOUCH_XPT2046 0
#define BOARD_SD_SDMMC 1
#define BOARD_SD_SPI 0
#define BOARD_AUDIO_I2S_ES8311 1
#define BOARD_AUDIO_LEDC 0
#define BOARD_HAS_PMU 1
#define BOARD_HAS_RTC_CHIP 1
#define BOARD_HAS_BATTERY 1
// Not BOARD_HAS_PSRAM: that is the Arduino core's own macro (passed by platformio.ini)
#define BOARD_HAS_PSRAM_CHIP 1
#define BOARD_USES_BAND_CANVAS 0
#define BOARD_HAS_WIFI 1
#define BOARD_USB_CDC_SERIAL 1
#define BOARD_HAS_I2C_BUS 1
#define BOARD_HAS_BOOT_POWER_KEY 0
#pragma endregion

#pragma region Display: 466x466 AMOLED, CO5300 driver over QSPI
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
// Arduino_CO5300 constructor arguments: rotation and the panel's RAM offsets
// (the visible area starts 6 columns into the controller's memory)
#define BOARD_DISPLAY_ROTATION 0
#define BOARD_DISPLAY_COL_OFFSET1 6
#define BOARD_DISPLAY_ROW_OFFSET1 0
#define BOARD_DISPLAY_COL_OFFSET2 0
#define BOARD_DISPLAY_ROW_OFFSET2 0
// QSPI clock: 80 MHz (library default 40). The framebuffer flush is the fps
// bottleneck (~56 ms at 40 MHz). If the panel shows garbage, drop back to 40 MHz.
#define BOARD_DISPLAY_SPEED_HZ 80000000
#pragma endregion

#pragma region Shared I2C bus: CST9217 touch, AXP2101 PMU, PCF85063 RTC and ES8311 codec
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40
#pragma endregion

#pragma region AXP2101 PMU (XPowersLib)
// XPowersLib picks its chip driver from this macro: it must be defined before
// <XPowersLib.h> is included (rtcbat.cpp includes it after this header).
#define XPOWERS_CHIP_AXP2101
#pragma endregion

#pragma region ES8311 audio codec over I2S, then the amplifier
// The real MCLK is GPIO42 (verified on the board with the PlaneRadar2.0
// project); the 16 listed before was wrong. Even so, the codec is configured
// with a clock derived from BCLK, so MCLK barely matters.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46  // speaker amplifier enable, active high (audio.cpp)
#pragma endregion

#pragma region microSD slot: SD_MMC in 1-bit mode (sdmon.cpp)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41  // DAT3 of the slot; unused in 1-bit mode
#pragma endregion

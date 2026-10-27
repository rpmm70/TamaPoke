// Display construction and initialization for the selected board (see board_display.h).

#pragma region Includes
#include "board_display.h"
#include "boards/layout.h"  // LCD_WIDTH/LCD_HEIGHT, CX/CY, BEZEL_RADIUS, display pins and settings
#include "colors.h"         // UI_BG_DAY
#include "sdmon.h"          // sdBegin (banded framebuffer boards mount the SD first)
#if BOARD_USES_BAND_CANVAS
#include "band_canvas.h"
#endif
#pragma endregion

#pragma region Bus, panel and frame buffer
#if BOARD_DISPLAY_CO5300_QSPI
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
static Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, BOARD_DISPLAY_ROTATION, LCD_WIDTH, LCD_HEIGHT,
  BOARD_DISPLAY_COL_OFFSET1, BOARD_DISPLAY_ROW_OFFSET1, BOARD_DISPLAY_COL_OFFSET2, BOARD_DISPLAY_ROW_OFFSET2);
// Full framebuffer in PSRAM: everything is drawn and then flush()ed (no flicker)
Arduino_GFX *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
#elif BOARD_DISPLAY_ILI9341_SPI
// ILI9341 over 4-wire SPI on the HSPI bus, which is exactly what the library
// assumes by default for HSPI.
static Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, BOARD_DISPLAY_SPI_HOST);
static Arduino_ILI9341 *panel = new Arduino_ILI9341(bus, TFT_RST, BOARD_DISPLAY_ROTATION, BOARD_DISPLAY_IPS);
#if BOARD_USES_BAND_CANVAS
// Full framebuffer (320x240x2 = 150 KB) in INTERNAL RAM: without PSRAM, 150 KB
// in one piece never fits (largest free block ~110 KB), so BandCanvas splits it
// into 32-row bands (see band_canvas.h). If even the bands do not fit,
// displayBegin() falls back to drawing directly on the panel (gfx = panel;
// works but flickers).
static BandCanvas *fbCanvas = new BandCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
Arduino_GFX *gfx = fbCanvas;
#else
Arduino_GFX *gfx = panel;
#endif
#else
#error "No display driver: the board header must set BOARD_DISPLAY_CO5300_QSPI or BOARD_DISPLAY_ILI9341_SPI to 1"
#endif
#pragma endregion

#pragma region Public API
void displayBegin(uint8_t initialBrightness) {
#if BOARD_DISPLAY_CO5300_QSPI
  #pragma region CO5300: canvas and panel together
  if (!gfx->begin(BOARD_DISPLAY_SPEED_HZ)) Serial.println("gfx->begin() failed");
  boardSetBrightness(initialBrightness);
  #pragma endregion
#else
  #pragma region ILI9341: backlight off until there is an image, then the panel
  ledcAttach(TFT_BL, BOARD_BACKLIGHT_PWM_FREQUENCY_HZ, BOARD_BACKLIGHT_PWM_RESOLUTION_BITS);
  boardSetBrightness(0);  // no white flash until there is an image
  if (!panel->begin(BOARD_DISPLAY_SPEED_HZ)) Serial.println("panel->begin() failed");
  #pragma endregion
#if BOARD_USES_BAND_CANVAS
  #pragma region SD first, then the framebuffer bands
  Serial.printf("CYD heap: free=%u largest_block=%u (framebuffer needs %u)\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)(LCD_WIDTH * LCD_HEIGHT * 2));
  // SD BEFORE the framebuffer: mounting needs one ~14 KB contiguous block (4 KB
  // FAT sectors, one cache per open file), and once the 20 KB bands fill the
  // heap's gaps no block that size is left (esp_vfs_fat_register 0x101)
  sdBegin();
  // the panel is already initialized: GFX_SKIP_OUTPUT_BEGIN only allocates the
  // bands, and if they do not fit it draws straight to the panel
  bool useCanvas = fbCanvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  gfx = useCanvas ? (Arduino_GFX *)fbCanvas : (Arduino_GFX *)panel;
  if (useCanvas) Serial.printf("CYD: banded framebuffer OK, free heap=%u\n", (unsigned)ESP.getFreeHeap());
  else Serial.println("CYD: framebuffer does NOT fit -> drawing directly (flicker)");
  #pragma endregion
#endif
  boardSetBrightness(initialBrightness);
#endif
}

void boardSetBrightness(uint8_t brightness) {
#if BOARD_DISPLAY_CO5300_QSPI
  panel->setBrightness(brightness);
#else
  ledcWrite(TFT_BL, brightness);  // per-pin ledc API (ESP32 Arduino core 3.x)
#endif
}

void fillScreenBg(uint16_t color) {
#if BOARD_IS_ROUND
  gfx->fillCircle(CX, CY, BEZEL_RADIUS, color);
#else
  gfx->fillScreen(color);
#endif
}

void clearScreenToDayBackground() {
#if BOARD_IS_ROUND
  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
#else
  gfx->fillScreen(UI_BG_DAY);  // the day fill covers the whole black fill
#endif
}
#pragma endregion

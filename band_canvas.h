#pragma once

// Full-screen framebuffer split into horizontal bands, for boards WITHOUT
// PSRAM (CYD). The ESP32 has ~300 KB of free internal RAM, but it is split
// into regions: the largest contiguous block is ~110 KB, so a single 150 KB
// Arduino_Canvas never fits. Several 20 KB bands do (40 KB bands stopped
// fitting when an earlier CYD build linked the WiFi library, whose static RAM
// shrank the region after .bss to ~80 KB; small bands still fill whatever
// gaps the heap has, so they are kept now that WiFi is compiled out of the
// CYD build). The scene is drawn ONCE
// (same drawing code as with Arduino_Canvas) and flush() sends the bands one
// after the other, so the panel never shows a half-drawn frame.
// Rotation 0 only: the landscape rotation is done by the panel (rotation=1).

#pragma region Includes
#include "Arduino_GFX_Library.h"
#include <esp_heap_caps.h>
#pragma endregion

#pragma region Band geometry
#define BAND_SHIFT 5                  // 32 rows per band (20 KB at 320 px): y >> 5 picks the band
#define BAND_ROWS (1 << BAND_SHIFT)
#define BAND_MASK (BAND_ROWS - 1)
#define BAND_MAX 8                    // up to 256 rows
#pragma endregion

class BandCanvas : public Arduino_GFX {
public:
  BandCanvas(int16_t w, int16_t h, Arduino_G *output) : Arduino_GFX(w, h), _output(output) {}
  ~BandCanvas() { freeBands(); }

  #pragma region Setup
  // GFX_SKIP_OUTPUT_BEGIN: only allocates the bands (the panel is already initialized)
  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    if (speed != GFX_SKIP_OUTPUT_BEGIN && _output && !_output->begin(speed)) return false;
    if (_bands[0]) return true;
    if (bandCount() > BAND_MAX) return false;
    for (int i = 0; i < bandCount(); i++) {
      _bands[i] = (uint16_t *)heap_caps_malloc((size_t)WIDTH * bandRows(i) * 2,
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!_bands[i]) {
        freeBands();
        return false;
      }
    }
    return true;
  }
  #pragma endregion

  #pragma region Drawing primitives
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
    if (!_bands[0]) return;
    pixelRow(y)[x] = color;
  }

  // not preclipped: drawFastHLine() in the base calls it with raw coordinates
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (!_bands[0] || y < 0 || y >= HEIGHT) return;
    if (w < 0) { x += w + 1; w = -w; }
    if (x < 0) { w += x; x = 0; }
    if (x + w > WIDTH) w = WIDTH - x;
    if (w <= 0) return;
    fillSpan(pixelRow(y) + x, w, color);
  }

  // not preclipped either
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    if (!_bands[0] || x < 0 || x >= WIDTH) return;
    if (h < 0) { y += h + 1; h = -h; }
    if (y < 0) { h += y; y = 0; }
    if (y + h > HEIGHT) h = HEIGHT - y;
    for (int16_t row = y; row < y + h; row++) pixelRow(row)[x] = color;
  }

  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if (!_bands[0]) return;
    for (int16_t row = y; row < y + h; row++) fillSpan(pixelRow(row) + x, w, color);
  }
  #pragma endregion

  #pragma region Flush
  void flush(bool force_flush = false) override {
    (void)force_flush;
    if (!_bands[0] || !_output) return;
    for (int i = 0; i < bandCount(); i++)
      _output->draw16bitRGBBitmap(0, i * BAND_ROWS, _bands[i], WIDTH, bandRows(i));
  }
  #pragma endregion

private:
  #pragma region Internal helpers
  int bandCount() const { return (HEIGHT + BAND_ROWS - 1) / BAND_ROWS; }
  int bandRows(int band) const {
    int left = HEIGHT - band * BAND_ROWS;
    return left < BAND_ROWS ? left : BAND_ROWS;
  }
  uint16_t *pixelRow(int16_t y) { return _bands[y >> BAND_SHIFT] + (int32_t)(y & BAND_MASK) * WIDTH; }
  static void fillSpan(uint16_t *dst, int16_t w, uint16_t color) {
    while (w--) *dst++ = color;
  }
  void freeBands() {
    for (auto &band : _bands) {
      if (band) heap_caps_free(band);
      band = nullptr;
    }
  }
  #pragma endregion

  Arduino_G *_output = nullptr;
  uint16_t *_bands[BAND_MAX] = {};
};

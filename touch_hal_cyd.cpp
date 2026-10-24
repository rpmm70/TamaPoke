#include "touch_hal.h"
#include "boards/board_select.h"

#if BOARD_CYD_RECT

// Resistive XPT2046 read by bit-banging (TOUCH_* pins), NOT by hardware SPI.
// Reason: the CYD spreads three devices across two SPI buses (display on
// HSPI, SD on VSPI, touch on its own pins). An SPIClass for the touch would
// clash with the SD; reading two 12-bit words by hand costs ~100 us and
// cannot collide with anything. No extra library is needed.

// Calibrated on the real unit (landscape, rotation=1) with -DTOUCH_DEBUG, from
// the four bottom buttons (x=40/120/200/280, y=218) plus a tap at the center:
// the raw X channel follows the screen's Y and vice versa (hence SWAP), neither
// axis is mirrored. ~11.2 raw units per px horizontally, ~15.1 vertically; the
// ranges below are those lines extrapolated to the screen edges.
// If touches come out rotated/mirrored, change TOUCH_SWAP/INVERT; if they are
// off at the edges, adjust RAW_*. To see the raw values, build with
// -DTOUCH_DEBUG and touch the four corners while watching serial.
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 1
#endif
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 0
#endif
// per-axis ranges, in screen axes (applied after the swap)
static const int RAW_X_MIN = 166;
static const int RAW_X_MAX = 3726;
static const int RAW_Y_MIN = 260;
static const int RAW_Y_MAX = 3875;

static inline void clk() {
  digitalWrite(TOUCH_CLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(TOUCH_CLK, LOW);
  delayMicroseconds(2);
}

// Sends a control byte and returns the 12-bit conversion
static uint16_t xptRead(uint8_t cmd) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (cmd >> i) & 1);
    clk();
  }
  digitalWrite(TOUCH_MOSI, LOW);
  // No separate BUSY clock: the MSB comes out on the falling edge of the 9th
  // clock, which is the first clk() of this loop. An extra clock here lost the
  // MSB (every reading came out doubled, wrapped and always even).
  uint16_t v = 0;
  for (int i = 0; i < 12; i++) {
    clk();
    v = (v << 1) | (digitalRead(TOUCH_MISO) ? 1 : 0);
  }
  for (int i = 0; i < 4; i++) clk();  // pad out to the 24-clock conversion
  return v;
}

void touchHalBegin() {
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TOUCH_CLK, OUTPUT);
  digitalWrite(TOUCH_CLK, LOW);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);  // GPIO36 is input-only, no internal pull-up
}

bool touchHalHasEvent() {
  // The XPT2046 IRQ is low while pressed: cheap filter before spending a
  // full read.
  return digitalRead(TOUCH_IRQ) == LOW;
}

static int median3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { b = a; }
  return b;
}

bool touchHalGetPoint(int16_t *x, int16_t *y) {
  if (digitalRead(TOUCH_IRQ) != LOW) return false;

  digitalWrite(TOUCH_CS, LOW);
  int rx[3], ry[3];
  for (int i = 0; i < 3; i++) {
    rx[i] = xptRead(0xD0);  // X channel
    ry[i] = xptRead(0x90);  // Y channel
  }
  xptRead(0x90 & ~0x03);  // PD=00: leaves the chip idle with IRQ enabled
  digitalWrite(TOUCH_CS, HIGH);

  int rawX = median3(rx[0], rx[1], rx[2]);
  int rawY = median3(ry[0], ry[1], ry[2]);
#ifdef TOUCH_DEBUG
  Serial.printf("touch raw x=%d y=%d\n", rawX, rawY);
#endif
  // with no pressure the bus floats to 0 or 4095: discard
  if (rawX <= 50 || rawX >= 4090 || rawY <= 50 || rawY >= 4090) return false;

  if (TOUCH_SWAP_XY) { int t = rawX; rawX = rawY; rawY = t; }
  long mx = map(rawX, RAW_X_MIN, RAW_X_MAX, 0, LCD_WIDTH - 1);
  long my = map(rawY, RAW_Y_MIN, RAW_Y_MAX, 0, LCD_HEIGHT - 1);
  if (TOUCH_INVERT_X) mx = LCD_WIDTH - 1 - mx;
  if (TOUCH_INVERT_Y) my = LCD_HEIGHT - 1 - my;
  *x = (int16_t)constrain(mx, 0, LCD_WIDTH - 1);
  *y = (int16_t)constrain(my, 0, LCD_HEIGHT - 1);
  return true;
}

#endif  // BOARD_CYD_RECT

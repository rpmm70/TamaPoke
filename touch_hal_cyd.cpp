#pragma region Includes
#include "touch_hal.h"
#include "boards/board_select.h"
#pragma endregion

#if BOARD_TOUCH_XPT2046

// Resistive XPT2046 read by bit-banging (TOUCH_* pins), NOT by hardware SPI.
// Reason: the CYD spreads three devices across two SPI buses (display on
// HSPI, SD on VSPI, touch on its own pins). An SPIClass for the touch would
// clash with the SD; reading two 12-bit words by hand costs ~100 us and
// cannot collide with anything. No extra library is needed.
// Calibration (TOUCH_SWAP_XY / TOUCH_INVERT_* / TOUCH_RAW_*) lives in the board
// header (boards/board_cyd_rect.h).

#pragma region XPT2046 protocol constants
// Control byte: S=1, A2..A0 = channel, MODE=0 (12 bit), SER/DFR=0 (differential),
// PD1..PD0 = 00 (power down between conversions, PENIRQ enabled)
#define XPT2046_COMMAND_READ_X 0xD0
#define XPT2046_COMMAND_READ_Y 0x90
// Half period of the bit-banged clock. The XPT2046 allows up to 2.5 MHz (400 ns
// per clock), so 1 us high + 1 us low (500 kHz) leaves plenty of margin.
#define XPT2046_HALF_CLOCK_US 1
#define XPT2046_CONVERSION_BITS 12
#define XPT2046_PAD_CLOCKS 4      // pads the 8 + 12 clocks out to the 24-clock conversion frame
#define TOUCH_SAMPLES_PER_AXIS 3  // median of three readings per axis
// with no pressure the bus floats to 0 or 4095: readings outside this window are discarded
#define TOUCH_RAW_FLOATING_LOW 50
#define TOUCH_RAW_FLOATING_HIGH 4090
#pragma endregion

#pragma region Bit-banged XPT2046 access
static inline void pulseTouchClock() {
  digitalWrite(TOUCH_CLK, HIGH);
  delayMicroseconds(XPT2046_HALF_CLOCK_US);
  digitalWrite(TOUCH_CLK, LOW);
  delayMicroseconds(XPT2046_HALF_CLOCK_US);
}

// Sends a control byte and returns the 12-bit conversion
static uint16_t readXpt2046Channel(uint8_t controlByte) {
  #pragma region Shift out the control byte, MSB first
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (controlByte >> i) & 1);
    pulseTouchClock();
  }
  digitalWrite(TOUCH_MOSI, LOW);
  #pragma endregion

  #pragma region Shift in the 12-bit result
  // No separate BUSY clock: the MSB comes out on the falling edge of the 9th
  // clock, which is the first pulseTouchClock() of this loop. An extra clock
  // here lost the MSB (every reading came out doubled, wrapped and always even).
  uint16_t conversionValue = 0;
  for (int i = 0; i < XPT2046_CONVERSION_BITS; i++) {
    pulseTouchClock();
    conversionValue = (conversionValue << 1) | (digitalRead(TOUCH_MISO) ? 1 : 0);
  }
  for (int i = 0; i < XPT2046_PAD_CLOCKS; i++) pulseTouchClock();
  return conversionValue;
  #pragma endregion
}

static int medianOfThree(int first, int second, int third) {
  int swapTemporary;
  if (first > second) { swapTemporary = first; first = second; second = swapTemporary; }
  if (second > third) { swapTemporary = second; second = third; third = swapTemporary; }
  if (first > second) { second = first; }
  return second;
}
#pragma endregion

#pragma region Public API
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

bool touchHalGetPoint(int16_t *x, int16_t *y) {
  if (digitalRead(TOUCH_IRQ) != LOW) return false;

  #pragma region Sample both channels
  digitalWrite(TOUCH_CS, LOW);
  int rawXSamples[TOUCH_SAMPLES_PER_AXIS], rawYSamples[TOUCH_SAMPLES_PER_AXIS];
  for (int i = 0; i < TOUCH_SAMPLES_PER_AXIS; i++) {
    rawXSamples[i] = readXpt2046Channel(XPT2046_COMMAND_READ_X);
    rawYSamples[i] = readXpt2046Channel(XPT2046_COMMAND_READ_Y);
  }
  // PD=00 (already in the command byte): the last conversion leaves the chip idle with IRQ enabled
  readXpt2046Channel(XPT2046_COMMAND_READ_Y);
  digitalWrite(TOUCH_CS, HIGH);
  #pragma endregion

  int rawX = medianOfThree(rawXSamples[0], rawXSamples[1], rawXSamples[2]);
  int rawY = medianOfThree(rawYSamples[0], rawYSamples[1], rawYSamples[2]);
#ifdef TOUCH_DEBUG
  Serial.printf("touch raw x=%d y=%d\n", rawX, rawY);
#endif
  if (rawX <= TOUCH_RAW_FLOATING_LOW || rawX >= TOUCH_RAW_FLOATING_HIGH ||
      rawY <= TOUCH_RAW_FLOATING_LOW || rawY >= TOUCH_RAW_FLOATING_HIGH) return false;

  #pragma region Map raw readings to screen pixels
  if (TOUCH_SWAP_XY) { int swapTemporary = rawX; rawX = rawY; rawY = swapTemporary; }
  long screenX = map(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, LCD_WIDTH - 1);
  long screenY = map(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, LCD_HEIGHT - 1);
  if (TOUCH_INVERT_X) screenX = LCD_WIDTH - 1 - screenX;
  if (TOUCH_INVERT_Y) screenY = LCD_HEIGHT - 1 - screenY;
  *x = (int16_t)constrain(screenX, 0, LCD_WIDTH - 1);
  *y = (int16_t)constrain(screenY, 0, LCD_HEIGHT - 1);
  return true;
  #pragma endregion
}
#pragma endregion

#endif  // BOARD_TOUCH_XPT2046

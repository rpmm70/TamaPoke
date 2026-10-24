#include "touch_hal.h"
#include "boards/board_select.h"

#if BOARD_WAVESHARE_ROUND

#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"

#define TOUCH_ADDR 0x5A  // CST9217

static TouchDrvCST92xx touch;
static volatile bool gTouchIrq = false;
static void IRAM_ATTR touchIsr() { gTouchIrq = true; }

void touchHalBegin() {
  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // sometimes fails on the first attempt
    touchOk = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 no detectado");
  // begin() leaves the chip in command mode (reads the ID and never exits);
  // a hardware reset is needed for it to report touches again
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // the panel is mounted rotated 180 degrees
  // INT active-low: fires when data is available. Gates the I2C reads
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);
}

bool touchHalHasEvent() {
  if (!gTouchIrq) return false;
  gTouchIrq = false;
  return true;
}

bool touchHalGetPoint(int16_t *x, int16_t *y) {
  // An address-only cycle before reading. Without it, getPoint() would hang
  // exactly 1000 ms (the I2C driver's default timeout, which SensorLib does
  // not bound) and freeze the whole loop: that is what showed up as "the
  // minigame freezes 2-3 seconds when tapping the ball" (issue #16), because
  // touches there are fast and back to back.
  //
  // Measured on the board, playing the same way (~470 reads, score ~50):
  //   without this line: 5 stalls of 1000 ms in 60 s
  //   with it:           0
  // The rejection counter came out 0 in both cases, so it does NOT work by
  // skipping reads when the chip does not answer: what it does is wake it up,
  // so the next read does not find the CST9217 asleep.
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) return false;  // no answer: retried in 20 ms
  return touch.getPoint(x, y, 1) > 0;
}

#endif  // BOARD_WAVESHARE_ROUND

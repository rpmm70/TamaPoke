#pragma once

#pragma region Includes
#include <Arduino.h>
#pragma endregion

// Abstracts the touch panel: capacitive CST9217 over I2C (BOARD_TOUCH_CST9217,
// interrupt driven, touch_hal_waveshare.cpp) versus resistive XPT2046
// bit-banged on its own pins (BOARD_TOUCH_XPT2046, polled, touch_hal_cyd.cpp).
// handleTouch() only uses these three functions.

#pragma region Public API
void touchHalBegin();

// true if there is something worth reading now. CST9217: gates the I2C bus
// with the INT pin (see the comment about the CST9217 hang). XPT2046: cheap
// filter using the IRQ pin (low while touched).
bool touchHalHasEvent();

// Reads the current point in screen coordinates. true if a finger is down.
bool touchHalGetPoint(int16_t *x, int16_t *y);
#pragma endregion

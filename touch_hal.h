#pragma once
#include <Arduino.h>

// Abstracts the touch panel: capacitive CST9217 over I2C (Waveshare, interrupt
// driven) versus resistive XPT2046 over SPI (CYD, polled). handleTouch() only
// uses these three functions.

void touchHalBegin();

// true if there is something worth reading now. Waveshare: gates the I2C bus
// with the INT pin (see the comment about the CST9217 hang). CYD: cheap filter
// using the XPT2046 IRQ pin (low while touched).
bool touchHalHasEvent();

// Reads the current point in screen coordinates. true if a finger is down.
bool touchHalGetPoint(int16_t *x, int16_t *y);

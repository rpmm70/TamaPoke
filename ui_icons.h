#pragma once

// Small vector icons drawn with GFX primitives (no bitmaps), shared by every
// screen. Stroke thickness scales with the icon size (minimum 2 px), so the same
// call looks right on the CYD (320x240) and on the Waveshare (466x466).
// No dynamic allocation: everything is drawn straight onto `canvas`.

#pragma region Includes
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#pragma endregion

#pragma region Icon drawing
// Round "X" close button: filled circle background + two thick diagonal strokes.
void drawCloseButton(Arduino_GFX *canvas, int16_t centerX, int16_t centerY, int16_t radius, uint16_t backgroundColor, uint16_t crossColor);
// Small pencil (edit) icon inside the square at (left, top) of side `size`,
// drawn diagonally from the tip (bottom-left) to the eraser (top-right).
void drawPencilIcon(Arduino_GFX *canvas, int16_t left, int16_t top, int16_t size, uint16_t color);
// Filled triangular arrow centered on (centerX, centerY).
// direction -1 = points left, +1 = points right; `size` = width and height of the triangle.
void drawPageArrow(Arduino_GFX *canvas, int16_t centerX, int16_t centerY, int16_t size, int8_t direction, uint16_t color);
// Line of any thickness (>= 1) with rounded ends, built from two fillTriangle + caps.
void drawThickLine(Arduino_GFX *canvas, int16_t startX, int16_t startY, int16_t endX, int16_t endY, int16_t thickness, uint16_t color);
#pragma endregion

#pragma region Touch hit tests
// Inclusive tests. Callers pass a generous radius / padding for finger touches.
bool isTouchInsideCircle(int16_t touchX, int16_t touchY, int16_t centerX, int16_t centerY, int16_t radius);
// Covers left..left+width-1 and top..top+height-1.
bool isTouchInsideRect(int16_t touchX, int16_t touchY, int16_t left, int16_t top, int16_t width, int16_t height);
#pragma endregion

#pragma once

// Touch input: polls the touch panel, tells a tap from a swipe or a long press
// when the finger lifts, and routes the gesture to the screen that is open.
// Every touch event marks the screen dirty (static screens redraw on it).

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// polls the touch panel at 50 Hz and resolves gestures (call every loop)
void handleTouch();
#pragma endregion

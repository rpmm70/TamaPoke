#pragma once

// Ball game ("keepy-uppy"): keep the pokeball in the air by tapping it. Trains ATK.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
void startGame();
void gameTap(int16_t x, int16_t y);
// steps the physics by real time and draws one frame (or the results screen)
void renderGame();
#pragma endregion

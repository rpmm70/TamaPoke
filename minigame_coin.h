#pragma once

// Coin chase: tap a spot and the creature walks there; each coin it touches is a
// point and extra time. Trains SPE.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
void startCoinGame();
void coinGameTap(int16_t x, int16_t y);
// steps the movement by real time and draws one frame (or the results screen)
void renderCoinGame();
#pragma endregion

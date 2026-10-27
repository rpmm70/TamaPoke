#pragma once

// Bath scene: foam bubbles over the creature, and the cleanup when they pop.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// bath button: starts the foam (the cleaning applies when it pops)
void startBath();
// draws the foam while the bath lasts and applies the cleaning at its end
void drawBath();
#pragma endregion

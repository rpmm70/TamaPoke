#pragma once

// Bath scene: a shower with water droplets falling over the creature, and the
// cleanup when it ends.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// bath button: turns the shower on (the cleaning applies when it ends)
void startBath();
// once per frame before the background: true while the shower runs (render()
// then draws the shower room instead of the landscape); when it ends, applies
// the cleaning and returns false, so the same frame is back on the landscape
bool updateBathScene();
// draws the falling droplets and their splashes while the shower runs
void drawBath();
#pragma endregion

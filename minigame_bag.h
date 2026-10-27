#pragma once

// Training bag: mash the bag for a fixed time. Trains ATK.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
void startSack();
// one hit on the bag (each touch counts instantly, see handleTouch)
void sackTap();
// draws one frame of the bag game (or the results screen once the time is up)
void renderSack();
#pragma endregion

#pragma once

// Creature info card (tap the name on the main screen): profile with the
// animated portrait, stats, medals and progress pages.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// draws page cardPage (page 0 animates every frame; pages 1-3 are static screens)
void renderCard();
// close button, rename pencil (page 0) and page arrows; anywhere else does nothing
void handleInfoCardTap(int16_t x, int16_t y);
#pragma endregion

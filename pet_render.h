#pragma once

// The creature on the main screen: sprite loading for the current species, the
// PMD behavior (idle, walks, gestures), the B/W and flash fallbacks, the heart
// and the sleep bubble.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// loads/unloads the SD sprite when the species changes or new files arrived (call every loop)
void ensureMon();
// draws the creature (or the evolution FX / the missing-sprite notice); records lastSpriteDrawnBounds
void drawPet();
// heart over the creature's head; call right after drawPet()
void drawPetHeart();
// "Zzz" bubble next to the sleeping creature; call right after drawPet()
void drawSleepBubble();
#pragma endregion

#pragma once

// render(): picks the screen to draw this frame, and draws the main screen
// itself (landscape, header, creature, needs, buttons and the overlays over it:
// food / play pickers, wild prompt, release and choice dialogs) and the starter choice.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// draws one frame of whatever screen is open
void render();
// top edge of starter row starterIndex (0..STARTER_COUNT-1) on the starter choice
// screen; drawing and touch share it
int starterRowTop(int starterIndex);
#pragma endregion

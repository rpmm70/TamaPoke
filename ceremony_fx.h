#pragma once

// Big moments on the main screen: farewell / runaway ceremony animation, the
// evolution FX, the decision dialog and the pulsing call-to-action buttons.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// ceremony animation (10 s): farewell = a bow with hearts and it walks away;
// runaway = it gets scared and runs off. Replaces the idle.
void drawCeremony();
// decision dialog (2 stacked buttons): evolve/keep or say goodbye/stay together
void drawChoiceDialog();
// big red CTA button to evolve (pulses to draw attention)
void drawEvolveButton();
// gold farewell CTA button: "<name> wants to tell you something..."
void drawFarewellButton();
// somber runaway-from-neglect CTA button: "<name> feels abandoned..."
void drawRunawayButton();
// epic evolution animation: radial halo + spinning rays + accelerating sprite
// flashing + sparks shooting out + final flash
void drawEvolveFX(uint32_t now);
#pragma endregion

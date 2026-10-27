#pragma once

// Wild battles on screen: the random encounter prompt and the manual turn-based
// battle (the turn engine itself is battle.h / battle.cpp).

#pragma region Includes
#include <Arduino.h>
#include "battle.h"  // BattleStats
#pragma endregion

#pragma region Public API
// the pet's battle stats (level clamped to the battle range, types from the dex)
BattleStats petBattleStats();
// opens the "wild battle?" prompt for wildDex (random level and shiny roll)
void openWildPrompt(int16_t wildDex, uint32_t now);
// random wild prompt, only on the idle main screen (call every loop)
void maybeOfferWildEncounter(uint32_t now);
// starts a battle; forcedDex/forcedLevel 0 = random, forcedShiny -1 = roll
// (0 / 1 force it); fromWildEncounter = accepted from the random prompt
void startBattleWith(int16_t forcedDex, uint8_t forcedLevel, int8_t forcedShiny = -1, bool fromWildEncounter = false);
// battle from the play menu swords (random wild species and level)
void startBattle();
void battleTap(int16_t x, int16_t y);
// the prompt box over the main screen (drawn by render())
void drawWildPrompt();
void renderBattle();
#pragma endregion

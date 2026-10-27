#pragma once

// Shared by the three minigames (ball game, coin chase, training bag): who may
// start one, the creature drawn in the game, and the results screen.

#pragma region Includes
#include <Arduino.h>
#include "i18n.h"  // StrId
#pragma endregion

#pragma region Public API
// a minigame can start: no egg, not asleep, no ceremony in progress
bool canStartMinigame();
// the creature in a minigame, anchored by the feet: PMD action (idle if missing)
// or the B/W SD sprite fallback; sizePercent resizes either one (100 = usual size).
// Records lastSpriteDrawnBounds.
void drawMinigamePet(uint8_t act, int centerX, int feetY, uint8_t maxScale, uint16_t sizePercent);
// OK button of the results screen (same box as the battle result OK)
bool isMinigameOkTapped(int16_t x, int16_t y);
// Results screen: the creature on the habitat, the score in scoreFormat (green
// for a new record or a cleared coin chase), the record or NEW RECORD!, the joy
// line, the stat reward in rewardFormat when statGain > 0, and the OK button
void drawMinigameResults(StrId scoreFormat, uint16_t score, bool newRecord, bool cleared, uint16_t record, uint8_t statGain,
                         StrId rewardFormat);
#pragma endregion

#pragma once

// Background scenes: sky by the real time of day, the creature's biome and its
// details, clouds, sun/moon and night stars. drawScene() is the main screen's
// landscape, drawGameScene() the lower-horizon habitat of the minigames, and
// drawBattleArena() the top-down court behind battles, drawCoinChaseField() and
// drawBallGameStage() the coin chase and ball game backgrounds, drawBoxingRingInterior()
// the training bag background.

#pragma region Includes
#include <Arduino.h>
#include "boards/layout.h"  // LCD_HEIGHT
#pragma endregion

#pragma region Public API
// hour of the day 0-23 (from the real time cached every 30 s; 13 if there is no clock)
int sceneHour();
// Full landscape for biome at now. The ground is filled from the horizon down to
// groundBottom: screens that cover the bottom with a panel (main, egg) pass
// BOTTOM_PANEL_Y, the ceremony keeps the full ground (LCD_HEIGHT).
void drawScene(uint8_t biome, uint32_t now, bool night, int groundBottom = LCD_HEIGHT);
// Minigame background: sky by time of day (stars at night) over the
// creature's biome soil, with a lower horizon that leaves room for the action
void drawGameScene();
// Battle background: top-down sand court with blue-gray lines (same at any time of day)
void drawBattleArena();
// Coin chase background: top-down grass field fenced by red posts (same at any time of day)
void drawCoinChaseField();
// Ball game background: theater stage with red curtains; the creature walks on its floor
// (between STAGE_FLOOR_TOP_Y and STAGE_FRONT_Y, same at any time of day)
void drawBallGameStage();
// Training bag background: inside a boxing ring, looking at the far ropes
// and corner posts in a dark arena; the canvas starts at RING_CANVAS_TOP_Y
// (same at any time of day)
void drawBoxingRingInterior();
#pragma endregion

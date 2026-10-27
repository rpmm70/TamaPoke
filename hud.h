#pragma once

// Main screen heads-up display: header (name, level row, battery, clock,
// hamburger and Pokedex buttons, streak badge), the needs bars, the action
// buttons, poops, the medal celebration and the header messages.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Header
// Egg and ceremony views: battery, name plus a message line under it
void drawHeader(const char *name, uint16_t nameColor, const char *msg);
// Main pet view: battery, name plus "Lv.N" and a progress bar toward the next level
void drawHeaderWithLevel(const char *name, uint16_t nameColor);
// Menu button at the top left (main pet view and egg view)
void drawHamburgerButton();
// Pokedex button under the hamburger (main pet view and egg view): opens the gallery
void drawPokedexButton();
// Current local time at the top right, in the format chosen on the time screen
void drawHeaderClock();
// streak badge under the clock
void drawStreakBadge();
#pragma endregion

#pragma region Bottom panel and overlays
void drawPoops();
// the four needs bars (food, joy, energy, hygiene)
void drawBars();
// the four action buttons (only LIGHT works while asleep)
void drawButtons();
// medal / streak milestone banner
void drawCelebration();
#pragma endregion

#pragma region Header messages
// egg view message by the number of cracks
const char *eggMsg();
// the creature's current status line (info card profile page)
const char *statusMsg();
#pragma endregion

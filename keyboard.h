#pragma once

// On-screen keyboard: renames the creature, or asks for a WiFi password
// (BOARD_HAS_WIFI boards only). 7 x 5 keys, letters and two symbol pages.

#pragma region Includes
#include <Arduino.h>
#include "boards/board_select.h"  // BOARD_HAS_WIFI
#pragma endregion

#pragma region Public API
// opens it on the creature's nickname (rename pencil on the info card)
void openKeyboard();
#if BOARD_HAS_WIFI
// opens it empty for the password of ssid (WiFi settings screen)
void openKeyboardForWifiPassword(const char *ssid);
#endif
void renderKeyboard();
void keyboardTap(int16_t x, int16_t y);
#pragma endregion

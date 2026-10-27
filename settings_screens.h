#pragma once

// Settings menu (hamburger button) and its sub-screens: audio / video, time,
// language and WiFi (boards with BOARD_HAS_WIFI only). The X button is the only
// way out of each one.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
void openSettingsMenu();
// first boot: opens the time screen before the starter choice; OK closes it
void openClockForFirstRunSetup();
// NVS settings read once in setup()
void loadBrightnessLevelFromNvs();
void loadClockFormatFromNvs();
// Writes the 12h/24h choice only if it changed since the last write. Called when the
// time screen closes and from persistStateWhileIdle(), so a toggle survives a power cut
// without a ~1 s NVS freeze on every tap.
void saveClockFormatToNvs();
// draws the open settings screen (settingsScreen != SETTINGS_CLOSED)
void renderSettingsScreen();
void handleSettingsTap(int16_t touchX, int16_t touchY);
#pragma endregion

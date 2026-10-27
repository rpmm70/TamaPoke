#pragma once

// Screen power: the user's brightness level, AMOLED protection dimming on
// inactivity and while sleeping, screen off/on and the saves that may only run
// while nothing is animating.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// panel brightness for the user's slider level (settingsBrightnessLevel)
uint8_t brightnessForSettingsLevel();
// true when render() draws the plain main screen: no other screen and no dialog/menu over it
bool isMainScreenShowing();
// Screen Off (settings menu, or the PWR / BOOT button): the panel goes dark on the
// next updateBrightness() and the creature goes to sleep
void turnScreenOff();
// brightness based on the user's level, sleep + inactivity (call every loop)
void updateBrightness(uint32_t now);
// short PWR press (BOOT button on boards without a PMU): screen on/off, polled every 250 ms
void handlePowerKey(uint32_t now);
// Flushes the periodic pet save and the soft clock (rtcPersistIfDue) ONLY with
// the screen dimmed/off or while sleeping: writing to NVS freezes both cores for
// ~1 s, and then there is no animation to interrupt nor a finger waiting.
void persistStateWhileIdle();
#pragma endregion

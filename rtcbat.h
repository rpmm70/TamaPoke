#pragma once

#pragma region Includes
#include <Arduino.h>
#pragma endregion

// Real-time clock, battery and power key. Two implementations, picked by the
// board capability flags: rtcbat.cpp (PCF85063 RTC + AXP2101 PMU, Waveshare)
// and rtcbat_cyd.cpp (NVS-backed soft clock, no battery, BOOT button, CYD).

#pragma region Clock
// PCF85063 RTC: time persists as long as the board has power.
// Soft clock (no RTC chip): NVS epoch advanced with millis().
bool rtcBegin();
uint32_t rtcEpoch();             // unix seconds; 0 if the clock is not valid
void rtcSetEpoch(uint32_t e);
// Soft clock only: writes the current epoch to NVS if the last write is older
// than the persist interval, so a reboot does not go back much. Call it from
// the loop's idle path; it is a no-op on boards with an RTC chip.
void rtcPersistIfDue();
#pragma endregion

#pragma region Battery and panel power (AXP2101 PMU)
bool batBegin();
void pmuEnablePanel();           // turns on BLDO1 (OLED VDD 3.3V); call before gfx->begin()
int batPercent();                // 0-100, -1 if no battery is connected
bool batCharging();
int batMillivolts();             // battery voltage in mV, 0 if no reading
bool usbPresent();
#pragma endregion

#pragma region Power key
// AXP2101 PWR button: 4s long press = physical power off (RTC stays alive);
// the short press is captured by the firmware (screen on/off).
// Boards without a PMU use the BOOT button as the short-press key.
void pwrSetup();
bool pwrShortPressed();  // poll in the loop
#pragma endregion

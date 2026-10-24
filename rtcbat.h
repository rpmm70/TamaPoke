#pragma once
#include <Arduino.h>

// PCF85063 RTC: time persists as long as the board has power
bool rtcBegin();
uint32_t rtcEpoch();             // unix seconds; 0 if the RTC is not valid
void rtcSetEpoch(uint32_t e);

// AXP2101 PMU: battery status
bool batBegin();
void pmuEnablePanel();           // turns on BLDO1 (OLED VDD 3.3V); call before gfx->begin()
int batPercent();                // 0-100, -1 if no battery is connected
bool batCharging();
int batMillivolts();               // battery voltage in mV, 0 if no reading
bool usbPresent();

// AXP2101 PWR button: 4s long press = physical power off (RTC stays alive);
// the short press is captured by the firmware (screen on/off)
void pwrSetup();
bool pwrShortPressed();  // poll in the loop

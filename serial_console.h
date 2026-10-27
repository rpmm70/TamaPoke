#pragma once

// Serial console: SD provisioning (PUT/LS/SDINFO, see sd_upload.cpp) and the
// debugging commands (species, stats, battles, ceremonies...).

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
// reads and runs one command line if one is waiting (call every loop)
void handleSerial();
// HEALTH line every 5 min for the soak test (dropped if no monitor is attached)
void printHealthHeartbeatIfDue(uint32_t now);
#pragma endregion

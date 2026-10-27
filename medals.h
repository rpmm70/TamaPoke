#pragma once

// Medals an individual can earn (bitmask in Pet::medals). Kept apart from pet.h
// so the translation tables (i18n.cpp) can size their medal rows without
// pulling in the whole Pet class.

#pragma region Includes
#include <stdint.h>
#pragma endregion

#pragma region Medal bits
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8
#pragma endregion

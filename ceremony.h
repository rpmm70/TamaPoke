#pragma once

// End-of-cycle ceremonies (Pet::ceremony, Pet::lastEnd). Kept apart from pet.h
// so the hardware-free battle logic (battle.cpp) can test for CER_NONE.

#pragma region Includes
#include <stdint.h>
#pragma endregion

#pragma region Ceremony kinds
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };
#pragma endregion

#pragma once

// Number of elements of a fixed-size array (not a pointer).

#pragma region Includes
#include <stddef.h>
#pragma endregion

#pragma region Array helpers
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#pragma endregion

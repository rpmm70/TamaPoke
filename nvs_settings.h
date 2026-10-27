#pragma once

// Shared NVS (Preferences) settings. Every module stores its keys in the same
// namespace, so a factory reset (Preferences::clear() on this namespace) wipes
// the whole game state at once.

#pragma region Namespace
#define NVS_NAMESPACE "tamapoke"
#pragma endregion

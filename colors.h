#pragma once

// RGB565 colors shared by every screen. The base UI palette (UI_BG_DAY, UI_INK,
// UI_TRACK, UI_BAR_*, UI_WHITE, SPRITE_OUTLINE_COLOR) is generated into
// species.h by tools/sprites.py; this header adds the C565() helper and the
// hand-picked colors that used to be literals scattered through TamaPoke.ino.

#pragma region Includes
#include <stdint.h>
#include "species.h"  // UI_* palette and SPRITE_OUTLINE_COLOR (generated)
#pragma endregion

#pragma region Color helpers
// 8-bit-per-channel RGB to RGB565
#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

// Linear blend from colorA (step 0) to colorB (step totalSteps), per channel
inline uint16_t lerp565(uint16_t colorA, uint16_t colorB, int step, int totalSteps) {
  if (totalSteps <= 0) return colorA;
  int redA = (colorA >> 11) & 31, greenA = (colorA >> 5) & 63, blueA = colorA & 31;
  int redB = (colorB >> 11) & 31, greenB = (colorB >> 5) & 63, blueB = colorB & 31;
  return (uint16_t)((((redA + (redB - redA) * step / totalSteps) << 11)) |
                    (((greenA + (greenB - greenA) * step / totalSteps) << 5)) |
                    (blueA + (blueB - blueA) * step / totalSteps));
}
#pragma endregion

#pragma region UI text colors
// Secondary text (labels, version): darker than UI_TRACK, lighter than UI_INK
#define UI_LABEL C565(0x6B, 0x66, 0x58)
// Text of a medal not earned yet (info card medals page)
#define MEDAL_LOCKED_TEXT_COLOR 0x8410
#pragma endregion

#pragma region Scene colors
// Night sky top ("night blue"): also the blend target of night soil details
#define NIGHT_SKY_TOP_COLOR C565(0x0c, 0x12, 0x24)
// Night sky bottom, just above the horizon
#define NIGHT_SKY_BOTTOM_COLOR C565(0x1e, 0x26, 0x46)
// Biome soil is blended 9/16 toward this color after dark
#define NIGHT_SOIL_BLEND_COLOR C565(0x16, 0x1c, 0x30)
#pragma endregion

#pragma region Accent colors
// Water-type blue: water type chip, DEF stat bar, Dodge button, rare egg label
#define WATER_BLUE_COLOR 0x4C98
// Battle "Attack" button of the Quick / Attack / Heavy overlay
#define BATTLE_NORMAL_ATTACK_COLOR 0xB104
// Weight stat bar on the info card
#define WEIGHT_BAR_COLOR 0xB3C8
// Bond bar on the info card profile page
#define BOND_BAR_COLOR C565(0xd4, 0x52, 0x7e)
// Charging bolt inside the battery icon
#define BATTERY_BOLT_COLOR C565(0xff, 0xd9, 0x4a)
// Water droplets and splashes of the bath shower
#define SHOWER_DROPLET_COLOR C565(0x4A, 0xA8, 0xE8)
#pragma endregion

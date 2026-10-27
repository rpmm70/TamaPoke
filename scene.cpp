// Background scenes: sky by the real time of day, the creature's biome and its
// details, clouds, sun/moon and night stars (see scene.h).

#pragma region Includes
#include "scene.h"
#include "app_state.h"      // pet
#include "board_display.h"  // gfx
#include "boards/layout.h"  // HORIZON, SX()/SY(), LCD_WIDTH/LCD_HEIGHT, CX
#include "colors.h"         // C565, lerp565, NIGHT_* colors, UI_WHITE
#include "countof.h"
#include "dex.h"            // DEX_TBL, BIOME_*
#pragma endregion

#pragma region Constants
// hour sceneHour() reports when there is no clock yet (plain daylight)
static const int SCENE_HOUR_WITHOUT_CLOCK = 13;
// the sky is painted in horizontal bands this many rows tall
static const int SKY_BAND_HEIGHT = 8;
// minigame and battle horizon: lower than HORIZON, leaves room for the action
#define GAME_SCENE_HORIZON_Y SY(376)
// biome soil is blended this many sixteenths toward NIGHT_SOIL_BLEND_COLOR after dark
static const int NIGHT_SOIL_BLEND_SIXTEENTHS = 9;
#pragma endregion

#pragma region Night mode stars
// x/y in 466-space (scaled with SX/SY), size in px drawn literally,
// brightness 0-16 (16 = full white, lower = lerp565 toward the local sky)
struct NightStar { uint16_t x, y; uint8_t size, brightness; };
static const NightStar STARS[] = {
  {120, 140, 4, 16}, {330, 130, 4, 16}, {385, 190, 2, 10}, { 75, 205, 2,  9},
  {275,  95, 3, 14}, {170, 100, 2, 16}, { 68, 125, 3, 12}, {410, 140, 2, 12},
  {225, 105, 2,  9}, {300, 170, 2,  8}, {235, 160, 3, 13}, {160, 185, 2, 11},
  {432, 205, 2,  8}, {112,  92, 2, 10},
};
// these stars dim to STAR_TWINKLE_DIM_BRIGHTNESS for one step out of every
// STAR_TWINKLE_CYCLE_STEPS, offset per star so they never dim together
static const uint8_t STAR_TWINKLE_INDICES[] = { 5, 7, 11 };
static const uint32_t STAR_TWINKLE_STEP_MS = 400;
static const uint8_t STAR_TWINKLE_CYCLE_STEPS = 7;
static const uint8_t STAR_TWINKLE_DIM_BRIGHTNESS = 5;
#pragma endregion

#pragma region Biome soil
// daytime ground of each biome (at night it is blended toward night blue)
static const uint16_t BIOME_SOIL[BIOME_COUNT] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 meadow
  C565(0xdc, 0xca, 0x94),  // 1 beach (sand)
  C565(0x4f, 0x8a, 0x55),  // 2 forest
  C565(0x8a, 0x55, 0x44),  // 3 volcano
  C565(0xa8, 0x90, 0x6a),  // 4 mountain
  C565(0xe6, 0xee, 0xf5),  // 5 snow
};
#pragma endregion

#pragma region Conifer tier shapes
// per tier, bottom first: baseRise60, apexRise60, halfWidth16 (forest and mountain conifers)
static const uint8_t TWO_TIER_SHAPE[2][3] = { { 0, 46, 16 }, { 28, 60, 12 } };
static const uint8_t THREE_TIER_SHAPE[3][3] = { { 0, 34, 16 }, { 22, 50, 12 }, { 40, 60, 8 } };
#pragma endregion

#pragma region Shared sky and soil helpers
// sky gradient colors for the hour: night, dawn (< 8), day (< 18) or dusk
static void skyColorsFor(int hour, bool night, uint16_t &skyTop, uint16_t &skyBottom) {
  if (night)          { skyTop = NIGHT_SKY_TOP_COLOR;      skyBottom = NIGHT_SKY_BOTTOM_COLOR; }
  else if (hour < 8)  { skyTop = C565(0xd1, 0x6a, 0x86); skyBottom = C565(0xf3, 0xb8, 0x7c); }  // dawn
  else if (hour < 18) { skyTop = C565(0x8f, 0xc8, 0xea); skyBottom = C565(0xdc, 0xee, 0xe6); }  // day
  else                { skyTop = C565(0xc7, 0x5a, 0x4a); skyBottom = C565(0xf0, 0xae, 0x64); }  // dusk
}

// the biome ground color, blended toward the night soil color after dark
static uint16_t biomeSoilColor(uint8_t biome, bool night) {
  uint16_t soilColor = BIOME_SOIL[biome < BIOME_COUNT ? biome : 0];
  if (night) soilColor = lerp565(soilColor, NIGHT_SOIL_BLEND_COLOR, NIGHT_SOIL_BLEND_SIXTEENTHS, 16);
  return soilColor;
}

// sky in bands from the top of the screen down to skyHeight (the last band may
// run past it: the ground drawn next covers it)
static void drawSkyGradient(uint16_t skyTop, uint16_t skyBottom, int skyHeight) {
  for (int y = 0; y < skyHeight; y += SKY_BAND_HEIGHT)
    gfx->fillRect(0, y, LCD_WIDTH, SKY_BAND_HEIGHT, lerp565(skyTop, skyBottom, y, skyHeight));
}
#pragma endregion

#pragma region Sky details
// night stars over a sky gradient of skyHeight rows (drawScene and the minigames)
static void drawNightStars(uint32_t nowMs, uint16_t skyTop, uint16_t skyBottom, int skyHeight) {
  uint32_t twinkleSlot = nowMs / STAR_TWINKLE_STEP_MS;
  for (int i = 0; i < (int)countof(STARS); i++) {
    const NightStar &star = STARS[i];

    #pragma region Apply deterministic twinkle
    int effectiveBrightness = star.brightness;
    for (int j = 0; j < (int)countof(STAR_TWINKLE_INDICES); j++) {
      if (STAR_TWINKLE_INDICES[j] == i && (twinkleSlot + j * 3) % STAR_TWINKLE_CYCLE_STEPS == 0)
        effectiveBrightness = STAR_TWINKLE_DIM_BRIGHTNESS;
    }
    #pragma endregion

    #pragma region Pick star color and draw
    uint16_t starColor = UI_WHITE;
    if (effectiveBrightness < 16) {
      uint16_t localSkyColor = lerp565(skyTop, skyBottom, SY(star.y), skyHeight);
      starColor = lerp565(localSkyColor, UI_WHITE, effectiveBrightness, 16);
    }
    gfx->fillRect(SX(star.x), SY(star.y), star.size, star.size, starColor);
    #pragma endregion
  }
}

static void drawClouds(uint32_t now, uint16_t cloudColor) {
  for (int k = 0; k < 2; k++) {
    int cloudX = (int)((now / 50 + k * SX(250)) % SX(560)) - SX(40);
    int cloudY = SY(70) + k * SY(34);
    gfx->fillCircle(cloudX, cloudY, 16, cloudColor);
    gfx->fillCircle(cloudX + 18, cloudY + 3, 13, cloudColor);
    gfx->fillCircle(cloudX - 15, cloudY + 4, 12, cloudColor);
  }
}
#pragma endregion

#pragma region Biome detail helpers
// conifer tiers (triangles), bottom first, from tierGroundY up tierSpanHeight px;
// each tier follows its row of tierShape
static void drawConiferTiers(int treeX, int tierGroundY, int tierSpanHeight, int baseHalfWidth,
                             const uint8_t (*tierShape)[3], int tierCount, uint16_t treeColor) {
  for (int j = 0; j < tierCount; j++) {
    int tierBaseY = tierGroundY - tierSpanHeight * tierShape[j][0] / 60;
    int tierApexY = tierGroundY - tierSpanHeight * tierShape[j][1] / 60;
    int tierHalfWidth = baseHalfWidth * tierShape[j][2] / 16;
    gfx->fillTriangle(treeX, tierApexY, treeX - tierHalfWidth, tierBaseY, treeX + tierHalfWidth, tierBaseY, treeColor);
  }
}
#pragma endregion

#pragma region Public API
int sceneHour() {
  uint32_t lastSeenEpoch = pet.lastSeenEpoch;
  return lastSeenEpoch ? (int)((lastSeenEpoch / 3600) % 24) : SCENE_HOUR_WITHOUT_CLOCK;
}

void drawScene(uint8_t biome, uint32_t now, bool night, int groundBottom) {
  int hour = sceneHour();
  uint16_t skyTop, skyBottom;
  skyColorsFor(hour, night, skyTop, skyBottom);

  #pragma region Sky in bands
  drawSkyGradient(skyTop, skyBottom, HORIZON);
  #pragma endregion

  #pragma region Sun or moon
  // positions hand-placed for 466 px: SX()/SY() rescale them; on the CYD they
  // are approximate, pending eyeball tweaking on a real board
  if (night) {
    gfx->fillCircle(SX(360), SY(78), 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(SX(370), SY(72), 22, lerp565(skyTop, skyBottom, SY(78), HORIZON));  // crescent
    drawNightStars(now, skyTop, skyBottom, HORIZON);
  } else if (hour < 18) {
    gfx->fillCircle(SX(360), SY(84), 26, hour < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(CX, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // setting sun
  }
  #pragma endregion

  #pragma region Ground and hill
  // biome soil (the beach sea is drawn with the biome details, over the hill).
  // The ground stops at groundBottom: below it the caller paints its own panel.
  uint16_t soilColor = biomeSoilColor(biome, night);
  gfx->fillRect(0, HORIZON, LCD_WIDTH, groundBottom - HORIZON, soilColor);
  uint16_t hillColor = lerp565(soilColor, night ? NIGHT_SKY_TOP_COLOR : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, LCD_WIDTH + 120, 60, 30, hillColor);
  #pragma endregion

  // biome details
  uint16_t soilDetailColor = lerp565(soilColor, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == BIOME_MEADOW) {  // meadow: flowering bushes plus grass tufts
    #pragma region Meadow palette (day colors, blended toward night blue after dark)
    const uint16_t nightBlue = NIGHT_SKY_TOP_COLOR;
    uint16_t bushShadeColor = C565(0x3c, 0x8a, 0x4c);
    uint16_t bushHighlightColor = C565(0x58, 0xb8, 0x68);
    uint16_t flowerColors[3] = { C565(0xf0, 0x8a, 0xa4), C565(0xff, 0xd9, 0x5e), C565(0xff, 0xff, 0xff) };
    if (night) {
      bushShadeColor = lerp565(bushShadeColor, nightBlue, 10, 16);
      bushHighlightColor = lerp565(bushHighlightColor, nightBlue, 10, 16);
      for (int colorIndex = 0; colorIndex < 3; colorIndex++)
        flowerColors[colorIndex] = lerp565(flowerColors[colorIndex], nightBlue, 10, 16);
    }
    #pragma endregion

    #pragma region Meadow layout (x and radius in 466-space, flower offsets in tenths of the bush radius)
    struct MeadowBushLayout { int16_t centerX466, radius466; int8_t mirror, flowerCount, colorShift; };
    static const MeadowBushLayout bushLayouts[4] = {
      { 62, 26, 1, 4, 0 },
      { 128, 17, -1, 3, 1 },
      { 338, 21, 1, 3, 2 },
      { 412, 28, -1, 4, 1 },
    };
    static const int8_t flowerOffsets[4][2] = { { -6, -12 }, { 4, -17 }, { 9, -8 }, { -12, -5 } };
    const int bushBaseY = HORIZON + SY(8);
    const int largeFlowerSize = SY(6);
    const int smallFlowerSize = SY(4);
    #pragma endregion

    #pragma region Bushes: two side lobes, main dome, flat base, highlight, flowers
    for (const MeadowBushLayout &bushLayout : bushLayouts) {
      const int bushCenterX = SX(bushLayout.centerX466);
      const int bushRadius = SY(bushLayout.radius466);
      const int mirror = bushLayout.mirror;

      #pragma region Bush body
      gfx->fillCircle(bushCenterX + mirror * (-8 * bushRadius / 10), bushBaseY + (-6 * bushRadius / 10),
                      7 * bushRadius / 10, bushShadeColor);
      gfx->fillCircle(bushCenterX + mirror * (8 * bushRadius / 10), bushBaseY + (-5 * bushRadius / 10),
                      6 * bushRadius / 10, bushShadeColor);
      gfx->fillCircle(bushCenterX, bushBaseY - bushRadius, bushRadius, bushShadeColor);
      gfx->fillRect(bushCenterX - bushRadius, bushBaseY - 4 * bushRadius / 10,
                    2 * bushRadius, 4 * bushRadius / 10 + 1, bushShadeColor);
      gfx->fillCircle(bushCenterX + mirror * (-3 * bushRadius / 10), bushBaseY + (-13 * bushRadius / 10),
                      4 * bushRadius / 10, bushHighlightColor);
      #pragma endregion

      #pragma region Flowers (alternating 3x3 and 2x2 on the CYD)
      for (int flowerIndex = 0; flowerIndex < bushLayout.flowerCount; flowerIndex++) {
        const int flowerSize = (flowerIndex % 2 == 0) ? largeFlowerSize : smallFlowerSize;
        const int flowerX = bushCenterX + mirror * (flowerOffsets[flowerIndex][0] * bushRadius / 10) - flowerSize / 2;
        const int flowerY = bushBaseY + (flowerOffsets[flowerIndex][1] * bushRadius / 10) - flowerSize / 2;
        gfx->fillRect(flowerX, flowerY, flowerSize, flowerSize,
                      flowerColors[(flowerIndex + bushLayout.colorShift) % 3]);
      }
      #pragma endregion
    }
    #pragma endregion

    #pragma region Grass tufts in front (bottom-aligned, shorter side blades)
    const int tuftBaseY = HORIZON + SY(24);
    const int sideBladeHeight = SY(12);
    const int centerBladeHeight = SY(18);
    const int bladeSpacing = SX(7);
    for (int tuftX466 : { 96, 160, 306, 452 }) {
      const int tuftX = SX(tuftX466);
      for (int blade = -1; blade <= 1; blade++) {
        const int bladeHeight = (blade == 0) ? centerBladeHeight : sideBladeHeight;
        gfx->fillRect(tuftX + blade * bladeSpacing, tuftBaseY - bladeHeight, 2, bladeHeight, soilDetailColor);
      }
    }
    #pragma endregion
  } else if (biome == BIOME_BEACH) {  // beach: sea rolling toward the shore, wash edge, palm tree
    #pragma region Beach colors (night uses the night sea colors; the palm blends toward night blue)
    const uint16_t nightBlue = NIGHT_SKY_TOP_COLOR;
    const uint16_t seaColor = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    const uint16_t foamColor = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
    const uint16_t farSeaColor = lerp565(seaColor, C565(0x10, 0x18, 0x20), 4, 16);
    const uint16_t shallowColor = lerp565(seaColor, foamColor, 6, 16);
    const uint16_t wetSandColor = lerp565(soilColor, soilDetailColor, 6, 16);
    uint16_t trunkColor = C565(0xa8, 0x7a, 0x4a);
    uint16_t trunkRingColor = C565(0x7a, 0x54, 0x30);
    uint16_t frondDarkColor = C565(0x2e, 0x7d, 0x3a);
    uint16_t frondLightColor = C565(0x5c, 0xb0, 0x4c);
    uint16_t coconutColor = C565(0x6b, 0x45, 0x22);
    if (night) {
      trunkColor = lerp565(trunkColor, nightBlue, 10, 16);
      trunkRingColor = lerp565(trunkRingColor, nightBlue, 10, 16);
      frondDarkColor = lerp565(frondDarkColor, nightBlue, 10, 16);
      frondLightColor = lerp565(frondLightColor, nightBlue, 10, 16);
      coconutColor = lerp565(coconutColor, nightBlue, 10, 16);
    }
    #pragma endregion

    #pragma region Shoreline geometry (CYD: sea 122-137 deep, 138-143 shallow, wash edge 144-151, wet sand to 152)
    const int seaTopY = HORIZON - SY(52);
    const int shallowTopY = HORIZON - SY(20);
    const int waterRestY = HORIZON - SY(8);
    const int washRange = SY(14);
    const int wetSandHeight = SY(18);
    const int foamRimHeight = SY(4);
    const int tongueRadius = SY(10);
    #pragma endregion

    #pragma region Sea body, far-sea band, wet sand and shallow strip
    gfx->fillRect(0, seaTopY, LCD_WIDTH, shallowTopY - seaTopY, seaColor);
    gfx->fillRect(0, seaTopY, LCD_WIDTH, SY(4), farSeaColor);
    gfx->fillRect(0, waterRestY, LCD_WIDTH, wetSandHeight, wetSandColor);
    gfx->fillRect(0, shallowTopY, LCD_WIDTH, waterRestY - shallowTopY, shallowColor);
    #pragma endregion

    #pragma region Wash tongues: five columns whose foam edge washes up and pulls back
    const uint32_t washPeriodMs = 2000, washRiseMs = 700, columnLagMs = 130;
    static const int8_t columnDepthTwelfths[5] = { 10, 13, 9, 12, 11 };
    const int columnCount = 5;
    const int columnWidth = (LCD_WIDTH + columnCount - 1) / columnCount;
    for (int columnIndex = 0; columnIndex < columnCount; columnIndex++) {
      #pragma region Smoothstep wash amount from 0 to 256 for this column
      const uint32_t washPhaseMs = (now + columnIndex * columnLagMs) % washPeriodMs;
      const int32_t rampAmount = washPhaseMs < washRiseMs
        ? (int32_t)(washPhaseMs * 256 / washRiseMs)
        : (int32_t)((washPeriodMs - washPhaseMs) * 256 / (washPeriodMs - washRiseMs));
      const int32_t washAmount = rampAmount * rampAmount * (768 - 2 * rampAmount) / 65536;
      #pragma endregion

      #pragma region Foam rim, then shallow water inset above it
      const int edgeY = waterRestY + (int)(washRange * columnDepthTwelfths[columnIndex] * washAmount / (12 * 256));
      const int columnX = columnIndex * columnWidth;
      const int tongueHeight = edgeY - shallowTopY;
      gfx->fillRoundRect(columnX, shallowTopY, columnWidth, tongueHeight + foamRimHeight, tongueRadius, foamColor);
      gfx->fillRoundRect(columnX, shallowTopY, columnWidth, tongueHeight, tongueRadius, shallowColor);
      #pragma endregion
    }
    #pragma endregion

    #pragma region Wave crests travelling toward the shore (accelerating, widening, brightening)
    const uint32_t crestPeriodMs = 6000;
    const int crestCount = 3;
    const int crestTopY = seaTopY + SY(6);
    const int crestSpan = waterRestY - SY(2) - crestTopY;
    static const int16_t crestCenters466[3][2] = { { 90, 300 }, { 200, 410 }, { 40, 250 } };
    for (int crestIndex = 0; crestIndex < crestCount; crestIndex++) {
      const uint32_t crestPhaseMs = (now + crestIndex * (crestPeriodMs / crestCount)) % crestPeriodMs;
      const int32_t crestProgress = (int32_t)(crestPhaseMs * 256 / crestPeriodMs);
      const int crestY = crestTopY + (int)(crestSpan * crestProgress * crestProgress / 65536);
      const int crestHalfWidth = SX(14) + (int)(SX(40) * crestProgress / 256);
      const int crestThickness = crestProgress > 128 ? SY(4) : SY(2);
      const uint16_t crestColor = lerp565(seaColor, foamColor, 3 + (int)(crestProgress * 11 / 256), 16);
      for (int segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
        gfx->fillRect(SX(crestCenters466[crestIndex][segmentIndex]) - crestHalfWidth, crestY,
                      2 * crestHalfWidth, crestThickness, crestColor);
      }
    }
    #pragma endregion

    #pragma region Palm trunk: eight tapered segments leaning left, darker ring on each
    const int trunkBaseX = SX(124), trunkBaseY = HORIZON + SY(18);
    const int crownX = SX(100), crownY = SY(128);
    const int trunkSegmentCount = 8;
    const int segmentHeight = (trunkBaseY - crownY) / trunkSegmentCount;
    const int trunkLean = trunkBaseX - crownX;
    for (int segmentIndex = 0; segmentIndex < trunkSegmentCount; segmentIndex++) {
      const int segmentCenterX = trunkBaseX - trunkLean * segmentIndex * segmentIndex / (trunkSegmentCount * trunkSegmentCount);
      const int segmentWidth = SX(14) - (SX(14) - SX(9)) * segmentIndex / (trunkSegmentCount - 1);
      const int segmentTopY = trunkBaseY - (segmentIndex + 1) * segmentHeight;
      gfx->fillRect(segmentCenterX - segmentWidth / 2, segmentTopY, segmentWidth, segmentHeight + 1, trunkColor);
      gfx->fillRect(segmentCenterX - segmentWidth / 2, segmentTopY, segmentWidth, SY(3), trunkRingColor);
    }
    #pragma endregion

    #pragma region Palm crown: hub, six drooping fronds (two triangles each), coconuts
    gfx->fillCircle(crownX, crownY, SY(8), frondDarkColor);
    // { midDx, midDy, tipDx, tipDy, useLightGreen } offsets from the crown in 466-space
    static const int8_t fronds466[6][5] = {
      { -38, -14, -66, 16, 0 }, { 40, -14, 70, 14, 0 }, { -26, -22, -52, -12, 1 },
      { 28, -22, 54, -10, 1 }, { -22, 4, -34, 34, 1 }, { 22, 4, 38, 32, 0 },
    };
    const int frondThickness = SY(6);
    const int frondTipThickness = SY(4);
    for (const auto &frond : fronds466) {
      const uint16_t frondColor = frond[4] ? frondLightColor : frondDarkColor;
      const int midX = crownX + SX(frond[0]), midY = crownY + SY(frond[1]);
      const int tipX = crownX + SX(frond[2]), tipY = crownY + SY(frond[3]);
      gfx->fillTriangle(crownX, crownY - frondThickness, crownX, crownY + frondThickness, midX, midY, frondColor);
      gfx->fillTriangle(midX, midY - frondTipThickness, midX, midY + frondTipThickness, tipX, tipY, frondColor);
    }
    const int coconutRadius = SY(7);
    gfx->fillCircle(crownX - SX(8), crownY + SY(8), coconutRadius, coconutColor);
    gfx->fillCircle(crownX + SX(7), crownY + SY(10), coconutRadius, coconutColor);
    gfx->fillCircle(crownX - SX(1), crownY + SY(15), coconutRadius, coconutColor);
    #pragma endregion
  } else if (biome == BIOME_FOREST) {  // forest: hazy back row, two leafy trees, darker front row of conifers
    #pragma region Forest colors
    uint16_t hazeTarget = night ? C565(0x3a, 0x48, 0x66) : C565(0xb8, 0xd4, 0xcc);
    uint16_t backRowColor = lerp565(soilDetailColor, hazeTarget, night ? 5 : 6, 16);
    uint16_t frontRowColor = lerp565(soilDetailColor, C565(0x10, 0x28, 0x18), 4, 16);
    uint16_t trunkColor = lerp565(soilDetailColor, C565(0x3a, 0x26, 0x18), 9, 16);
    uint16_t canopyColor = lerp565(soilDetailColor, C565(0x6a, 0x96, 0x3a), night ? 3 : 6, 16);
    #pragma endregion

    #pragma region Forest tree tables (466-space)
    // conifers: centerX, height, baseHalfWidth, tierCount, baseOffsetY, isFrontRow
    static const int16_t FOREST_CONIFERS[10][6] = {
      { 40, 95, 22, 2, -2, 0 }, { 112, 130, 26, 3, -1, 0 }, { 200, 62, 18, 2, -2, 0 },   // back row (hazy)
      { 268, 74, 20, 2, -1, 0 }, { 300, 96, 22, 3, -2, 0 }, { 436, 90, 22, 2, -1, 0 },
      { 72, 140, 34, 3, 4, 1 }, { 150, 175, 36, 3, 6, 1 }, { 340, 160, 34, 3, 5, 1 },    // front row (dark)
      { 422, 118, 30, 2, 3, 1 },
    };
    const int BACK_ROW_CONIFER_COUNT = 6;
    // leafy trees: centerX, height, canopyRadius, baseOffsetY
    static const int16_t FOREST_DECIDUOUS[2][4] = { { 24, 78, 22, 5 }, { 384, 92, 24, 4 } };
    #pragma endregion

    #pragma region Trees: back row conifers, then leafy trees, then front row conifers
    for (int pass = 0; pass < 3; pass++) {
      #pragma region Pass 1: leafy trees (thin trunk, two-circle canopy)
      if (pass == 1) {
        for (int i = 0; i < 2; i++) {
          int treeX = SX(FOREST_DECIDUOUS[i][0]);
          int groundY = HORIZON + FOREST_DECIDUOUS[i][3];
          int treeTopY = groundY - SY(FOREST_DECIDUOUS[i][1]);
          int canopyRadius = SX(FOREST_DECIDUOUS[i][2]);
          int canopyCenterY = treeTopY + canopyRadius;
          gfx->fillRect(treeX - 1, canopyCenterY, 3, groundY - canopyCenterY, trunkColor);
          gfx->fillCircle(treeX, canopyCenterY, canopyRadius, canopyColor);
          gfx->fillCircle(treeX + canopyRadius * 6 / 10, canopyCenterY + canopyRadius * 5 / 10,
                          canopyRadius * 7 / 10, canopyColor);
        }
        continue;
      }
      #pragma endregion

      #pragma region Pass 0 and 2: conifers of the matching row
      int firstConifer = (pass == 0) ? 0 : BACK_ROW_CONIFER_COUNT;
      int lastConifer = (pass == 0) ? BACK_ROW_CONIFER_COUNT : (int)countof(FOREST_CONIFERS);
      for (int i = firstConifer; i < lastConifer; i++) {
        int treeX = SX(FOREST_CONIFERS[i][0]);
        int treeHeight = SY(FOREST_CONIFERS[i][1]);
        int baseHalfWidth = SX(FOREST_CONIFERS[i][2]);
        int tierCount = FOREST_CONIFERS[i][3];
        int groundY = HORIZON + FOREST_CONIFERS[i][4];
        bool isFrontRow = FOREST_CONIFERS[i][5] != 0;
        uint16_t bodyColor = isFrontRow ? frontRowColor : backRowColor;

        #pragma region Trunk (front row only): tiers start a few pixels above the ground
        int trunkHeight = isFrontRow ? SY(8) : 0;
        if (isFrontRow) gfx->fillRect(treeX - 1, groundY - trunkHeight - 1, 3, trunkHeight + 1, trunkColor);
        int tierGroundY = groundY - trunkHeight;
        int tierSpanHeight = treeHeight - trunkHeight;
        #pragma endregion

        #pragma region Tiers, bottom first
        drawConiferTiers(treeX, tierGroundY, tierSpanHeight, baseHalfWidth,
                         (tierCount == 3) ? THREE_TIER_SHAPE : TWO_TIER_SHAPE, tierCount, bodyColor);
        #pragma endregion
      }
      #pragma endregion
    }
    #pragma endregion
  } else if (biome == BIOME_VOLCANO) {  // volcano: flat-topped cone, lava down the left slope, rocks and embers
    #pragma region Cone geometry (466-space, scaled with SX/SY)
    const int craterY = HORIZON - SY(120);
    const int craterLeftX = SX(82), craterRightX = SX(118);
    const int baseLeftX = SX(10), baseRightX = SX(180);
    const int litFaceTopX = SX(108), litFaceBaseX = SX(126);
    #pragma endregion

    #pragma region Volcano colors (the cone darkens at night, lava and glow stay bright)
    const uint16_t nightBlue = NIGHT_SOIL_BLEND_COLOR;
    uint16_t coneShadowColor = C565(0x5e, 0x44, 0x3e);
    uint16_t coneLitColor = C565(0x7a, 0x58, 0x4c);
    uint16_t smokeColor = C565(0xa8, 0xa0, 0x9c);
    if (night) {
      coneShadowColor = lerp565(coneShadowColor, nightBlue, 9, 16);
      coneLitColor = lerp565(coneLitColor, nightBlue, 9, 16);
      smokeColor = C565(0x4a, 0x3c, 0x4c);
    }
    const uint16_t lavaColor = C565(0xff, 0x5a, 0x1e);
    const uint16_t lavaCoreColor = C565(0xff, 0xb0, 0x30);
    const uint16_t dripColor = C565(0xff, 0xe8, 0x80);
    static const uint8_t flickerSteps[4] = { 0, 1, 3, 2 };
    const uint16_t glowColor = lerp565(C565(0xff, 0xa8, 0x40), C565(0xff, 0xf0, 0xa0), flickerSteps[(now / 150) % 4], 3);
    #pragma endregion

    #pragma region Smoke puffs rising from the crater (drawn first so the glow and cone sit in front)
    for (int puffIndex = 0; puffIndex < 2; puffIndex++) {
      int puffRise = (int)((now / 80 + puffIndex * SY(24)) % SY(48));
      int puffX = SX(100) + puffIndex * SX(6) + puffRise / 3;
      int puffY = craterY - SY(12) - puffRise;
      gfx->fillCircle(puffX, puffY, SY(6) + puffRise / 6, smokeColor);
    }
    #pragma endregion

    #pragma region Crater glow halo (the cone covers its lower half)
    gfx->fillRoundRect(craterLeftX, craterY - SY(10), craterRightX - craterLeftX, SY(14), SY(6), glowColor);
    #pragma endregion

    #pragma region Cone: flat-topped trapezoid as two triangles, plus a lit right face
    gfx->fillTriangle(craterLeftX, craterY, baseLeftX, HORIZON, baseRightX, HORIZON, coneShadowColor);
    gfx->fillTriangle(craterLeftX, craterY, craterRightX, craterY, baseRightX, HORIZON, coneShadowColor);
    gfx->fillTriangle(litFaceTopX, craterY, craterRightX, craterY, baseRightX, HORIZON, coneLitColor);
    gfx->fillTriangle(litFaceTopX, craterY, baseRightX, HORIZON, litFaceBaseX, HORIZON, coneLitColor);
    gfx->drawFastHLine(craterLeftX, craterY, craterRightX - craterLeftX + 1, glowColor);
    #pragma endregion

    #pragma region Lava stream down the left slope (parallelogram segments, tapering tip)
    const int lavaPointX[4] = { (int)SX(84), (int)SX(70), (int)SX(54), (int)SX(36) };
    const int lavaPointY[4] = { craterY, (int)(HORIZON - SY(84)), (int)(HORIZON - SY(52)), (int)(HORIZON - SY(20)) };
    const int lavaWidth[4] = { (int)SX(10), (int)SX(8), (int)SX(7), 0 };
    for (int segmentIndex = 0; segmentIndex < 3; segmentIndex++) {
      int topX = lavaPointX[segmentIndex], topY = lavaPointY[segmentIndex], topWidth = lavaWidth[segmentIndex];
      int bottomX = lavaPointX[segmentIndex + 1], bottomY = lavaPointY[segmentIndex + 1], bottomWidth = lavaWidth[segmentIndex + 1];
      gfx->fillTriangle(topX, topY, topX + topWidth, topY, bottomX + bottomWidth, bottomY, lavaColor);
      if (bottomWidth > 0) gfx->fillTriangle(topX, topY, bottomX, bottomY, bottomX + bottomWidth, bottomY, lavaColor);
    }
    #pragma endregion

    #pragma region Bright lava core on the upper two segments
    const int coreTopX = SX(87), coreMidX = SX(72), coreTipX = SX(56), coreWidth = SX(4);
    gfx->fillTriangle(coreTopX, lavaPointY[0], coreTopX + coreWidth, lavaPointY[0], coreMidX + coreWidth, lavaPointY[1], lavaCoreColor);
    gfx->fillTriangle(coreTopX, lavaPointY[0], coreMidX, lavaPointY[1], coreMidX + coreWidth, lavaPointY[1], lavaCoreColor);
    gfx->fillTriangle(coreMidX, lavaPointY[1], coreMidX + coreWidth, lavaPointY[1], coreTipX, lavaPointY[2], lavaCoreColor);
    #pragma endregion

    #pragma region Animated drip sliding down the stream centerline
    int dripPhase = (int)((now / 25) % 90);
    int dripSegment = dripPhase / 30, dripStep = dripPhase % 30;
    int dripStartX = lavaPointX[dripSegment] + lavaWidth[dripSegment] / 2;
    int dripEndX = lavaPointX[dripSegment + 1] + lavaWidth[dripSegment + 1] / 2;
    int dripX = dripStartX + (dripEndX - dripStartX) * dripStep / 30;
    int dripY = lavaPointY[dripSegment] + (lavaPointY[dripSegment + 1] - lavaPointY[dripSegment]) * dripStep / 30;
    gfx->fillRect(dripX - 1, dripY - 1, 3, 3, dripColor);
    #pragma endregion

    #pragma region Ground rocks and embers (embers glow day and night)
    gfx->fillTriangle(SX(70), HORIZON, SX(40), HORIZON + 30, SX(100), HORIZON + 30, soilDetailColor);
    gfx->fillTriangle(SX(400), HORIZON + 4, SX(372), HORIZON + 30, SX(430), HORIZON + 30, soilDetailColor);
    for (int ember = 0; ember < 4; ember++)
      gfx->fillRect(SX(120) + ember * SX(70), HORIZON + 8 + (ember % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
    #pragma endregion
  } else if (biome == BIOME_MOUNTAIN) {  // mountain: hazy back range, darker front range, snow caps, small conifers
    #pragma region Mountain colors
    uint16_t hazeTarget = night ? C565(0x5a, 0x66, 0x8a) : C565(0xc8, 0xd8, 0xe8);
    uint16_t backRowColor = lerp565(soilDetailColor, hazeTarget, night ? 5 : 6, 16);
    uint16_t frontRowColor = soilDetailColor;
    uint16_t frontSnowColor = night ? lerp565(C565(0xf4, 0xf8, 0xfc), NIGHT_SKY_BOTTOM_COLOR, 9, 16)
                                    : C565(0xf4, 0xf8, 0xfc);
    uint16_t backSnowColor = lerp565(frontSnowColor, backRowColor, 5, 16);
    uint16_t treeColor = lerp565(soilDetailColor, C565(0x10, 0x30, 0x18), 8, 16);
    #pragma endregion

    #pragma region Peak table (466-space: apexX, peakHeight, baseHalfWidth)
    // the front-left peak stays at 155 so on the round board it clears the header text
    static const int16_t MOUNTAIN_PEAKS[6][3] = {
      { 60, 130, 100 }, { 200, 110, 100 }, { 300, 135, 110 }, { 415, 140, 100 },  // back row (hazy)
      { 110, 155, 120 }, { 395, 150, 105 },                                        // front row (dark)
    };
    const int BACK_ROW_PEAK_COUNT = 4;
    #pragma endregion

    #pragma region Peaks with snow caps (back row first, then front row)
    for (int i = 0; i < (int)countof(MOUNTAIN_PEAKS); i++) {
      bool isBackRow = i < BACK_ROW_PEAK_COUNT;
      uint16_t bodyColor = isBackRow ? backRowColor : frontRowColor;
      uint16_t snowColor = isBackRow ? backSnowColor : frontSnowColor;

      #pragma region Peak body
      int apexX = SX(MOUNTAIN_PEAKS[i][0]);
      int peakHeight = SY(MOUNTAIN_PEAKS[i][1]);
      int baseHalfWidth = SX(MOUNTAIN_PEAKS[i][2]);
      int apexY = HORIZON - peakHeight;
      gfx->fillTriangle(apexX, apexY, apexX - baseHalfWidth, HORIZON, apexX + baseHalfWidth, HORIZON, bodyColor);
      #pragma endregion

      #pragma region Snow cap on the same slopes (top 30 percent of the peak)
      int capDepth = peakHeight * 3 / 10;
      int capHalfWidth = baseHalfWidth * capDepth / peakHeight;
      int capBottomY = apexY + capDepth;
      gfx->fillTriangle(apexX, apexY, apexX - capHalfWidth, capBottomY, apexX + capHalfWidth, capBottomY, snowColor);
      #pragma endregion

      #pragma region Notch in the lower edge of the cap so the snow line is jagged
      int notchHalfWidth = capHalfWidth / 2;
      int notchTipY = capBottomY - capDepth / 3;
      gfx->fillTriangle(apexX, notchTipY, apexX - notchHalfWidth, capBottomY, apexX + notchHalfWidth, capBottomY, bodyColor);
      #pragma endregion
    }
    #pragma endregion

    #pragma region Small conifers at the foot of the range (x466, baseOffsetY, isLarge)
    // the forest two-tier shape (lower tier apex at 46/60, upper tier base at 28/60)
    static const int16_t MOUNTAIN_CONIFERS[6][3] = {
      { 18, 2, 1 }, { 48, -2, 0 }, { 80, 3, 1 }, { 350, 1, 0 }, { 388, -3, 1 }, { 446, 2, 0 },
    };
    for (int i = 0; i < (int)countof(MOUNTAIN_CONIFERS); i++) {
      int treeX = SX(MOUNTAIN_CONIFERS[i][0]);
      int treeBaseY = HORIZON + MOUNTAIN_CONIFERS[i][1];
      int treeHeight = MOUNTAIN_CONIFERS[i][2] ? 30 : 22;
      int treeHalfWidth = MOUNTAIN_CONIFERS[i][2] ? 8 : 6;
      drawConiferTiers(treeX, treeBaseY, treeHeight, treeHalfWidth, TWO_TIER_SHAPE, (int)countof(TWO_TIER_SHAPE), treeColor);
    }
    #pragma endregion
  } else if (biome == BIOME_SNOW) {  // snow: background mounds, frozen lake, falling flakes (day only)
    #pragma region Snow colors (day palette, blended toward night blue after dark)
    const uint16_t nightBlue = NIGHT_SKY_TOP_COLOR;
    uint16_t backMoundLitColor = C565(0xdc, 0xe6, 0xf0);
    uint16_t backMoundShadeColor = C565(0xb0, 0xbe, 0xd2);
    uint16_t frontMoundLitColor = C565(0xf4, 0xf8, 0xfc);
    uint16_t frontMoundShadeColor = C565(0xbc, 0xc8, 0xda);
    uint16_t lakeRimColor = C565(0x7c, 0xa2, 0xc0);
    uint16_t lakeIceColor = C565(0xb0, 0xd2, 0xe8);
    uint16_t lakeReflectionColor = C565(0xd0, 0xe6, 0xf4);
    uint16_t lakeGlintColor = UI_WHITE;
    if (night) {
      backMoundLitColor = lerp565(backMoundLitColor, nightBlue, 9, 16);
      backMoundShadeColor = lerp565(backMoundShadeColor, nightBlue, 9, 16);
      frontMoundLitColor = lerp565(frontMoundLitColor, nightBlue, 9, 16);
      frontMoundShadeColor = lerp565(frontMoundShadeColor, nightBlue, 9, 16);
      lakeRimColor = lerp565(lakeRimColor, nightBlue, 7, 16);
      lakeIceColor = lerp565(lakeIceColor, nightBlue, 7, 16);
      lakeReflectionColor = lerp565(lakeReflectionColor, nightBlue, 7, 16);
      lakeGlintColor = lerp565(lakeGlintColor, nightBlue, 8, 16);
    }
    const uint16_t driftCrestColor = lerp565(hillColor, frontMoundShadeColor, 8, 16);
    const uint16_t lakeCrackColor = lerp565(lakeRimColor, lakeIceColor, 6, 16);
    #pragma endregion

    #pragma region Mound table (466-space: leftX, width, rise; back row first, then front row)
    static const int16_t SNOW_MOUNDS[4][3] = {
      { -20, 150, 34 }, { 250, 170, 40 },  // back row (hazier)
      { 95, 110, 22 }, { 380, 110, 26 },   // front row (whiter)
    };
    const int BACK_ROW_MOUND_COUNT = 2;
    const int shadeWidth = SX(10);
    #pragma endregion

    #pragma region Background snow mounds (shade on the left, the sun is on the right)
    for (int i = 0; i < (int)countof(SNOW_MOUNDS); i++) {
      bool isBackRow = i < BACK_ROW_MOUND_COUNT;
      int moundLeft = SX(SNOW_MOUNDS[i][0]);
      int moundWidth = SX(SNOW_MOUNDS[i][1]);
      int moundRadius = SY(SNOW_MOUNDS[i][2]);
      int moundTop = HORIZON - 14 - moundRadius;
      gfx->fillRoundRect(moundLeft, moundTop, moundWidth, 2 * moundRadius, moundRadius,
                         isBackRow ? backMoundShadeColor : frontMoundShadeColor);
      gfx->fillRoundRect(moundLeft + shadeWidth, moundTop, moundWidth - shadeWidth, 2 * moundRadius, moundRadius,
                         isBackRow ? backMoundLitColor : frontMoundLitColor);
    }
    #pragma endregion

    #pragma region Repaint the front hill over the mound bases so the mounds read as behind it
    // the hill is flat across the visible width from HORIZON-14 down, so this is pixel-identical
    gfx->fillRect(0, HORIZON - 14, LCD_WIDTH, SY(40) + 2, hillColor);
    gfx->drawFastHLine(0, HORIZON - 14, LCD_WIDTH, driftCrestColor);
    #pragma endregion

    #pragma region Frozen lake on the right (rim, ice, paler far half)
    const int lakeLeft = SX(318);
    const int lakeWidth = SX(124);
    const int lakeTop = HORIZON + SY(4);
    const int lakeHeight = SY(18);
    const int lakeInset = SX(4);
    const int iceHeight = lakeHeight - 2;
    gfx->fillRoundRect(lakeLeft, lakeTop, lakeWidth, lakeHeight, lakeHeight / 2, lakeRimColor);
    gfx->fillRoundRect(lakeLeft + lakeInset, lakeTop + 1, lakeWidth - 2 * lakeInset, iceHeight, iceHeight / 2,
                       lakeIceColor);
    gfx->fillRoundRect(lakeLeft + 3 * lakeInset, lakeTop + 1, lakeWidth - 6 * lakeInset, iceHeight / 2,
                       iceHeight / 4, lakeReflectionColor);
    #pragma endregion

    #pragma region Lake glints (white) and a stepped crack (darker ice)
    gfx->drawFastHLine(lakeLeft + SX(26), lakeTop + 2, SX(16), lakeGlintColor);
    gfx->drawFastHLine(lakeLeft + SX(80), lakeTop + lakeHeight - 3, SX(10), lakeGlintColor);
    gfx->drawFastHLine(lakeLeft + SX(50), lakeTop + SY(6), SX(8), lakeCrackColor);
    gfx->drawFastHLine(lakeLeft + SX(56), lakeTop + SY(9), SX(8), lakeCrackColor);
    gfx->drawFastHLine(lakeLeft + SX(62), lakeTop + SY(12), SX(8), lakeCrackColor);
    #pragma endregion

    #pragma region Falling snowflakes (day only)
    if (!night) {
      for (int flake = 0; flake < 10; flake++) {
        int flakeX = (flake * 53 + now / 40) % LCD_WIDTH;
        int flakeY = (flake * 90 + now / 18) % HORIZON;
        gfx->fillRect(flakeX, flakeY, 3, 3, UI_WHITE);
      }
    }
    #pragma endregion
  }
}

void drawGameScene() {
  int hour = sceneHour();
  bool night = isNightHour(hour);
  uint16_t skyTop, skyBottom;
  skyColorsFor(hour, night, skyTop, skyBottom);
  const int gameHorizonY = GAME_SCENE_HORIZON_Y;
  drawSkyGradient(skyTop, skyBottom, gameHorizonY);
  if (night)
    drawNightStars(millis(), skyTop, skyBottom, gameHorizonY);
  uint8_t biome = pet.isEgg() ? BIOME_MEADOW : DEX_TBL[pet.speciesId].biome;
  gfx->fillRect(0, gameHorizonY, LCD_WIDTH, LCD_HEIGHT - gameHorizonY, biomeSoilColor(biome, night));
}
#pragma endregion

#pragma region Battle arena
// Top-down sand court: tiled floor with cracks, blue-gray court lines (outer
// rectangle, center line, center circle, small center circle) and a half circle
// bulging out of each short side. Sized from the screen, so it fits inside the
// bezel on the round board and fills the rectangle on the CYD.
static const uint16_t ARENA_SAND_COLOR = C565(0xD9, 0xC9, 0xA0);
static const uint16_t ARENA_CRACK_COLOR = C565(0xA8, 0x96, 0x6E);
static const uint16_t ARENA_LINE_COLOR = C565(0x66, 0x78, 0x9A);
// floor tiles across the screen width (the reference court shows about 17)
static const int ARENA_TILES_ACROSS = 17;
// gap between the court (or its half circles) and the screen edge
static const int ARENA_EDGE_MARGIN = 6;

// Horizontal spans of a ring between innerRadius and outerRadius around
// (centerX, centerY); drawLeftHalf / drawRightHalf pick which side is drawn.
static void fillRingHalves(int centerX, int centerY, int outerRadius, int innerRadius,
                           bool drawLeftHalf, bool drawRightHalf, uint16_t color) {
  for (int rowOffset = -outerRadius; rowOffset <= outerRadius; rowOffset++) {
    int outerHalfWidth = (int)sqrtf((float)(outerRadius * outerRadius - rowOffset * rowOffset));
    int innerHalfWidth = -1;
    if (abs(rowOffset) < innerRadius)
      innerHalfWidth = (int)sqrtf((float)(innerRadius * innerRadius - rowOffset * rowOffset));
    int rowY = centerY + rowOffset;
    if (innerHalfWidth < 0) {
      // above / below the hole: one span, clipped to the requested halves
      int spanLeft = drawLeftHalf ? centerX - outerHalfWidth : centerX;
      int spanRight = drawRightHalf ? centerX + outerHalfWidth : centerX;
      gfx->drawFastHLine(spanLeft, rowY, spanRight - spanLeft + 1, color);
      continue;
    }
    if (drawLeftHalf)
      gfx->drawFastHLine(centerX - outerHalfWidth, rowY, outerHalfWidth - innerHalfWidth, color);
    if (drawRightHalf)
      gfx->drawFastHLine(centerX + innerHalfWidth + 1, rowY, outerHalfWidth - innerHalfWidth, color);
  }
}

// small cross at every tile corner plus short dashes along some tile edges,
// picked by a fixed hash so the pattern does not flicker between frames
static void drawArenaFloorCracks(int tileSize) {
  int crossArm = tileSize >= 24 ? 2 : 1;
#if BOARD_IS_ROUND
  int visibleRadius = BEZEL_RADIUS - 3;
#endif
  for (int tileRow = 0; tileRow * tileSize <= LCD_HEIGHT; tileRow++) {
    for (int tileColumn = 0; tileColumn * tileSize <= LCD_WIDTH; tileColumn++) {
      int cornerX = tileColumn * tileSize, cornerY = tileRow * tileSize;
#if BOARD_IS_ROUND
      int offsetX = cornerX - CX, offsetY = cornerY - CY;
      if (offsetX * offsetX + offsetY * offsetY > visibleRadius * visibleRadius) continue;
#endif
      uint32_t cornerHash = (uint32_t)(tileColumn * 73856093) ^ (uint32_t)(tileRow * 19349663);
      cornerHash ^= cornerHash >> 13;
      if (cornerHash & 3) {
        gfx->drawFastHLine(cornerX - crossArm, cornerY, 2 * crossArm + 1, ARENA_CRACK_COLOR);
        gfx->drawFastVLine(cornerX, cornerY - crossArm, 2 * crossArm + 1, ARENA_CRACK_COLOR);
      }
      if (cornerHash & 4) {
        int dashStart = tileSize * (1 + ((cornerHash >> 4) & 3)) / 6;
        int dashLength = tileSize * (1 + ((cornerHash >> 6) & 1)) / 5;
        gfx->drawFastHLine(cornerX + dashStart, cornerY, dashLength, ARENA_CRACK_COLOR);
      }
      if (cornerHash & 8) {
        int dashStart = tileSize * (1 + ((cornerHash >> 8) & 3)) / 6;
        int dashLength = tileSize * (1 + ((cornerHash >> 10) & 1)) / 5;
        gfx->drawFastVLine(cornerX, cornerY + dashStart, dashLength, ARENA_CRACK_COLOR);
      }
    }
  }
}

void drawBattleArena() {
  #pragma region Court size from the screen
#if BOARD_IS_ROUND
  // corners on a circle inside the bezel, with the reference court's 1.37 aspect
  int courtHalfHeight = BEZEL_RADIUS * 56 / 100;
#else
  int courtHalfHeight = LCD_HEIGHT / 2 - ARENA_EDGE_MARGIN;
#endif
  int widestHalfWidth = LCD_WIDTH / 2 - ARENA_EDGE_MARGIN - courtHalfHeight * 26 / 100;
  int courtHalfWidth = min(courtHalfHeight * 137 / 100, widestHalfWidth);
  int goalRadius = min(courtHalfHeight * 36 / 100, LCD_WIDTH / 2 - ARENA_EDGE_MARGIN - courtHalfWidth);
  int centerCircleRadius = courtHalfHeight * 56 / 100;
  int centerSpotRadius = courtHalfHeight * 11 / 100;
  int lineWidth = max(2, (int)SK(3));
  int courtLeft = CX - courtHalfWidth, courtTop = CY - courtHalfHeight;
  int courtWidth = 2 * courtHalfWidth, courtHeight = 2 * courtHalfHeight;
  #pragma endregion

  #pragma region Sand floor and tile cracks
#if BOARD_IS_ROUND
  gfx->fillScreen(RGB565_BLACK);
#endif
  fillScreenBg(ARENA_SAND_COLOR);
  drawArenaFloorCracks(LCD_WIDTH / ARENA_TILES_ACROSS);
  #pragma endregion

  #pragma region Court lines
  for (int inset = 0; inset < lineWidth; inset++)
    gfx->drawRect(courtLeft + inset, courtTop + inset, courtWidth - 2 * inset, courtHeight - 2 * inset, ARENA_LINE_COLOR);
  gfx->fillRect(CX - lineWidth / 2, courtTop, lineWidth, courtHeight, ARENA_LINE_COLOR);
  fillRingHalves(CX, CY, centerCircleRadius, centerCircleRadius - lineWidth, true, true, ARENA_LINE_COLOR);
  fillRingHalves(CX, CY, centerSpotRadius, centerSpotRadius - lineWidth, true, true, ARENA_LINE_COLOR);
  fillRingHalves(courtLeft + lineWidth / 2, CY, goalRadius, goalRadius - lineWidth, true, false, ARENA_LINE_COLOR);
  fillRingHalves(courtLeft + courtWidth - 1 - lineWidth / 2, CY, goalRadius, goalRadius - lineWidth, false, true,
                 ARENA_LINE_COLOR);
  #pragma endregion
}
#pragma endregion

#pragma region Coin chase field
// Top-down grass field in mowed stripes with small tufts, fenced by a red rail
// with square posts just inside the screen edge (a ring along the bezel on the
// round board, a rectangle on the CYD).
static const uint16_t FIELD_GRASS_LIGHT_COLOR = C565(0x7C, 0xC4, 0x5C);
static const uint16_t FIELD_GRASS_DARK_COLOR = C565(0x6C, 0xB4, 0x4E);
static const uint16_t FIELD_TUFT_COLOR = C565(0x4E, 0x94, 0x3A);
static const uint16_t FENCE_RAIL_COLOR = C565(0xB8, 0x32, 0x2A);
static const uint16_t FENCE_POST_COLOR = C565(0xD8, 0x44, 0x36);
static const uint16_t FENCE_POST_OUTLINE_COLOR = C565(0x7A, 0x1E, 0x18);
// mowed stripes and tuft cells across the screen width
static const int FIELD_STRIPES_ACROSS = 8;
static const int FIELD_TUFT_CELLS_ACROSS = 14;

// two or three short blades per cell at a hashed offset (fixed: no flicker)
static void drawFieldGrassTufts(int cellSize) {
  int bladeHeight = max(2, cellSize / 7);
  for (int cellRow = 0; cellRow * cellSize < LCD_HEIGHT; cellRow++) {
    for (int cellColumn = 0; cellColumn * cellSize < LCD_WIDTH; cellColumn++) {
      uint32_t cellHash = (uint32_t)(cellColumn * 83492791) ^ (uint32_t)(cellRow * 2654435761u);
      cellHash ^= cellHash >> 15;
      if ((cellHash & 3) == 0) continue;  // about a quarter of the cells stay plain
      int tuftX = cellColumn * cellSize + (int)((cellHash >> 2) % (uint32_t)cellSize);
      int tuftY = cellRow * cellSize + (int)((cellHash >> 10) % (uint32_t)cellSize);
      gfx->drawFastVLine(tuftX - 2, tuftY + 1, bladeHeight - 1, FIELD_TUFT_COLOR);
      gfx->drawFastVLine(tuftX, tuftY, bladeHeight, FIELD_TUFT_COLOR);
      if (cellHash & 4) gfx->drawFastVLine(tuftX + 2, tuftY + 1, bladeHeight - 1, FIELD_TUFT_COLOR);
    }
  }
}

// square post with a dark outline, centered on (postCenterX, postCenterY)
static void drawFencePost(int postCenterX, int postCenterY, int postSize) {
  int postLeft = postCenterX - postSize / 2, postTop = postCenterY - postSize / 2;
  gfx->fillRect(postLeft, postTop, postSize, postSize, FENCE_POST_COLOR);
  gfx->drawRect(postLeft, postTop, postSize, postSize, FENCE_POST_OUTLINE_COLOR);
}

void drawCoinChaseField() {
  #pragma region Mowed grass stripes and tufts
  int stripeWidth = (LCD_WIDTH + FIELD_STRIPES_ACROSS - 1) / FIELD_STRIPES_ACROSS;
  for (int stripe = 0; stripe < FIELD_STRIPES_ACROSS; stripe++)
    gfx->fillRect(stripe * stripeWidth, 0, stripeWidth, LCD_HEIGHT,
                  (stripe & 1) ? FIELD_GRASS_DARK_COLOR : FIELD_GRASS_LIGHT_COLOR);
  drawFieldGrassTufts(LCD_WIDTH / FIELD_TUFT_CELLS_ACROSS);
  #pragma endregion

  #pragma region Fence: rail plus posts
  int railWidth = max(3, (int)SK(5));
  int postSize = max(7, (int)SK(13));
  int fenceInset = postSize / 2 + 1;  // posts stay fully on screen
#if BOARD_IS_ROUND
  int fenceRadius = BEZEL_RADIUS - fenceInset - 2;
  fillRingHalves(CX, CY, fenceRadius + railWidth / 2, fenceRadius - (railWidth - 1) / 2, true, true, FENCE_RAIL_COLOR);
  // posts every ~36 px of the 466 board's fence circle
  int postCount = (int)(2.0f * (float)M_PI * fenceRadius / SK(36));
  for (int post = 0; post < postCount; post++) {
    float postAngle = 2.0f * (float)M_PI * post / postCount;
    drawFencePost(CX + (int)lroundf(cosf(postAngle) * fenceRadius), CY + (int)lroundf(sinf(postAngle) * fenceRadius),
                  postSize);
  }
  // black outside the bezel: hides the stripes and tufts past the round panel
  int cornerDistance = (int)ceilf(sqrtf((float)(CX * CX + CY * CY))) + 1;
  fillRingHalves(CX, CY, cornerDistance, BEZEL_RADIUS + 1, true, true, RGB565_BLACK);
#else
  int fenceLeft = fenceInset, fenceTop = fenceInset;
  int fenceRight = LCD_WIDTH - 1 - fenceInset, fenceBottom = LCD_HEIGHT - 1 - fenceInset;
  int railOffset = railWidth / 2;
  gfx->fillRect(fenceLeft - railOffset, fenceTop - railOffset, fenceRight - fenceLeft + railWidth, railWidth, FENCE_RAIL_COLOR);
  gfx->fillRect(fenceLeft - railOffset, fenceBottom - railOffset, fenceRight - fenceLeft + railWidth, railWidth,
                FENCE_RAIL_COLOR);
  gfx->fillRect(fenceLeft - railOffset, fenceTop - railOffset, railWidth, fenceBottom - fenceTop + railWidth, FENCE_RAIL_COLOR);
  gfx->fillRect(fenceRight - railOffset, fenceTop - railOffset, railWidth, fenceBottom - fenceTop + railWidth,
                FENCE_RAIL_COLOR);
  // posts at the corners and evenly spaced (about every 36 px of the 466 board) along each side
  int postSpacing = SK(36);
  int postsAcross = max(1, (fenceRight - fenceLeft + postSpacing / 2) / postSpacing);
  int postsDown = max(1, (fenceBottom - fenceTop + postSpacing / 2) / postSpacing);
  for (int post = 0; post <= postsAcross; post++) {
    int postX = fenceLeft + (fenceRight - fenceLeft) * post / postsAcross;
    drawFencePost(postX, fenceTop, postSize);
    drawFencePost(postX, fenceBottom, postSize);
  }
  for (int post = 1; post < postsDown; post++) {
    int postY = fenceTop + (fenceBottom - fenceTop) * post / postsDown;
    drawFencePost(fenceLeft, postY, postSize);
    drawFencePost(fenceRight, postY, postSize);
  }
#endif
  #pragma endregion
}
#pragma endregion

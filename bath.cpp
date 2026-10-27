// Bath scene (see bath.h): foam bubbles over the creature, and the cleanup when
// they pop. Every distance is a 466-px design value scaled with SX()/SY()/SK()
// (an exact no-op on the 466x466 round board).

#pragma region Includes
#include "bath.h"
#include <math.h>
#include "app_state.h"      // pet, pmd, beh, bathUntil, bathPending, bubbles
#include "board_display.h"  // gfx
#include "boards/layout.h"  // SX()/SY()/SK()
#include "colors.h"         // BATH_BUBBLE_OUTLINE_COLOR
#include "sprite_draw.h"    // pmdActTotalMs
#include "ui_widgets.h"     // millisPhaseRadians
#pragma endregion

#pragma region Constants
static const uint32_t BATH_DURATION_MS = 3000;
// the last BATH_POP_MS of the bath show the popping sparkles instead of the foam
static const uint32_t BATH_POP_MS = 800;
// bubbles spawn within BATH_SPAWN_HALF_WIDTH px of the creature, up to BATH_SPAWN_HEIGHT above its ground
#define BATH_SPAWN_HALF_WIDTH SX(70)
#define BATH_SPAWN_WIDTH SX(140)
#define BATH_SPAWN_HEIGHT SY(150)
// bubble radius: BATH_BUBBLE_MIN_RADIUS plus up to BATH_BUBBLE_RADIUS_RANGE - 1
#define BATH_BUBBLE_MIN_RADIUS SK(8)
#define BATH_BUBBLE_RADIUS_RANGE SK(16)
static const int BATH_BUBBLE_PHASE_RANGE = 64;
// foam sway: radians per ms and px per side
static const float BATH_SWAY_RADIANS_PER_MS = 1.0f / 220.0f;
#define BATH_SWAY_AMPLITUDE SX(6)
// foam rise: 1 px (466-px units) every BATH_RISE_MS_PER_PX ms
static const uint32_t BATH_RISE_MS_PER_PX = 90;
// sparkles of the first BATH_SPARKLE_COUNT bubbles, BATH_SPARKLE_RISE px above them
static const int BATH_SPARKLE_COUNT = 8;
#define BATH_SPARKLE_RISE SY(18)
// the clean creature strikes its pose for this many loops of the action
static const uint32_t BATH_POSE_LOOPS = 2;
#pragma endregion

#pragma region Public API
void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + BATH_DURATION_MS;
  bathPending = true;
  int creatureCenterX = (int)beh.x;
  for (auto &bubble : bubbles) {
    bubble.x = creatureCenterX - BATH_SPAWN_HALF_WIDTH + random(BATH_SPAWN_WIDTH);
    bubble.y = PET_GROUND - random(BATH_SPAWN_HEIGHT);
    bubble.r = BATH_BUBBLE_MIN_RADIUS + random(BATH_BUBBLE_RADIUS_RANGE);
    bubble.ph = random(BATH_BUBBLE_PHASE_RANGE);
  }
}

void drawBath() {
  uint32_t now = millis();
  #pragma region Bath over: apply the cleaning once, with a happy pose
  if (!timeLeft(bathUntil)) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      if (pmd.has(PMD_POSE)) {
        beh.mode = PET_BEHAVIOR_GESTURE;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * BATH_POSE_LOOPS;
      }
    }
    return;
  }
  #pragma endregion
  uint32_t remainingMs = bathUntil - now;
  if (remainingMs > BATH_POP_MS) {
    #pragma region Foam: bubbles swaying and slowly rising
    float swayPhase = millisPhaseRadians(now, BATH_SWAY_RADIANS_PER_MS);
    for (auto &bubble : bubbles) {
      int bubbleX = bubble.x + (int)(sinf(swayPhase + bubble.ph) * BATH_SWAY_AMPLITUDE);
      int bubbleY = bubble.y - SY((int)((BATH_DURATION_MS - remainingMs) / BATH_RISE_MS_PER_PX));
      gfx->fillCircle(bubbleX, bubbleY, bubble.r, UI_WHITE);
      gfx->drawCircle(bubbleX, bubbleY, bubble.r, BATH_BUBBLE_OUTLINE_COLOR);
      gfx->fillCircle(bubbleX - bubble.r / 3, bubbleY - bubble.r / 3, bubble.r / 4, UI_BG_DAY);
    }
    #pragma endregion
  } else {
    #pragma region The bubbles pop: sparkles
    for (int i = 0; i < BATH_SPARKLE_COUNT; i++) {
      auto &bubble = bubbles[i];
      int sparkleX = bubble.x + (i % 3) * 6 - 6, sparkleY = bubble.y - BATH_SPARKLE_RISE;
      uint16_t sparkleColor = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sparkleX - 6, sparkleY - 1, 13, 3, sparkleColor);
      gfx->fillRect(sparkleX - 1, sparkleY - 6, 3, 13, sparkleColor);
    }
    #pragma endregion
  }
}
#pragma endregion

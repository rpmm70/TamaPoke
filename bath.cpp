// Bath scene (see bath.h): a shower in the bathroom of drawShowerRoom(), water
// droplets falling from its head over the creature, and the cleanup when it
// ends. Every distance is a 466-px design value scaled with SX()/SY()/SK()
// (an exact no-op on the 466x466 round board).

#pragma region Includes
#include "bath.h"
#include "app_state.h"      // pet, pmd, beh, bathUntil, bathPending, PET_GROUND
#include "board_display.h"  // gfx
#include "boards/layout.h"  // SX()/SY()/SK()/SKM()
#include "colors.h"         // SHOWER_DROPLET_COLOR, UI_WHITE
#include "scene.h"          // SHOWER_HEAD_CX, SHOWER_HEAD_FACE_Y, SHOWER_HEAD_HALF_WIDTH
#include "sprite_draw.h"    // pmdActTotalMs
#pragma endregion

#pragma region Constants
static const uint32_t BATH_DURATION_MS = 3000;
// droplets in the spray; each falls for SHOWER_FALL_MS, splashes for
// SHOWER_SPLASH_MS, then starts again from the head at a new spot
static const int SHOWER_DROPLET_COUNT = 28;
static const uint32_t SHOWER_FALL_MS = 520;
static const uint32_t SHOWER_SPLASH_MS = 140;
static const uint32_t SHOWER_DROPLET_PERIOD_MS = SHOWER_FALL_MS + SHOWER_SPLASH_MS;
// by the floor the spray is this wide, in percent of the head's face
static const int SHOWER_SPRAY_SPREAD_PERCENT = 170;
// droplet: a round bottom of SHOWER_DROPLET_RADIUS with a point SHOWER_DROPLET_TAIL above its center
#define SHOWER_DROPLET_RADIUS SKM(3)
#define SHOWER_DROPLET_TAIL SKM(9)
// splash: two specks thrown SHOWER_SPLASH_REACH px sideways and up to SHOWER_SPLASH_RISE px high
#define SHOWER_SPLASH_REACH SK(10)
#define SHOWER_SPLASH_RISE SK(8)
#define SHOWER_SPLASH_SPECK_RADIUS SKM(2)
// the clean creature strikes its pose for this many loops of the action
static const uint32_t BATH_POSE_LOOPS = 2;
#pragma endregion

#pragma region Internal helpers
// 0..range-1, fixed for one fall of one droplet (no jitter from frame to frame)
static int dropletFallHash(int dropletIndex, uint32_t fallNumber, int range) {
  uint32_t hash = (uint32_t)(dropletIndex * 83492791) ^ (fallNumber * 2654435761u);
  hash ^= hash >> 15;
  hash *= 2246822519u;
  hash ^= hash >> 13;
  return (int)(hash % (uint32_t)range);
}

// teardrop centered on (dropletX, dropletY), point up, with a white glint
static void drawShowerDroplet(int dropletX, int dropletY) {
  gfx->fillCircle(dropletX, dropletY, SHOWER_DROPLET_RADIUS, SHOWER_DROPLET_COLOR);
  gfx->fillTriangle(dropletX - SHOWER_DROPLET_RADIUS, dropletY, dropletX + SHOWER_DROPLET_RADIUS, dropletY,
                    dropletX, dropletY - SHOWER_DROPLET_TAIL, SHOWER_DROPLET_COLOR);
  gfx->drawPixel(dropletX - SHOWER_DROPLET_RADIUS / 2, dropletY - SHOWER_DROPLET_RADIUS / 2, UI_WHITE);
}

// two specks flying out of the landing point; splashPermille 0..1000 over SHOWER_SPLASH_MS
static void drawShowerSplash(int landingX, int splashPermille) {
  int speckOffsetX = SHOWER_SPLASH_REACH * splashPermille / 1000;
  // up then back down: highest halfway through the splash
  int speckRise = SHOWER_SPLASH_RISE * 4 * splashPermille * (1000 - splashPermille) / 1000000;
  int speckY = PET_GROUND - speckRise;
  gfx->fillCircle(landingX - speckOffsetX, speckY, SHOWER_SPLASH_SPECK_RADIUS, SHOWER_DROPLET_COLOR);
  gfx->fillCircle(landingX + speckOffsetX, speckY, SHOWER_SPLASH_SPECK_RADIUS, SHOWER_DROPLET_COLOR);
}
#pragma endregion

#pragma region Public API
void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  uint32_t now = millis();
  bathUntil = now + BATH_DURATION_MS;
  bathPending = true;
  #pragma region The creature stands still under the shower head while the water runs
  beh.x = beh.targetX = SHOWER_HEAD_CX;
  beh.mode = PET_BEHAVIOR_IDLE;
  beh.t0 = now;
  beh.until = bathUntil;
  #pragma endregion
}

bool updateBathScene() {
  if (!bathUntil) return false;
  if (timeLeft(bathUntil)) return true;
  #pragma region Shower over: apply the cleaning once, with a happy pose
  bathUntil = 0;
  if (bathPending) {
    bathPending = false;
    pet.clean();
    if (pmd.has(PMD_POSE)) {
      uint32_t now = millis();
      beh.mode = PET_BEHAVIOR_GESTURE;
      beh.act = PMD_POSE;
      beh.t0 = now;
      beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * BATH_POSE_LOOPS;
    }
  }
  #pragma endregion
  return false;
}

void drawBath() {
  if (!bathUntil) return;
  uint32_t showerElapsedMs = millis() - (bathUntil - BATH_DURATION_MS);
  // a droplet's center goes from just under the head's face down to the floor
  int fallHeight = PET_GROUND - SHOWER_HEAD_FACE_Y - SHOWER_DROPLET_TAIL;
  for (int i = 0; i < SHOWER_DROPLET_COUNT; i++) {
    #pragma region Where this droplet is in its fall-and-splash cycle
    // the droplets are spread evenly over one cycle so the spray stays steady
    uint32_t dropletTimeMs = showerElapsedMs + (uint32_t)i * SHOWER_DROPLET_PERIOD_MS / SHOWER_DROPLET_COUNT;
    uint32_t fallNumber = dropletTimeMs / SHOWER_DROPLET_PERIOD_MS;
    uint32_t cycleMs = dropletTimeMs % SHOWER_DROPLET_PERIOD_MS;
    if (cycleMs > showerElapsedMs) continue;  // this fall began before the water was turned on
    int headOffsetX = dropletFallHash(i, fallNumber, 2 * SHOWER_HEAD_HALF_WIDTH + 1) - SHOWER_HEAD_HALF_WIDTH;
    int landingX = SHOWER_HEAD_CX + headOffsetX * SHOWER_SPRAY_SPREAD_PERCENT / 100;
    #pragma endregion
    if (cycleMs < SHOWER_FALL_MS) {
      #pragma region Falling: speeding up and spreading out toward the floor
      int fallPermille = (int)(cycleMs * 1000 / SHOWER_FALL_MS);
      int dropPermille = fallPermille * (1000 + fallPermille) / 2000;  // gentle acceleration
      int dropletX = SHOWER_HEAD_CX + headOffsetX * (100 + (SHOWER_SPRAY_SPREAD_PERCENT - 100) * fallPermille / 1000) / 100;
      int dropletY = SHOWER_HEAD_FACE_Y + SHOWER_DROPLET_TAIL + fallHeight * dropPermille / 1000;
      drawShowerDroplet(dropletX, dropletY);
      #pragma endregion
    } else {
      #pragma region Landed: splash on the floor
      drawShowerSplash(landingX, (int)((cycleMs - SHOWER_FALL_MS) * 1000 / SHOWER_SPLASH_MS));
      #pragma endregion
    }
  }
}
#pragma endregion

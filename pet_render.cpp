// The creature on the main screen (see pet_render.h): sprite loading, the PMD
// behavior scheduler, the B/W and flash fallbacks, the heart and the sleep bubble.

#pragma region Includes
#include "pet_render.h"
#include <math.h>
#include "app_state.h"      // pet, mon, pmd, evoPmd, beh, monFor, lastSpriteDrawnBounds, inkColor
#include "board_display.h"  // gfx
#include "boards/layout.h"  // CX, SX()/SY(), PET_HEART_SCALE, SLEEP_BUBBLE_*
#include "ceremony_fx.h"    // drawEvolveFX
#include "colors.h"
#include "countof.h"
#include "i18n.h"           // T()
#include "sdmon.h"          // thumbs, sdDirty, PMD_*
#include "species.h"        // SPECIES, SPR_HEART, SPRITE_W/H, SPRITE_OUTLINE_COLOR
#include "sprite_draw.h"
#include "ui_text.h"
#pragma endregion

#pragma region Constants
// little hop of a happy creature: up this many px every other half second
static const int PET_HOP_HEIGHT = 6;
static const uint32_t PET_HOP_PERIOD_MS = 500;
// the evolving creature alternates silhouette / color every this many ms
static const uint32_t EVOLVE_FLASH_PERIOD_MS = 300;
// flash sprites blink for BLINK_DURATION_MS out of every BLINK_CYCLE_MS
static const uint32_t BLINK_CYCLE_MS = 3500;
static const uint32_t BLINK_DURATION_MS = 300;
// zoom limit of the main creature's PMD sprite
static const uint8_t PET_PMD_MAX_SCALE = 5;
// the Hop pose shown while eating (sprites with no Eat pose) keeps this much of its jump height
static const uint8_t EATING_HOP_HEIGHT_PERCENT = 35;
// PMD walk: px per frame, and how close to the target counts as arrived
static const float PET_WALK_STEP_PX = 3.0f;
static const float PET_WALK_ARRIVED_PX = 4.0f;
// behavior scheduler: walk chance, gesture chance (cumulative percent) and timings
static const int BEHAVIOR_WALK_CHANCE_PCT = 35;
static const int BEHAVIOR_GESTURE_CHANCE_PCT = 60;
static const uint32_t BEHAVIOR_WALK_MAX_MS = 15000;
static const uint32_t BEHAVIOR_IDLE_MIN_MS = 2000;
static const uint32_t BEHAVIOR_IDLE_RANDOM_MS = 3000;
// "missing sprites" notice: big "?" and two lines of text around PET_CY
static const int MISSING_SPRITE_MARK_SIZE = 6;
static const int MISSING_SPRITE_MARK_DX = 18;
static const int MISSING_SPRITE_MARK_DY = 80;
static const int MISSING_SPRITE_LINE1_DY = 4;
static const int MISSING_SPRITE_LINE2_DY = 20;
static const int MISSING_SPRITE_TEXT_SIZE = 2;
// default frame time of a legacy B/W sprite with frameMs 0
static const uint16_t LEGACY_SPRITE_FRAME_MS = 100;
#pragma endregion

#pragma region Flash sprites
// the 9 species with their own sprite in flash (fallback without SD): dex -> index
static int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= (int16_t)(countof(IDX) - 1)) ? IDX[dex] : -1;
}

// closed eye: erases the 3x4 eye and draws the eyelid
static void overlayEye(const Species &species, int x, int y, int s, int eyeColumn) {
  gfx->fillRect(x + eyeColumn * s, y + species.eyeRow * s, 3 * s, 4 * s, species.bodyColor);
  gfx->fillRect(x + eyeColumn * s, y + (species.eyeRow + 2) * s, 3 * s, s, SPRITE_OUTLINE_COLOR);
}

// erases the base smile and draws an open mouth (eating) or a frown (sad)
static void overlayMouth(const Species &species, int x, int y, int s, bool open) {
  int mouthColumn = species.mouthCol, mouthRow = species.mouthRow;
  gfx->fillRect(x + (mouthColumn - 3) * s, y + mouthRow * s, 7 * s, 2 * s, species.bodyColor);
  if (open) {
    gfx->fillRect(x + (mouthColumn - 2) * s, y + mouthRow * s, 5 * s, 2 * s, SPRITE_OUTLINE_COLOR);
  } else {
    gfx->fillRect(x + (mouthColumn - 2) * s, y + mouthRow * s, 5 * s, s, SPRITE_OUTLINE_COLOR);
    gfx->fillRect(x + (mouthColumn - 3) * s, y + (mouthRow + 1) * s, s, s, SPRITE_OUTLINE_COLOR);
    gfx->fillRect(x + (mouthColumn + 3) * s, y + (mouthRow + 1) * s, s, s, SPRITE_OUTLINE_COLOR);
  }
}
#pragma endregion

#pragma region Sprite loading
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  // freshly received files may include a new thumbs.bin, which was only
  // loaded in setup(): without this it did not show in the gallery until a reboot
  if (sdDirty) { thumbs.unload(); thumbs.load(); }
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = CX;
  beh.mode = PET_BEHAVIOR_IDLE;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);                   // main: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // fallback: B/W
  }
}
#pragma endregion

#pragma region PMD creature: behavior scheduler and drawing
// picks the creature's next whim when it is happy
static void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int roll = random(100);
  if (roll < BEHAVIOR_WALK_CHANCE_PCT && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = PET_BEHAVIOR_WALK;
    beh.targetX = SX(150) + random(SX(176));  // SX(): unscaled, the CYD walked half off its 320 px screen
    beh.until = now + BEHAVIOR_WALK_MAX_MS;
  } else if (roll < BEHAVIOR_GESTURE_CHANCE_PCT) {
    #pragma region Random gesture among the available ones (Hop excluded: jumps too high; Sit excluded: faces backwards)
    static const uint8_t gestureActions[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t availableGestures[countof(gestureActions)], availableCount = 0;
    for (uint8_t gestureAction : gestureActions)
      if (pmd.has(gestureAction)) availableGestures[availableCount++] = gestureAction;
    if (availableCount) {
      beh.mode = PET_BEHAVIOR_GESTURE;
      beh.act = availableGestures[random(availableCount)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    #pragma endregion
    beh.mode = PET_BEHAVIOR_IDLE;
    beh.until = now + BEHAVIOR_IDLE_MIN_MS + random(BEHAVIOR_IDLE_RANDOM_MS);
  } else {
    beh.mode = PET_BEHAVIOR_IDLE;  // face forward
    beh.until = now + BEHAVIOR_IDLE_MIN_MS + random(BEHAVIOR_IDLE_RANDOM_MS);
  }
}

static void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // evolution finished: free the previous form

  #pragma region Pick the action from the mood (happy: the scheduler decides)
  PetMood mood = pet.mood();
  uint8_t act;
  bool loop = true;
  bool keepIdleScale = false;
  uint8_t jumpHeightPercent = 100;
  if (mood == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = PET_BEHAVIOR_IDLE;
  } else if (mood == MOOD_EATING && (pmd.has(PMD_EAT) || pmd.has(PMD_HOP))) {
    // most sprites have no Eat pose: a small happy Hop stands in, at the Idle zoom so it does not shrink
    act = pmd.has(PMD_EAT) ? PMD_EAT : PMD_HOP;
    keepIdleScale = true;
    if (act == PMD_HOP) jumpHeightPercent = EATING_HOP_HEIGHT_PERCENT;
    beh.t0 = 0;
  } else if (mood == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // happy: the scheduler decides (idle / walk / gesture)
    if (now > beh.until) behNext();
    if (beh.mode == PET_BEHAVIOR_WALK) {
      float distanceToTarget = beh.targetX - beh.x;
      if (fabsf(distanceToTarget) < PET_WALK_ARRIVED_PX) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (distanceToTarget > 0 ? PET_WALK_STEP_PX : -PET_WALK_STEP_PX);
        act = (distanceToTarget > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == PET_BEHAVIOR_GESTURE) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }
  #pragma endregion

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, PET_PMD_MAX_SCALE, keepIdleScale,
             jumpHeightPercent);
  // the "likes it" heart is drawn by render() next to the head (drawPetHeart)
}
#pragma endregion

#pragma region Legacy B/W creature
// animated sprite from the SD: integer per-pixel zoom, frames at their own pace
static void drawPetSD() {
  int scale = mon.scale;
  int drawnWidth = mon.w * scale, drawnHeight = mon.h * scale;
  int left = CX - drawnWidth / 2;
  int top = PET_CY - drawnHeight / 2;

  bool silhouette = false;
  if (pet.evolving()) {
    silhouette = (millis() / EVOLVE_FLASH_PERIOD_MS) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / PET_HOP_PERIOD_MS) % 2) {
    top -= PET_HOP_HEIGHT;  // little hop
  }

  uint16_t frameMs = mon.frameMs ? mon.frameMs : LEGACY_SPRITE_FRAME_MS;
  uint16_t frameIndex = pet.sleeping ? 0 : (millis() / frameMs) % mon.frames;
  const uint8_t *framePixels = mon.data + (uint32_t)frameIndex * mon.w * mon.h;
  drawIndexedPixelsAtWholeScale(framePixels, mon.w, mon.h, left, top, scale, mon.pal, silhouette);
  lastSpriteDrawnBounds = { (int16_t)left, (int16_t)top, (int16_t)(left + drawnWidth - 1), (int16_t)(top + drawnHeight - 1),
                            true };

  // emotes instead of expressions (imported sprites have no anchors): the heart
  // is drawn by render() next to the head (drawPetHeart)
}
#pragma endregion

#pragma region Creature dispatcher
void drawPet() {
  lastSpriteDrawnBounds.valid = false;  // each path below fills it (sleep bubble anchor)
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int flashIndex = flashIdxForDex(pet.speciesId);
  if (flashIndex < 0) {
    #pragma region No SD and no flash sprite: clear notice that sprites are missing
    gfx->setTextColor(inkColor());
    setSize(MISSING_SPRITE_MARK_SIZE);
    setCur(CX - MISSING_SPRITE_MARK_DX, PET_CY - MISSING_SPRITE_MARK_DY);
    printT("?");
    setSize(MISSING_SPRITE_TEXT_SIZE);
    const char *firstLine = T(S_NO_SPRITES);
    setCur(centerX(firstLine, MISSING_SPRITE_TEXT_SIZE), PET_CY - MISSING_SPRITE_LINE1_DY);
    printT(firstLine);
    const char *secondLine = T(S_LOAD_SPRITES);
    setCur(centerX(secondLine, MISSING_SPRITE_TEXT_SIZE), PET_CY + MISSING_SPRITE_LINE2_DY);
    printT(secondLine);
    return;
    #pragma endregion
  }
  const Species &species = SPECIES[flashIndex];
  int scale = species.scale;
  int left = CX - 16 * scale;
  int top = PET_CY - 16 * scale;

  #pragma region Evolution animation: alternates the silhouette of the previous and the new form
  if (pet.evolving()) {
    bool flash = (millis() / EVOLVE_FLASH_PERIOD_MS) % 2;
    int16_t shownDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int shownFlashIndex = flashIdxForDex(shownDex);
    if (shownFlashIndex >= 0) {
      const Species &shown = SPECIES[shownFlashIndex];
      drawMap(shown.sprite, SPRITE_H, CX - 16 * shown.scale, PET_CY - 16 * shown.scale, shown.scale, flash);
    }
    return;
  }
  #pragma endregion

  PetMood mood = pet.mood();
  if (mood == MOOD_HAPPY && (millis() / PET_HOP_PERIOD_MS) % 2) top -= PET_HOP_HEIGHT;  // little hop

  drawMap(species.sprite, SPRITE_H, left, top, scale, false);
  lastSpriteDrawnBounds = { (int16_t)left, (int16_t)top, (int16_t)(left + SPRITE_W * scale - 1),
                            (int16_t)(top + SPRITE_H * scale - 1), true };

  #pragma region Overlaid expressions using the species anchors
  bool blink = (millis() % BLINK_CYCLE_MS < BLINK_DURATION_MS);
  if (mood == MOOD_SLEEPING || blink) {
    overlayEye(species, left, top, scale, species.eyeColL);
    overlayEye(species, left, top, scale, species.eyeColR);
  }
  if (mood == MOOD_EATING) overlayMouth(species, left, top, scale, true);
  else if (mood == MOOD_SAD) overlayMouth(species, left, top, scale, false);
  // the "likes it" heart is drawn by render() next to the head (drawPetHeart)
  #pragma endregion
}
#pragma endregion

#pragma region Heart and sleep bubble next to the head
// White rounded bubble above and to the right of the drawn sprite box, with a
// tail pointing down at the head. Without a box (nothing drawn) it sits where the
// old "Zz" text was. Kept inside the screen; sizes in boards/layout.h (SLEEP_BUBBLE_*).
// A point on the head, three quarters across the drawn sprite box (where the
// sleep bubble tail and the heart sit); a fixed spot when nothing was drawn
static void findPetHeadAnchor(int &headX, int &headY) {
  headX = SX(320);
  headY = SY(150);
  if (!lastSpriteDrawnBounds.valid) return;
  int boxWidth = lastSpriteDrawnBounds.right - lastSpriteDrawnBounds.left + 1;
  headX = lastSpriteDrawnBounds.left + boxWidth * 3 / 4;
  headY = lastSpriteDrawnBounds.top + SLEEP_BUBBLE_HEAD_INSET;
}

// "likes it" heart by the head, like the sleep bubble: its lower-left corner just
// above and right of the anchor. Asleep, the bubble takes that side, so the heart
// mirrors to the left of the anchor. Kept inside the same screen limits.
// SPR_HEART only fills columns 5..26 and rows 6..21 of its 32x32 grid
static const int HEART_VISIBLE_LEFT_COL = 5;
static const int HEART_VISIBLE_RIGHT_COL = 26;
static const int HEART_VISIBLE_TOP_ROW = 6;
static const int HEART_VISIBLE_BOTTOM_ROW = 21;
static const int HEART_MAP_SIZE = 32;

void drawPetHeart() {
  int headX, headY;
  findPetHeadAnchor(headX, headY);
  #pragma region Place the visible heart: tip just above the anchor, beside it
  int heartTop = headY - (HEART_VISIBLE_BOTTOM_ROW + 1) * PET_HEART_SCALE - SLEEP_BUBBLE_GAP;
  int heartLeft = pet.sleeping ? headX - SLEEP_BUBBLE_GAP - (HEART_VISIBLE_RIGHT_COL + 1) * PET_HEART_SCALE
                               : headX + SLEEP_BUBBLE_GAP - HEART_VISIBLE_LEFT_COL * PET_HEART_SCALE;
  #pragma endregion
  #pragma region Keep the visible heart inside the same limits as the bubble
  int maxLeft = SLEEP_BUBBLE_MAX_RIGHT - (HEART_VISIBLE_RIGHT_COL + 1) * PET_HEART_SCALE;
  int minLeft = -HEART_VISIBLE_LEFT_COL * PET_HEART_SCALE;
  int minTop = SLEEP_BUBBLE_MIN_TOP - HEART_VISIBLE_TOP_ROW * PET_HEART_SCALE;
  if (heartLeft > maxLeft) heartLeft = maxLeft;
  if (heartLeft < minLeft) heartLeft = minLeft;
  if (heartTop < minTop) heartTop = minTop;
  #pragma endregion
  drawMap(SPR_HEART, HEART_MAP_SIZE, heartLeft, heartTop, PET_HEART_SCALE, false);
}

void drawSleepBubble() {
  const char *sleepText = "Zzz";
  setSize(SLEEP_BUBBLE_TEXT_SIZE);  // before textW(): a CJK font measures with the active size
  int textWidth = textW(sleepText, SLEEP_BUBBLE_TEXT_SIZE);
  int bubbleWidth = textWidth + 2 * SLEEP_BUBBLE_PAD_X;
  int bubbleHeight = FONT_H * SLEEP_BUBBLE_TEXT_SIZE + 2 * SLEEP_BUBBLE_PAD_Y;

  int headX, headY;
  findPetHeadAnchor(headX, headY);

  #pragma region Bubble box: bottom-left corner just above the anchor, clamped inside the screen
  int bubbleLeft = headX + SLEEP_BUBBLE_GAP;
  int bubbleTop = headY - SLEEP_BUBBLE_TAIL - bubbleHeight;
  if (bubbleLeft > SLEEP_BUBBLE_MAX_RIGHT - bubbleWidth) bubbleLeft = SLEEP_BUBBLE_MAX_RIGHT - bubbleWidth;
  if (bubbleTop < SLEEP_BUBBLE_MIN_TOP) bubbleTop = SLEEP_BUBBLE_MIN_TOP;
  #pragma endregion

  #pragma region Tail toward the head, then the bubble over its base, then the text
  int tailBaseY = bubbleTop + bubbleHeight - 1;
  int tailBaseLeft = bubbleLeft + SLEEP_BUBBLE_PAD_X / 2;
  int tailBaseRight = tailBaseLeft + SLEEP_BUBBLE_TAIL;
  gfx->fillTriangle(tailBaseLeft, tailBaseY, tailBaseRight, tailBaseY, headX, headY, UI_WHITE);
  gfx->drawLine(tailBaseLeft, tailBaseY, headX, headY, UI_INK);
  gfx->drawLine(tailBaseRight, tailBaseY, headX, headY, UI_INK);
  gfx->fillRoundRect(bubbleLeft, bubbleTop, bubbleWidth, bubbleHeight, SLEEP_BUBBLE_R, UI_WHITE);
  gfx->drawRoundRect(bubbleLeft, bubbleTop, bubbleWidth, bubbleHeight, SLEEP_BUBBLE_R, UI_INK);
  gfx->drawLine(tailBaseLeft + 1, tailBaseY, tailBaseRight - 1, tailBaseY, UI_WHITE);  // opens the border onto the tail
  gfx->setTextColor(UI_INK);  // always dark on the white bubble, day or night
  setCur(bubbleLeft + SLEEP_BUBBLE_PAD_X, bubbleTop + SLEEP_BUBBLE_PAD_Y);
  printT(sleepText);
  #pragma endregion
}
#pragma endregion

// Coin chase: tap a spot and the creature walks there; each coin it touches is a
// point and extra time (see minigame_coin.h).

#pragma region Includes
#include "minigame_coin.h"
#include <math.h>
#include "app_state.h"        // coin chase state and COIN_* tuning, pet, lastSpriteDrawnBounds, isNightHour
#include "audio.h"            // sfxPlay
#include "board_display.h"    // gfx
#include "boards/layout.h"    // COIN_* layout, MINIGAME_SCORE_*
#include "colors.h"
#include "i18n.h"             // T()
#include "minigame_common.h"  // canStartMinigame, drawMinigamePet, isMinigameOkTapped, drawMinigameResults
#include "scene.h"            // drawGameScene, sceneHour
#include "ui_text.h"          // ctext
#include "ui_widgets.h"       // drawScreenCloseButton, isTouchOnScreenCloseButton
#pragma endregion

#pragma region Constants
// the timer turns red with this much time left
#define COIN_LOW_TIME_WARNING_MS 5000
// the three circles of the coin, outside in
#define COIN_RIM_COLOR C565(0x7e, 0x55, 0x30)
#define COIN_EDGE_COLOR C565(0xe0, 0xb8, 0x60)
#define COIN_FACE_COLOR C565(0xff, 0xd9, 0x5e)
#pragma endregion

#pragma region Play area and walking speed
// keeps a body-center point inside the play area: the bezel circle on the round
// board (trimmed above for the timer and below for the score), a rectangle on the CYD
static void clampToCoinPlayArea(float &bodyX, float &bodyY) {
#if BOARD_IS_ROUND
  float offsetX = bodyX - CX, offsetY = bodyY - CY;
  float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
  if (distance > COIN_PLAY_RADIUS) {
    bodyX = CX + offsetX * COIN_PLAY_RADIUS / distance;
    bodyY = CY + offsetY * COIN_PLAY_RADIUS / distance;
  }
#else
  if (bodyX < COIN_PLAY_LEFT) bodyX = COIN_PLAY_LEFT;
  if (bodyX > COIN_PLAY_RIGHT) bodyX = COIN_PLAY_RIGHT;
#endif
  if (bodyY < COIN_PLAY_TOP) bodyY = COIN_PLAY_TOP;
  if (bodyY > COIN_PLAY_BOTTOM) bodyY = COIN_PLAY_BOTTOM;
}

// px/s from the speed stat: higher SPE walks faster, clamped so nobody crawls or teleports
static float coinPetSpeedPerSecond() {
  uint16_t speedStat = pet.speStat();
  if (speedStat > COIN_SPEED_STAT_CAP) speedStat = COIN_SPEED_STAT_CAP;
  return COIN_PET_SPEED_MIN + (COIN_PET_SPEED_MAX - COIN_PET_SPEED_MIN) * speedStat / COIN_SPEED_STAT_CAP;
}
#pragma endregion

#pragma region Coins
// new coin at a random spot in the play area, not right next to the creature
static void respawnCoin() {
  coinSpawnedAt = millis();
  float petBodyY = coinPetFeetY - COIN_PET_BODY_OFFSET;
  for (int attempt = 0; attempt < COIN_SPAWN_ATTEMPTS; attempt++) {
#if BOARD_IS_ROUND
    coinX = CX - COIN_PLAY_RADIUS + random(2 * COIN_PLAY_RADIUS + 1);
#else
    coinX = COIN_PLAY_LEFT + random(COIN_PLAY_RIGHT - COIN_PLAY_LEFT + 1);
#endif
    coinY = COIN_PLAY_TOP + random(COIN_PLAY_BOTTOM - COIN_PLAY_TOP + 1);
    clampToCoinPlayArea(coinX, coinY);
    float offsetX = coinX - coinPetX, offsetY = coinY - petBodyY;
    if (offsetX * offsetX + offsetY * offsetY >= (float)COIN_SPAWN_MIN_DISTANCE * COIN_SPAWN_MIN_DISTANCE) return;
  }
  // every attempt landed too close: the last one stays (the creature grabs it at once)
}

// the coin circle overlaps the creature's opaque pixel box from the last drawn frame
static bool isCoinTouchingSprite() {
  if (!lastSpriteDrawnBounds.valid) return false;
  #pragma region Closest point of the box to the coin center
  float closestX = coinX, closestY = coinY;
  if (closestX < lastSpriteDrawnBounds.left) closestX = lastSpriteDrawnBounds.left;
  if (closestX > lastSpriteDrawnBounds.right) closestX = lastSpriteDrawnBounds.right;
  if (closestY < lastSpriteDrawnBounds.top) closestY = lastSpriteDrawnBounds.top;
  if (closestY > lastSpriteDrawnBounds.bottom) closestY = lastSpriteDrawnBounds.bottom;
  #pragma endregion
  float offsetX = coinX - closestX, offsetY = coinY - closestY;
  return offsetX * offsetX + offsetY * offsetY <= (float)COIN_RADIUS * COIN_RADIUS;
}
#pragma endregion

#pragma region Start and touch
void startCoinGame() {
  if (!canStartMinigame()) return;
  coinOpen = true;
  coinOver = false;
  coinPoints = 0;
  coinSpeedGain = 0;
  coinNewHi = false;
  coinTimeLeftMs = COIN_START_TIME_MS;
  coinPetX = coinTargetX = CX;
  coinPetFeetY = coinTargetFeetY = COIN_PLAY_BOTTOM + COIN_PET_BODY_OFFSET;
  coinPetFacingLeft = false;
  lastSpriteDrawnBounds.valid = false;  // from another screen: no pickup until the creature is drawn here
  lastCoinStep = millis();
  respawnCoin();
}

void coinGameTap(int16_t x, int16_t y) {
  if (coinOver) {
    if (isMinigameOkTapped(x, y)) {
      coinOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (isTouchOnScreenCloseButton(x, y)) {  // X = quit with no reward
    coinOpen = false;
    return;
  }
  #pragma region The tapped spot becomes the body destination
  float targetBodyX = x, targetBodyY = y;
  clampToCoinPlayArea(targetBodyX, targetBodyY);
  coinTargetX = targetBodyX;
  coinTargetFeetY = targetBodyY + COIN_PET_BODY_OFFSET;
  #pragma endregion
}
#pragma endregion

#pragma region Round step
// ends the round: reward (SPE training, joy, record) and the results screen
static void finishCoinGame() {
  coinNewHi = (coinPoints > pet.coinHi);
  coinSpeedGain = pet.coinChaseResult(coinPoints);
  sfxPlay(coinNewHi && coinPoints > 0 ? SFX_MEDAL : SFX_LEVEL);
  coinOver = true;
}

static void stepCoinGame() {
  uint32_t now = millis();
  #pragma region Elapsed real time (clamped for an abnormally late frame)
  uint32_t elapsedMs = now - lastCoinStep;
  if (elapsedMs > COIN_STEP_MAX_MS) elapsedMs = COIN_STEP_MAX_MS;
  lastCoinStep = now;
  #pragma endregion

  #pragma region Walk toward the tapped spot and stop there
  float offsetX = coinTargetX - coinPetX, offsetY = coinTargetFeetY - coinPetFeetY;
  float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
  float stepLength = coinPetSpeedPerSecond() * elapsedMs / 1000.0f;
  if (distance <= stepLength) {
    coinPetX = coinTargetX;
    coinPetFeetY = coinTargetFeetY;
  } else {
    coinPetX += offsetX * stepLength / distance;
    coinPetFeetY += offsetY * stepLength / distance;
  }
  if (offsetX < -1) coinPetFacingLeft = true;
  else if (offsetX > 1) coinPetFacingLeft = false;
  #pragma endregion

  #pragma region Coin pickup when the coin touches the creature: +1 point, bonus time (capped), new coin
  if (isCoinTouchingSprite()) {
    coinPoints++;
    sfxPlay(SFX_PLAY);
    coinTimeLeftMs += COIN_BONUS_TIME_MS;
    if (coinTimeLeftMs > COIN_MAX_TIME_MS) coinTimeLeftMs = COIN_MAX_TIME_MS;
    if (coinPoints >= COIN_WIN_POINTS) {
      finishCoinGame();
      return;
    }
    respawnCoin();
  } else if (now - coinSpawnedAt >= COIN_LIFETIME_MS) {
    respawnCoin();  // missed: the coin moves somewhere else
  }
  #pragma endregion

  #pragma region Clock: the round ends at 0 s
  coinTimeLeftMs -= (int32_t)elapsedMs;
  if (coinTimeLeftMs <= 0) {
    coinTimeLeftMs = 0;
    finishCoinGame();
  }
  #pragma endregion
}
#pragma endregion

#pragma region Drawing
// gold coin with a darker rim and a shine: three circles, cheaper than a scaled bitmap
static void drawCoin(int centerX, int centerY) {
  gfx->fillCircle(centerX, centerY, COIN_RADIUS, COIN_RIM_COLOR);
  gfx->fillCircle(centerX, centerY, COIN_RADIUS - 2, COIN_EDGE_COLOR);
  gfx->fillCircle(centerX, centerY, COIN_RADIUS - 4, COIN_FACE_COLOR);
  gfx->fillCircle(centerX - COIN_RADIUS / 3, centerY - COIN_RADIUS / 3, COIN_RADIUS / 4, UI_WHITE);
}

void renderCoinGame() {
  const uint16_t ink = UI_INK;  // the field is light grass at any hour

  if (coinOver) {
    drawMinigameResults(S_SCORE_FMT, coinPoints, coinNewHi && coinPoints > 0, coinPoints >= COIN_WIN_POINTS, pet.coinHi,
                        coinSpeedGain, S_SPD_GAIN_FMT);
    return;
  }

  drawCoinChaseField();
  stepCoinGame();
  if (coinOver) return;  // the round just ended: the results screen draws next frame

  #pragma region Timer (top middle): whole seconds, rounded up
  char timerText[8];
  snprintf(timerText, sizeof(timerText), "%ld", (long)((coinTimeLeftMs + 999) / 1000));
  gfx->setTextColor(coinTimeLeftMs <= COIN_LOW_TIME_WARNING_MS ? UI_BAR_BAD : ink);
  ctext(timerText, COIN_TIMER_TEXT_SIZE, COIN_TIMER_Y);
  #pragma endregion

  drawCoin((int)coinX, (int)coinY);

  #pragma region The creature: walks while it has somewhere to go
  bool walking = coinPetX != coinTargetX || coinPetFeetY != coinTargetFeetY;
  uint8_t act = !walking ? PMD_IDLE : coinPetFacingLeft ? PMD_WALKL : PMD_WALKR;
  drawMinigamePet(act, (int)coinPetX, (int)coinPetFeetY, COIN_PET_MAX_SCALE, COIN_PET_SIZE_PERCENT);
  #pragma endregion

  #pragma region Score (bottom, centered on CX)
  char scoreLine[24];
  snprintf(scoreLine, sizeof(scoreLine), T(S_SCORE_FMT), coinPoints);
  gfx->setTextColor(ink);
  ctext(scoreLine, MINIGAME_SCORE_TEXT_SIZE, MINIGAME_SCORE_Y, MINIGAME_SCORE_MAX_W);
  #pragma endregion

  drawScreenCloseButton();  // quit (see coinGameTap)
  gfx->flush();
}
#pragma endregion

// Ball game ("keepy-uppy"): keep the pokeball in the air by tapping it (see minigame_ball.h).

#pragma region Includes
#include "minigame_ball.h"
#include <math.h>
#include "app_state.h"        // ball game state and BALL_* tuning, pet
#include "audio.h"            // sfxPlay
#include "board_display.h"    // gfx
#include "boards/layout.h"    // ARENA_*, LIFE_*, BALL_*, GAME_PET_*, MINIGAME_SCORE_*
#include "colors.h"
#include "i18n.h"             // T()
#include "minigame_common.h"  // canStartMinigame, drawMinigamePet, isMinigameOkTapped, drawMinigameResults
#include "scene.h"            // drawBallGameStage
#include "species.h"          // SPR_ICON_PLAY
#include "sprite_draw.h"      // drawMap
#include "ui_text.h"          // drawGlyphCenteredText
#include "ui_widgets.h"       // sineOfMillis, drawScreenCloseButton, isTouchOnScreenCloseButton
#pragma endregion

#pragma region Constants
// misses (balls that touch the ground) before the round ends
#define BALL_LIVES 3
// the creature follows the ball: this ratio of the horizontal gap per frame, at most this many px
#define BALL_PET_CHASE_RATIO 0.12f
#define BALL_PET_CHASE_MAX_STEP 7.0f
// the creature walks toward the ball once it is more than this many px to one side
#define BALL_PET_WALK_DEAD_ZONE 4
// impact ring: lasts this long, starts at this radius and grows 1 px every HIT_RING_MS_PER_PIXEL ms
#define HIT_RING_DURATION_MS 260
#define HIT_RING_START_RADIUS 22
#define HIT_RING_MS_PER_PIXEL 6
#define HIT_RING_OUTER_COLOR C565(0xff, 0xe7, 0x9f)
#define HIT_RING_INNER_COLOR C565(0xff, 0xd9, 0x8a)
#pragma endregion

#pragma region Ball physics helpers
static void respawnBall() {
  ballX = SX(112) + random(SX(242));
  ballY = SY(82);
  float respawnSpeed = BALL_RESPAWN_SPEED_BASE + gameScore * BALL_RESPAWN_SPEED_PER_POINT;  // livelier as you progress
  if (respawnSpeed > BALL_RESPAWN_SPEED_MAX) respawnSpeed = BALL_RESPAWN_SPEED_MAX;
  ballVX = random(2) ? respawnSpeed : -respawnSpeed;
  ballVX += ((int)random(9) - 4) * BALL_RESPAWN_JITTER_STEP;
  ballVY = BALL_RESPAWN_VY;
}

// touch radius around the ball: shrinks as the score grows, floored for resistive touch
static int ballHitRadius() {
  int hitRadius = gameScore >= 20 ? BALL_HIT_RADIUS_LATE : gameScore >= 8 ? BALL_HIT_RADIUS_MID : BALL_HIT_RADIUS_START;
  return hitRadius < BALL_HIT_RADIUS_MIN ? BALL_HIT_RADIUS_MIN : hitRadius;
}

// random sideways noise added on every wall bounce
static float ballWallNoise() {
  return ((int)random(9) - 4) * BALL_WALL_NOISE_STEP;
}
#pragma endregion

#pragma region Start and touch
void startGame() {
  if (!canStartMinigame()) return;
  gameOpen = true;
  gameOver = false;
  gameAttackGain = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  hitTime = 0;
  ballLastHitAt = 0;
  gamePetX = CX;
  lastGameStep = millis();
  respawnBall();
}

void gameTap(int16_t x, int16_t y) {
  if (gameOver) {
    if (isMinigameOkTapped(x, y)) {
      gameOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (isTouchOnScreenCloseButton(x, y)) {  // X = quit with no reward
    gameOpen = false;
    return;
  }
  float offsetX = ballX - x, offsetY = ballY - y;
  int hitRadius = ballHitRadius();
  if (offsetX * offsetX + offsetY * offsetY < hitRadius * hitRadius) {  // the ball was hit!
    #pragma region Ignore hits during the cooldown or while the ball still rises
    uint32_t now = millis();
    if (now - ballLastHitAt < BALL_HIT_COOLDOWN_MS || ballVY < BALL_HIT_MIN_VY) return;
    #pragma endregion

    #pragma region Lift, drift away from the finger, chaos and a random reversal
    gameScore++;
    sfxPlay(SFX_PLAY);
    uint16_t liftScore = gameScore > BALL_LIFT_SCORE_CAP ? BALL_LIFT_SCORE_CAP : gameScore;
    float lift = BALL_LIFT_BASE + liftScore * BALL_LIFT_PER_POINT;
    ballVY = -lift;
    float drift = BALL_DRIFT_BASE + (gameScore >= BALL_DRIFT_STEP1_SCORE ? BALL_DRIFT_STEP1 : 0.0f) +
                  (gameScore >= BALL_DRIFT_STEP2_SCORE ? BALL_DRIFT_STEP2 : 0.0f);
    float chaos = ((int)random(17) - 8) * BALL_CHAOS_STEP;
    ballVX += offsetX * drift + chaos;
    if (random(100) < BALL_REVERSE_CHANCE_PCT) ballVX = -ballVX * (0.72f + random(45) * 0.01f);
    if (ballVX > BALL_MAX_VX) ballVX = BALL_MAX_VX;
    if (ballVX < -BALL_MAX_VX) ballVX = -BALL_MAX_VX;
    #pragma endregion

    hitX = ballX;
    hitY = ballY;
    hitTime = now;
    ballLastHitAt = now;
  }
}
#pragma endregion

#pragma region Physics step
static void stepGame() {
  uint32_t now = millis();
  #pragma region Frame-time factor (normalised to the 115 ms frame of the fork)
  // without this the physics advanced "one step per frame" and the game ran
  // faster or slower depending on how long the previous frame took to draw,
  // which depends on the size of the loaded sprite
  float frameFactor = lastGameStep ? (now - lastGameStep) / BALL_FRAME_MS : 1.0f;
  if (frameFactor > BALL_FRAME_FACTOR_MAX) frameFactor = BALL_FRAME_FACTOR_MAX;  // abnormally late frame: do not take a huge jump
  lastGameStep = now;
  #pragma endregion

  #pragma region Gravity, wind sway and random kicks
  float gravity = BALL_GRAVITY_BASE + gameScore * BALL_GRAVITY_PER_POINT;  // falls faster each time
  if (gameScore >= BALL_GRAVITY_STEP1_SCORE) gravity += BALL_GRAVITY_STEP1;
  if (gameScore >= BALL_GRAVITY_STEP2_SCORE) gravity += BALL_GRAVITY_STEP2;
  if (gravity > BALL_GRAVITY_MAX) gravity = BALL_GRAVITY_MAX;
  ballVX += sineOfMillis(now + gameScore * BALL_WIND_SCORE_PHASE_MS, BALL_WIND_RATE) * BALL_WIND_AMPLITUDE * frameFactor;
  if (random(1000) < (long)(BALL_KICK_CHANCE_PCT * 10.0f * frameFactor)) ballVX += ((int)random(7) - 3) * BALL_KICK_STEP;
  ballVY += gravity * frameFactor;
  ballX += ballVX * frameFactor;
  ballY += ballVY * frameFactor;
  #pragma endregion

#if BOARD_IS_ROUND
  #pragma region Bounce off the circular wall (physical round bezel)
  float offsetX = ballX - CX, offsetY = ballY - CY;
  float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
  if (distance > ARENA_WALL_R) {
    float normalX = offsetX / distance, normalY = offsetY / distance;
    float dot = ballVX * normalX + ballVY * normalY;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * normalX) * BALL_WALL_GAIN_X + ballWallNoise();
      ballVY = (ballVY - 2 * dot * normalY) * BALL_WALL_GAIN_Y;
      sfxPlay(SFX_BALL_BOUNCE);
    }
    ballX = CX + normalX * ARENA_WALL_R;
    ballY = CY + normalY * ARENA_WALL_R;
  }
  #pragma endregion
#else
  #pragma region Bounce off left and right (gain and noise) and the ceiling (0.88)
  if (ballX < ARENA_LEFT) {
    ballX = ARENA_LEFT;
    ballVX = fabsf(ballVX) * BALL_WALL_GAIN_X + ballWallNoise();
    sfxPlay(SFX_BALL_BOUNCE);
  }
  if (ballX > ARENA_RIGHT) {
    ballX = ARENA_RIGHT;
    ballVX = -fabsf(ballVX) * BALL_WALL_GAIN_X + ballWallNoise();
    sfxPlay(SFX_BALL_BOUNCE);
  }
  if (ballY < ARENA_TOP) {
    ballY = ARENA_TOP;
    ballVY = fabsf(ballVY) * BALL_WALL_GAIN_Y;
    sfxPlay(SFX_BALL_BOUNCE);
  }
  // the narrow arena bounces often: keep the 1.05 gain from building up past the hit cap
  if (ballVX > BALL_MAX_VX) ballVX = BALL_MAX_VX;
  if (ballVX < -BALL_MAX_VX) ballVX = -BALL_MAX_VX;
  #pragma endregion
#endif

  #pragma region Ground: a miss, or the end of the round after the last life
  if (ballY > ARENA_GROUND_Y) {
    if (++gameMisses >= BALL_LIVES) {
      gameNewHi = (gameScore > pet.gameHi);
      uint8_t attackBefore = pet.trAtk;
      pet.playResult(gameScore);  // trains ATK, updates the record and gives happiness
      gameAttackGain = pet.trAtk - attackBefore;
      sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOver = true;
    } else {
      respawnBall();
      sfxPlay(SFX_BALL_MISS);
    }
  }
  #pragma endregion

  #pragma region The creature follows the ball from below
  float chase = (ballX - gamePetX) * BALL_PET_CHASE_RATIO;
  if (chase > BALL_PET_CHASE_MAX_STEP) chase = BALL_PET_CHASE_MAX_STEP;
  if (chase < -BALL_PET_CHASE_MAX_STEP) chase = -BALL_PET_CHASE_MAX_STEP;
  gamePetX += chase * frameFactor;
  #pragma endregion
}
#pragma endregion

#pragma region Drawing
void renderGame() {
  // no fillScreen(BLACK): drawBallGameStage covers the full screen. If the
  // previous flush's DMA is still reading the buffer, it sees valid content
  // (not half-painted black), which was the flicker at 25 fps.
  const uint16_t ink = UI_WHITE;  // the stage is dark (wall, valance, apron) at any hour

  if (gameOver) {
    drawMinigameResults(S_SCORE_FMT, gameScore, gameNewHi && gameScore > 0, false, pet.gameHi, gameAttackGain, S_ATK_GAIN_FMT);
    return;
  }

  drawBallGameStage();
  stepGame();

  #pragma region Record and lives (top)
  // 24 and not 12: in Japanese the record label is 10 bytes before the number and
  // the record was cut to its first digit. Text buffer sizes are watched by the
  // formatted text buffer test in test/test_tools.py.
  char recordText[24];
  snprintf(recordText, sizeof(recordText), T(S_REC_FMT), pet.gameHi);
  gfx->setTextColor(ink);
  drawGlyphCenteredText(recordText, 2, BALL_RECORD_Y);
  for (int i = 0; i < BALL_LIVES; i++) {
    if (i < BALL_LIVES - gameMisses) gfx->fillCircle(LIFE_X0 + i * LIFE_DX, LIFE_Y, LIFE_R, UI_BAR_BAD);
    else gfx->drawCircle(LIFE_X0 + i * LIFE_DX, LIFE_Y, LIFE_R, UI_TRACK);
  }
  #pragma endregion

  #pragma region The creature, walking toward the ball on the stage floor
  uint8_t act = (ballX > gamePetX + BALL_PET_WALK_DEAD_ZONE)   ? PMD_WALKR
                : (ballX < gamePetX - BALL_PET_WALK_DEAD_ZONE) ? PMD_WALKL
                                                               : PMD_IDLE;
  drawMinigamePet(act, (int)gamePetX, GAME_PET_Y, GAME_PET_MAX_SCALE, 100);
  #pragma endregion

  #pragma region Score (bottom, centered on CX, on the stage apron)
  char scoreLine[24];
  snprintf(scoreLine, sizeof(scoreLine), T(S_SCORE_FMT), gameScore);
  gfx->setTextColor(ink);
  drawGlyphCenteredText(scoreLine, MINIGAME_SCORE_TEXT_SIZE, MINIGAME_SCORE_Y, MINIGAME_SCORE_MAX_W);
  #pragma endregion

  #pragma region Impact ring that expands and fades (soft hit feedback)
  uint32_t sinceHitMs = millis() - hitTime;
  if (hitTime && sinceHitMs < HIT_RING_DURATION_MS) {
    int ringRadius = HIT_RING_START_RADIUS + (int)(sinceHitMs / HIT_RING_MS_PER_PIXEL);
    gfx->drawCircle((int)hitX, (int)hitY, ringRadius, HIT_RING_OUTER_COLOR);
    gfx->drawCircle((int)hitX, (int)hitY, ringRadius - 2, HIT_RING_INNER_COLOR);
  }
  #pragma endregion

  // the pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - BALL_OFF, (int)ballY - BALL_OFF, BALL_S, false);

  drawScreenCloseButton();  // quit (see gameTap)
  gfx->flush();
}
#pragma endregion

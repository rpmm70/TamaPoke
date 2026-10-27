// Training bag: mash the bag for a fixed time; trains ATK (see minigame_bag.h).

#pragma region Includes
#include "minigame_bag.h"
#include "app_state.h"        // training bag state, pet, isNightHour
#include "audio.h"            // sfxPlay
#include "board_display.h"    // gfx
#include "boards/layout.h"    // CX, SX()/SY()/SK()/SKM()
#include "colors.h"
#include "i18n.h"             // T()
#include "minigame_common.h"  // canStartMinigame, drawMinigameResults
#include "pet.h"              // timeLeft
#include "scene.h"            // drawGameScene, sceneHour
#include "ui_text.h"          // ctext
#include "ui_widgets.h"       // sineOfMillis
#pragma endregion

#pragma region Constants
// length of one round
#define SACK_DURATION_MS 10000
// each hit sets the shake to this many px; it decays by SACK_SHAKE_DECAY per frame
#define SACK_HIT_SHAKE 16
#define SACK_SHAKE_DECAY 0.84f
#define SACK_SHAKE_RADIANS_PER_MS 0.05f
// layout in 466 px units (rescaled by SX/SY/SK)
#define SACK_ROPE_TOP SK(56)
#define SACK_BAG_TOP SK(86)
#define SACK_HIT_COUNT_Y SY(268)
#define SACK_HIT_COUNT_TEXT_SIZE 6
#define SACK_HINT_Y SY(322)
#define SACK_HINT_TEXT_SIZE 2
#define SACK_TIME_BAR_Y SY(350)
#define SACK_TIME_BAR_W SX(280)
#define SACK_BAG_COLOR C565(0xb5, 0x3a, 0x3a)
#define SACK_BAG_CAP_COLOR C565(0x7e, 0x28, 0x28)
#pragma endregion

#pragma region Start and touch
void startSack() {
  if (!canStartMinigame()) return;
  sackOpen = true;
  sackUntil = millis() + SACK_DURATION_MS;
  sackOver = false;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (!timeLeft(sackUntil)) return;  // time is already up
  sackHits++;
  sackShake = SACK_HIT_SHAKE;  // shakes the bag
}
#pragma endregion

#pragma region Drawing
void renderSack() {
  // read before the time check, so the time bar below never sees a deadline already past
  uint32_t now = millis();

  #pragma region Time is up: apply the training (ATK) once, then the results right away
  // (the results used to wait one frame, which flashed the habitat with no bag)
  if (!sackOver && !timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOver = true;
  }
  #pragma endregion

  #pragma region Results screen: same layout as the ball game and coin chase (OK closes it)
  if (sackOver) {
    drawMinigameResults(S_HITS_FMT, sackHits, sackNewHi && sackHits > 0, false, pet.strHi, sackGain, S_ATK_GAIN_FMT);
    return;
  }
  #pragma endregion

  drawGameScene();  // habitat background
  bool night = isNightHour(sceneHour());
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  #pragma region The bag swinging from its rope (active mashing)
  sackShake *= SACK_SHAKE_DECAY;
  int shakeOffset = (int)(sackShake * sineOfMillis(now, SACK_SHAKE_RADIANS_PER_MS));
  int bagCenterX = CX + shakeOffset, bagTop = SACK_BAG_TOP;
  gfx->fillRect(CX - SKM(6) / 2, SACK_ROPE_TOP, SKM(6), bagTop - SACK_ROPE_TOP, ink);  // hook/rope
  gfx->fillRect(bagCenterX - SKM(8) / 2, bagTop - SK(30), SKM(8), SK(34), ink);        // chain
  gfx->fillRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(150), SK(26), SACK_BAG_COLOR);      // bag
  gfx->fillRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(22), SK(18), SACK_BAG_CAP_COLOR);   // cap
  gfx->drawRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(150), SK(26), ink);
  gfx->fillRect(bagCenterX - SK(42), bagTop + SK(70), SK(84), SKM(4), SACK_BAG_CAP_COLOR);       // seam
  #pragma endregion

  #pragma region Hit counter and hint
  char hitCountText[8];
  snprintf(hitCountText, sizeof(hitCountText), "%u", sackHits);
  gfx->setTextColor(ink);
  ctext(hitCountText, SACK_HIT_COUNT_TEXT_SIZE, SACK_HIT_COUNT_Y);
  ctext(T(S_HIT_FAST), SACK_HINT_TEXT_SIZE, SACK_HINT_Y);
  #pragma endregion

  #pragma region Time bar
  uint32_t timeLeftMs = sackUntil - now;
  int barWidth = SACK_TIME_BAR_W, fillWidth = (int)((uint32_t)barWidth * timeLeftMs / SACK_DURATION_MS);
  gfx->fillRoundRect(CX - barWidth / 2, SACK_TIME_BAR_Y, barWidth, SKM(16), SKM(5), UI_TRACK);
  if (fillWidth > 2) gfx->fillRoundRect(CX - barWidth / 2, SACK_TIME_BAR_Y, fillWidth, SKM(16), SKM(5), UI_BAR_OK);
  #pragma endregion

  gfx->flush();
}
#pragma endregion

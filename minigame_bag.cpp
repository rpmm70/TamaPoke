// Training bag: mash the bag for a fixed time; trains ATK (see minigame_bag.h).

#pragma region Includes
#include "minigame_bag.h"
#include "app_state.h"        // training bag state, pet
#include "audio.h"            // sfxPlay
#include "board_display.h"    // gfx
#include "boards/layout.h"    // CX, SK()/SKM(), SACK_* layout, MINIGAME_SCORE_TEXT_SIZE / _MAX_W
#include "colors.h"
#include "i18n.h"             // T()
#include "minigame_common.h"  // canStartMinigame, drawMinigameResults
#include "pet.h"              // timeLeft
#include "scene.h"            // drawBoxingRingInterior
#include "ui_text.h"          // ctext
#include "ui_widgets.h"       // sineOfMillis, drawScreenCloseButton
#pragma endregion

#pragma region Constants
// length of one round
#define SACK_DURATION_MS 10000
// the timer turns red with this much time left
#define SACK_LOW_TIME_WARNING_MS 3000
// each hit sets the shake to this many px; it decays by SACK_SHAKE_DECAY per frame
#define SACK_HIT_SHAKE 16
#define SACK_SHAKE_DECAY 0.84f
#define SACK_SHAKE_RADIANS_PER_MS 0.05f
// the bag hangs this far (466 px units, rescaled by SK) below the hook
#define SACK_BAG_TOP (SACK_ROPE_TOP + SK(30))
#define SACK_HINT_TEXT_SIZE 2
#define SACK_BAG_COLOR C565(0xb5, 0x3a, 0x3a)
#define SACK_BAG_CAP_COLOR C565(0x7e, 0x28, 0x28)
// hook and chain: light metal against the dark arena
#define SACK_CHAIN_COLOR C565(0x9a, 0x9e, 0xa8)
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
  // read before the time check, so the timer below never sees a deadline already past
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

  drawBoxingRingInterior();  // same at any time of day
  const uint16_t ink = UI_INK;  // the bag outline and the text on the light canvas

  #pragma region Timer (top middle, over the dark arena): whole seconds, rounded up
  uint32_t timeLeftMs = sackUntil - now;
  char timerText[8];
  snprintf(timerText, sizeof(timerText), "%lu", (unsigned long)((timeLeftMs + 999) / 1000));
  gfx->setTextColor(timeLeftMs <= SACK_LOW_TIME_WARNING_MS ? UI_BAR_BAD : UI_INK_NIGHT);
  ctext(timerText, SACK_TIMER_TEXT_SIZE, SACK_TIMER_Y);
  #pragma endregion

  #pragma region The bag swinging from its rope (active mashing)
  sackShake *= SACK_SHAKE_DECAY;
  int shakeOffset = (int)(sackShake * sineOfMillis(now, SACK_SHAKE_RADIANS_PER_MS));
  int bagCenterX = CX + shakeOffset, bagTop = SACK_BAG_TOP;
  // the hanging line starts just under the timer digits (8 px font rows per text size step)
  int lineTop = SACK_TIMER_Y + 8 * TS(SACK_TIMER_TEXT_SIZE) + SKM(4);
  gfx->fillRect(bagCenterX - SKM(8) / 2, lineTop, SKM(8), bagTop + SK(4) - lineTop, SACK_CHAIN_COLOR);  // chain, swings with the bag
  gfx->fillRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(150), SK(26), SACK_BAG_COLOR);      // bag
  gfx->fillRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(22), SK(18), SACK_BAG_CAP_COLOR);   // cap
  gfx->drawRoundRect(bagCenterX - SK(42), bagTop, SK(84), SK(150), SK(26), ink);
  gfx->fillRect(bagCenterX - SK(42), bagTop + SK(70), SK(84), SKM(4), SACK_BAG_CAP_COLOR);       // seam
  #pragma endregion

  #pragma region Hint and hit count (bottom, centered on CX, on the canvas)
  gfx->setTextColor(ink);
  ctext(T(S_HIT_FAST), SACK_HINT_TEXT_SIZE, SACK_HINT_Y);
  char scoreLine[24];
  snprintf(scoreLine, sizeof(scoreLine), T(S_HITS_FMT), sackHits);
  ctext(scoreLine, MINIGAME_SCORE_TEXT_SIZE, SACK_SCORE_Y, MINIGAME_SCORE_MAX_W);
  #pragma endregion

  drawScreenCloseButton();  // quit (see handleTouch)
  gfx->flush();
}
#pragma endregion

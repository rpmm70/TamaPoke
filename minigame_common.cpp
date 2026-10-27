// Shared by the three minigames: who may start one, the creature drawn in the
// game and the results screen (see minigame_common.h).

#pragma region Includes
#include "minigame_common.h"
#include "app_state.h"      // pet, mon, pmd, lastSpriteDrawnBounds, isNightHour
#include "board_display.h"  // gfx
#include "boards/layout.h"  // CX, BATTLE_RESULT_*, BATTLE_OK_*, BATTLE_GROUND_Y, BATTLE_SPRITE_MAX_S
#include "colors.h"
#include "i18n.h"           // T()
#include "scene.h"          // drawGameScene, sceneHour
#include "sprite_draw.h"    // drawPmdActResizedM, scaledPixelEdge
#include "ui_widgets.h"     // drawBorderedBox, drawLabeledButton, drawResultLine, isTouchInsideRect
#pragma endregion

#pragma region Constants
// the B/W sprite is at most this wide (SdMon::load rejects wider files)
#define MINIGAME_BW_SPRITE_MAX_WIDTH 256
// joy line: from this score on the results say "great joy" instead of "+joy"
#define MINIGAME_GREAT_JOY_SCORE 10
#pragma endregion

#pragma region Start guard
bool canStartMinigame() {
  return !(pet.isEgg() || pet.sleeping || pet.ceremony);
}
#pragma endregion

#pragma region The creature in a minigame
void drawMinigamePet(uint8_t act, int centerX, int feetY, uint8_t maxScale, uint16_t sizePercent) {
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActResizedM(pmd, act, centerX, feetY, millis(), true, false, maxScale, sizePercent);
  } else if (mon.loaded) {
    #pragma region Frame, size and position of the B/W sprite
    uint16_t scalePercent = ((mon.h * 2 > 130) ? 1 : 2) * sizePercent;
    int scaledWidth = mon.w * scalePercent / 100, scaledHeight = mon.h * scalePercent / 100;
    uint16_t frameDurationMs = mon.frameMs ? mon.frameMs : 100;
    uint16_t frameIndex = (millis() / frameDurationMs) % mon.frames;
    const uint8_t *framePixels = mon.data + (uint32_t)frameIndex * mon.w * mon.h;
    int spriteLeft = centerX - scaledWidth / 2, spriteTop = feetY - scaledHeight;
    #pragma endregion

    #pragma region Column edges computed once (the same values scaledPixelEdge gives per pixel)
    int16_t columnEdges[MINIGAME_BW_SPRITE_MAX_WIDTH + 1];
    int columnCount = mon.w <= MINIGAME_BW_SPRITE_MAX_WIDTH ? mon.w : MINIGAME_BW_SPRITE_MAX_WIDTH;
    for (int column = 0; column <= columnCount; column++)
      columnEdges[column] = (int16_t)scaledPixelEdge(spriteLeft, column, scalePercent);
    #pragma endregion

    #pragma region Pixels, tracking the opaque box in source pixels
    int opaqueColumnMin = mon.w, opaqueColumnMax = -1, opaqueRowMin = mon.h, opaqueRowMax = -1;
    for (int row = 0; row < mon.h; row++) {
      int rowTop = scaledPixelEdge(spriteTop, row, scalePercent);
      int rowHeight = scaledPixelEdge(spriteTop, row + 1, scalePercent) - rowTop;
      if (rowHeight <= 0) continue;
      const uint8_t *rowPixels = framePixels + row * mon.w;
      for (int column = 0; column < columnCount; column++) {
        uint8_t paletteIndex = rowPixels[column];
        if (paletteIndex == 0xFF) continue;
        int columnLeft = columnEdges[column];
        int columnWidth = columnEdges[column + 1] - columnLeft;
        if (columnWidth > 0) gfx->fillRect(columnLeft, rowTop, columnWidth, rowHeight, mon.pal[paletteIndex]);
        if (column < opaqueColumnMin) opaqueColumnMin = column;
        if (column > opaqueColumnMax) opaqueColumnMax = column;
        if (row < opaqueRowMin) opaqueRowMin = row;
        opaqueRowMax = row;
      }
    }
    #pragma endregion

    #pragma region Remember the opaque pixel box on screen (coin chase pickup)
    lastSpriteDrawnBounds.valid = false;
    if (opaqueColumnMax >= 0) {
      lastSpriteDrawnBounds = { (int16_t)scaledPixelEdge(spriteLeft, opaqueColumnMin, scalePercent),
                                (int16_t)scaledPixelEdge(spriteTop, opaqueRowMin, scalePercent),
                                (int16_t)(scaledPixelEdge(spriteLeft, opaqueColumnMax + 1, scalePercent) - 1),
                                (int16_t)(scaledPixelEdge(spriteTop, opaqueRowMax + 1, scalePercent) - 1), true };
    }
    #pragma endregion
  }
}
#pragma endregion

#pragma region Minigame results screen (ball game, coin chase and training bag)
// Same look as the wild battle result: the creature on the habitat, a big
// colored title, two detail lines, a gold reward line and an OK button.
// Reuses the BATTLE_RESULT_* / BATTLE_OK_* layout so both screens stay aligned.
bool isMinigameOkTapped(int16_t x, int16_t y) {
  return isTouchInsideRect(x, y, BATTLE_OK_X, BATTLE_RESULT_BTN_Y - BATTLE_RESULT_HIT_PAD_Y, BATTLE_OK_W,
                           BATTLE_RESULT_BTN_H + 2 * BATTLE_RESULT_HIT_PAD_Y);
}

// title: the score in scoreFormat ("SCORE %u", "%u HITS"), green for a new record or a cleared coin chase;
// lines: record (or NEW RECORD!), joy; reward: rewardFormat ("SPD +%u", "ATK +%u") when the stat went up
void drawMinigameResults(StrId scoreFormat, uint16_t score, bool newRecord, bool cleared, uint16_t record, uint8_t statGain,
                         StrId rewardFormat) {
  drawGameScene();
  bool night = isNightHour(sceneHour());
  uint16_t resultInk = night ? UI_INK_NIGHT : UI_INK;

  drawMinigamePet(PMD_IDLE, CX, BATTLE_GROUND_Y, BATTLE_SPRITE_MAX_S, 100);

  #pragma region Panel (rect board only, like the battle result)
  if (BATTLE_RESULT_PANEL_W > 0) {
    drawBorderedBox(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                    BATTLE_RESULT_PANEL_R, UI_WHITE, UI_INK);
    resultInk = UI_INK;
  }
  #pragma endregion

  #pragma region Title, record line, joy line, reward line
  char scoreText[22];
  snprintf(scoreText, sizeof(scoreText), T(scoreFormat), score);  // buffer-check: S_SCORE_FMT S_HITS_FMT
  gfx->setTextColor(newRecord || cleared ? UI_BAR_OK : resultInk);
  drawResultLine(scoreText, BATTLE_RESULT_TITLE_SIZE, BATTLE_RESULT_TITLE_Y);

  if (newRecord) {
    gfx->setTextColor(UI_BAR_WARN);
    drawResultLine(T(S_NEW_RECORD), BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
  } else {
    char recordText[24];
    snprintf(recordText, sizeof(recordText), T(S_RECORD_FMT), record);
    gfx->setTextColor(resultInk);
    drawResultLine(recordText, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
  }
  gfx->setTextColor(resultInk);
  drawResultLine(score >= MINIGAME_GREAT_JOY_SCORE ? T(S_GREAT_JOY) : T(S_PLUS_JOY), BATTLE_RESULT_LINE_SIZE,
                 BATTLE_RESULT_DAMAGE_Y);

  if (statGain) {
    char rewardText[24];
    snprintf(rewardText, sizeof(rewardText), T(rewardFormat), statGain);  // buffer-check: S_SPD_GAIN_FMT S_ATK_GAIN_FMT
    gfx->setTextColor(UI_BAR_WARN);
    drawResultLine(rewardText, BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
  }
  #pragma endregion

  #pragma region OK button
  drawLabeledButton(BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK,
                    UI_BG_DAY, T(S_OK), BATTLE_OK_TEXT_SIZE);
  #pragma endregion

  gfx->flush();
}
#pragma endregion

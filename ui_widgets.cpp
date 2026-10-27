// Shared drawing and hit-test helpers (see ui_widgets.h).

#pragma region Includes
#include "ui_widgets.h"
#include <math.h>
#include "app_state.h"      // gCjkFont, gTextSize
#include "board_display.h"  // gfx
#include "boards/layout.h"  // CX, CLOSE_BTN_*, CARD_ARROW_*, CARD_PROGRESS_TITLE_*, BATTLE_*, GALLERY_TYPE_CHIP_*
#include "colors.h"
#include "countof.h"
#include "i18n.h"           // T(), typeName()
#include "ui_text.h"
#pragma endregion

#pragma region Animation helper
float millisPhaseRadians(uint32_t nowMs, float radiansPerMs) {
  const double fullTurnRadians = 2.0 * M_PI;
  return (float)fmod((double)nowMs * (double)radiansPerMs, fullTurnRadians);
}

float sineOfMillis(uint32_t nowMs, float radiansPerMs) {
  return sinf(millisPhaseRadians(nowMs, radiansPerMs));
}
#pragma endregion

#pragma region Bars
void drawProgressBar(int left, int top, int width, int height, int radius, int value, int maxValue, uint16_t fillColor) {
  if (maxValue <= 0) maxValue = 1;
  int fillWidth = (int)((uint32_t)value * (uint32_t)width / (uint32_t)maxValue);
  if (fillWidth > width) fillWidth = width;
  gfx->fillRoundRect(left, top, width, height, radius, UI_TRACK);
  if (fillWidth > 2) gfx->fillRoundRect(left, top, fillWidth, height, radius, fillColor);
}

void drawInsetProgressBar(int left, int top, int width, int height, int trackRadius, int fillRadius, int value, int maxValue,
                          uint16_t fillColor) {
  gfx->fillRoundRect(left, top, width, height, trackRadius, UI_TRACK);
  int fillWidth = (width - 4) * value / maxValue;
  if (fillWidth > 0) gfx->fillRoundRect(left + 2, top + 2, fillWidth, height - 4, fillRadius, fillColor);
}
#pragma endregion

#pragma region Boxes and buttons
void drawBorderedBox(int left, int top, int width, int height, int radius, uint16_t fillColor, uint16_t borderColor) {
  gfx->fillRoundRect(left, top, width, height, radius, fillColor);
  gfx->drawRoundRect(left, top, width, height, radius, borderColor);
}

void drawLabeledButton(int left, int top, int width, int height, int radius, uint16_t fillColor, uint16_t textColor,
                       const char *label, int preferredTextSize) {
  gfx->fillRoundRect(left, top, width, height, radius, fillColor);
  gfx->setTextColor(textColor);
  drawTextCenteredInBox(label, preferredTextSize, left, top, width, height);
}

void drawOnOffPill(int left, int top, int width, int height, int radius, bool isOn, int preferredTextSize) {
  drawBorderedBox(left, top, width, height, radius, isOn ? UI_BAR_OK : UI_WHITE, UI_INK);
  gfx->setTextColor(isOn ? UI_BG_DAY : UI_INK);
  drawTextCenteredInBox(isOn ? T(S_ON) : T(S_OFF), preferredTextSize, left, top, width, height);
}

void drawPulsingCtaButton(const PulsingCtaButtonStyle &style, const char *label, uint32_t nowMs) {
  #pragma region Pulsing box and outline
  int pulse = (int)(style.pulseAmplitude * sineOfMillis(nowMs, style.pulseRadiansPerMs));
  int left = style.left - pulse, top = style.top - pulse;
  int width = style.width + 2 * pulse, height = style.height + 2 * pulse;
  drawBorderedBox(left, top, width, height, style.radius, style.fillColor, style.borderColor);
  if (style.hasInnerBorder) gfx->drawRoundRect(left + 2, top + 2, width - 4, height - 4, style.radius - 2, style.borderColor);
  #pragma endregion

  #pragma region Label centered on CX and on the box middle
  gfx->setTextColor(style.textColor);
  int labelSize = fitSize(label, style.preferredTextSize, style.width - 16);
  setSize(labelSize);
  // the label sits half its height above the middle; the evolve button nudges size 3 one px lower
  int labelLift = (style.size3LabelOnePxLower && labelSize == 3) ? 11 : 4 * labelSize;
  setCur(centerX(label, labelSize), top + height / 2 - labelLift);
  printT(label);
  #pragma endregion
}
#pragma endregion

#pragma region Close button and page navigation
void drawScreenCloseButton() {
  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);
}

bool isTouchOnScreenCloseButton(int16_t touchX, int16_t touchY) {
  return isTouchInsideCircle(touchX, touchY, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R);
}

void drawPageNavigation(bool showLeftArrow, bool showRightArrow) {
  drawScreenCloseButton();
  if (showLeftArrow) drawPageArrow(gfx, CARD_ARROW_LEFT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, -1, UI_INK);
  if (showRightArrow) drawPageArrow(gfx, CARD_ARROW_RIGHT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, 1, UI_INK);
}

bool pageNavigationHit(int16_t touchX, int16_t touchY, bool leftArrowVisible, bool rightArrowVisible, int &step, bool &closeHit) {
  step = 0;
  closeHit = isTouchOnScreenCloseButton(touchX, touchY);
  if (closeHit) return true;
  const int16_t arrowHitTop = CARD_ARROW_CY - CARD_ARROW_HIT_H / 2;
  if (leftArrowVisible && isTouchInsideRect(touchX, touchY, 0, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    step = -1;
    return true;
  }
  if (rightArrowVisible &&
      isTouchInsideRect(touchX, touchY, LCD_WIDTH - CARD_ARROW_HIT_W, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    step = 1;
    return true;
  }
  return false;
}

int pageCountFor(int itemCount, int perPage) {
  return itemCount == 0 ? 1 : (itemCount + perPage - 1) / perPage;
}
#pragma endregion

#pragma region Text lines
void drawTextCenteredOnCx(const char *text, int textSize, int top) {
  setSize(textSize);
  setCur(centerX(text, textSize), top);
  printT(text);
}

void drawScreenTitle(const char *title) {
  int titleSize = shrinkTextSizeToWidth(title, TS(5), CARD_PROGRESS_TITLE_MAXW);
  setCur(centerX(title, titleSize), CARD_PROGRESS_TITLE_Y);
  printT(title);
}

void drawResultLine(const char *text, int preferredSize, int top) {
  drawTextCenteredInBox(text, preferredSize, CX - BATTLE_RESULT_TEXT_MAX_W / 2, top, BATTLE_RESULT_TEXT_MAX_W,
                        FONT_H * preferredSize);
}
#pragma endregion

#pragma region Type chips
const TypeChipStyle BATTLE_TYPE_CHIP_STYLE = { 1, BATTLE_CHIP_H, BATTLE_CHIP_PAD, BATTLE_CHIP_R, BATTLE_CHIP_GAP };
const TypeChipStyle GALLERY_TYPE_CHIP_STYLE = { GALLERY_TYPE_CHIP_TEXT_SIZE, GALLERY_TYPE_CHIP_H,
                                                GALLERY_TYPE_CHIP_PAD, GALLERY_TYPE_CHIP_R, GALLERY_TYPE_CHIP_GAP };

// indexed by TYPE_* (dex.h)
static const uint16_t TYPE_CHIP_COLORS[] = {
  0x8C4D,  // TYPE_NONE
  0x8C4D,  // TYPE_NORMAL
  0xEA87,  // TYPE_FIRE
  WATER_BLUE_COLOR,  // TYPE_WATER
  0xBCA1,  // TYPE_ELECTRIC
  0x3C49,  // TYPE_GRASS
  0x5D99,  // TYPE_ICE
  0xA2A5,  // TYPE_FIGHTING
  0x8A73,  // TYPE_POISON
  0xB447,  // TYPE_GROUND
  0x8D7F,  // TYPE_FLYING
  0xD28F,  // TYPE_PSYCHIC
  0x7CC4,  // TYPE_BUG
  0x9407,  // TYPE_ROCK
  0x6B33,  // TYPE_GHOST
  0x5A5F,  // TYPE_DRAGON
  0x5ACB,  // TYPE_DARK
  0xA534,  // TYPE_STEEL
  0xF3B7,  // TYPE_FAIRY
};
static_assert(countof(TYPE_CHIP_COLORS) == TYPE_COUNT, "one chip color per battle type");
static const uint16_t UNKNOWN_TYPE_CHIP_COLOR = 0x8C4D;

uint16_t typeChipColor(uint8_t type) {
  return type < countof(TYPE_CHIP_COLORS) ? TYPE_CHIP_COLORS[type] : UNKNOWN_TYPE_CHIP_COLOR;
}

int typeChipWidth(uint8_t type, const TypeChipStyle &style) {
  setSize(style.textSize);  // textW() with a CJK font measures with the active size
  return (int)textW(typeName(type), style.textSize) + 2 * style.padding;
}

int typeChipsTotalWidth(const DexEntry &entry, const TypeChipStyle &style) {
  int firstWidth = typeChipWidth(entry.type1, style);
  if (entry.type2 == TYPE_NONE) return firstWidth;
  return firstWidth + style.gap + typeChipWidth(entry.type2, style);
}

void drawTypeChip(int x, int y, uint8_t type, const TypeChipStyle &style) {
  if (type == TYPE_NONE) return;
  int chipWidth = typeChipWidth(type, style);
  drawBorderedBox(x, y, chipWidth, style.height, style.radius, lerp565(typeChipColor(type), UI_WHITE, 5, 8), UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(style.textSize);
  // glyph height actually drawn: 8 px per scale step in the 5x7 font, 16 px in the CJK fonts
  int textHeight = (gCjkFont ? 16 : FONT_H) * gTextSize;
  setCur(x + style.padding, y + (style.height - textHeight) / 2);
  printT(typeName(type));
}

int drawTypeChips(int x, int y, const DexEntry &entry, bool alignRight, const TypeChipStyle &style) {
  int firstWidth = typeChipWidth(entry.type1, style);
  int totalWidth = typeChipsTotalWidth(entry, style);
  int chipsLeft = alignRight ? x - totalWidth : x;
  drawTypeChip(chipsLeft, y, entry.type1, style);
  if (entry.type2 != TYPE_NONE) drawTypeChip(chipsLeft + firstWidth + style.gap, y, entry.type2, style);
  return chipsLeft;
}
#pragma endregion

#pragma region Hit tests
bool isTouchInsidePaddedRect(int16_t touchX, int16_t touchY, int left, int top, int width, int height, int padX, int padY) {
  return isTouchInsideRect(touchX, touchY, left - padX, top - padY, width + 2 * padX, height + 2 * padY);
}
#pragma endregion

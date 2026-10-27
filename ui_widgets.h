#pragma once

// Shared drawing and hit-test helpers that replace code the screens used to
// duplicate. Each one draws EXACTLY the pixels of the code it replaced; which
// sites each helper replaces is listed in phaseB_function_map.md.

#pragma region Includes
#include <Arduino.h>
#include "dex.h"       // DexEntry, TYPE_*
#include "ui_icons.h"  // isTouchInsideRect, isTouchInsideCircle, drawCloseButton, drawPageArrow
#pragma endregion

#pragma region Animation helper
// nowMs * radiansPerMs reduced modulo 2*pi in double precision, in [0, 2*pi):
// a large millis() times a rate loses precision in float, so the phase is
// reduced first (add per-item offsets to the result, e.g. sinf(phase + i))
float millisPhaseRadians(uint32_t nowMs, float radiansPerMs);
// sinf(nowMs * radiansPerMs) without the float precision loss (millisPhaseRadians)
float sineOfMillis(uint32_t nowMs, float radiansPerMs);
#pragma endregion

#pragma region Bars
// UI_TRACK track of the full box, then the fill drawn over it from the left,
// same height and radius, width = value * width / maxValue (clamped to width,
// maxValue <= 0 counts as 1), only when wider than 2 px.
void drawProgressBar(int left, int top, int width, int height, int radius, int value, int maxValue, uint16_t fillColor);
// UI_TRACK track of the full box, then the fill inset by 2 px on every side,
// width = (width - 4) * value / maxValue (not clamped), only when wider than 0 px.
void drawInsetProgressBar(int left, int top, int width, int height, int trackRadius, int fillRadius, int value, int maxValue,
                          uint16_t fillColor);
#pragma endregion

#pragma region Boxes and buttons
// fillRoundRect in fillColor, then the outline in borderColor
void drawBorderedBox(int left, int top, int width, int height, int radius, uint16_t fillColor, uint16_t borderColor);
// fillRoundRect in fillColor, then the label in textColor centered in the box
// (drawTextCenteredInBox: shrinks to fit). No outline.
void drawLabeledButton(int left, int top, int width, int height, int radius, uint16_t fillColor, uint16_t textColor,
                       const char *label, int preferredTextSize);
// On/off pill: green with light text when on, white with ink text when off,
// ink outline, label T(S_ON) / T(S_OFF) centered
void drawOnOffPill(int left, int top, int width, int height, int radius, bool isOn, int preferredTextSize);

// Main screen call-to-action button that pulses (grows and shrinks by up to
// pulseAmplitude px per side). The label is fitted to the base width - 16 with
// fitSize() and centered on CX.
struct PulsingCtaButtonStyle {
  int16_t left, top, width, height, radius;
  float pulseAmplitude;     // px per side at the peak
  float pulseRadiansPerMs;  // sine rate
  uint16_t fillColor, borderColor, textColor;
  bool hasInnerBorder;      // second outline 2 px inside, radius - 2, in borderColor
  uint8_t preferredTextSize;  // already through TS()
  bool size3LabelOnePxLower;  // evolve button: a size-3 label sits 11 px above the middle, not 12
};
void drawPulsingCtaButton(const PulsingCtaButtonStyle &style, const char *label, uint32_t nowMs);
#pragma endregion

#pragma region Close button and page navigation (info card, gallery, settings, minigames)
// Round X at CLOSE_BTN_CX/CY, radius CLOSE_BTN_R, ink on the day background
void drawScreenCloseButton();
// Finger-sized test of the close button (CLOSE_BTN_HIT_R)
bool isTouchOnScreenCloseButton(int16_t touchX, int16_t touchY);
// The close X plus the left / right page arrows (hidden arrows are not drawn)
void drawPageNavigation(bool showLeftArrow, bool showRightArrow);
// true when the tap hit the X (closeHit = true, checked first with CLOSE_BTN_HIT_R)
// or a visible arrow (step = -1 left / +1 right; arrow hit boxes CARD_ARROW_HIT_W x
// CARD_ARROW_HIT_H at the screen edges)
bool pageNavigationHit(int16_t touchX, int16_t touchY, bool leftArrowVisible, bool rightArrowVisible, int &step, bool &closeHit);
// Pages of perPage items; an empty list still shows one (empty) page
int pageCountFor(int itemCount, int perPage);
#pragma endregion

#pragma region Text lines
// setSize(textSize), cursor at centerX(text, textSize) and top, printT(text)
void drawTextCenteredOnCx(const char *text, int textSize, int top);
// Large page title centered on CX at CARD_PROGRESS_TITLE_Y, shrunk to fit
// CARD_PROGRESS_TITLE_MAXW: every settings screen and the info card pages share it
void drawScreenTitle(const char *title);
// One line centered on CX in the battle / minigame result column
// (BATTLE_RESULT_TEXT_MAX_W wide, FONT_H * preferredSize tall)
void drawResultLine(const char *text, int preferredSize, int top);
#pragma endregion

#pragma region Type chips (battle screen, Pokedex detail and info card share the drawing, each with its own size)
struct TypeChipStyle {
  uint8_t textSize;
  int16_t height, padding, radius, gap;
};
extern const TypeChipStyle BATTLE_TYPE_CHIP_STYLE;
extern const TypeChipStyle GALLERY_TYPE_CHIP_STYLE;
// chip color per battle type (the fork's palette; grey for TYPE_NONE / unknown)
uint16_t typeChipColor(uint8_t type);
int typeChipWidth(uint8_t type, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE);
// width of one or two chips side by side
int typeChipsTotalWidth(const DexEntry &entry, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE);
void drawTypeChip(int x, int y, uint8_t type, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE);
// one or two chips starting at x (or ending at x when alignRight); returns the left edge
int drawTypeChips(int x, int y, const DexEntry &entry, bool alignRight, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE);
#pragma endregion

#pragma region Hit tests
// a drawn box grown by padX / padY on each side (generous finger hit areas)
bool isTouchInsidePaddedRect(int16_t touchX, int16_t touchY, int left, int top, int width, int height, int padX, int padY);
#pragma endregion

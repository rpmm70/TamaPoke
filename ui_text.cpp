// Text: size and cursor, font per language, measuring and centering (see ui_text.h).
//
// The classic font anchors the cursor at the TOP CORNER of the text, and every
// Y coordinate in the code is written that way. U8g2 fonts anchor it at the
// BASELINE (drawChar does curY = y - baseline*size), so switching fonts would
// shift all the text up. setCur() compensates for that difference in a single
// place, so Y keeps meaning "top" with any font.
//
// setSize() exists only because the library keeps textsize_x protected and
// setCur() needs to know the active scale to multiply the ascent.
//
// With the classic font gFontAscent is 0, so setCur(x,y) is exactly
// setCursor(x,y): this does not move a pixel in the Latin-script languages.

#pragma region Includes
#include "ui_text.h"
#include <U8g2lib.h>          // CJK fonts (Japanese, Korean)
#include "app_state.h"        // gCjkFont, gTextSize, gFontAscent
#include "board_display.h"    // gfx
#include "boards/layout.h"    // CX, TS(), LCD_WIDTH, SETTINGS_TEXT_SIDE_PAD
#include "i18n.h"             // gLang, LANG_IS_CJK
#pragma endregion

#pragma region CJK font tuning
// The classic font is 8 px tall at scale 1. If the CJK one is taller, the scale
// has to be divided so the sizes match... but dividing THINS the stroke: at
// scale 1 unifont draws 1 px lines while Latin at scale 2 draws them 2 px wide,
// and Japanese looks faint even though it is the same size.
//
// That is why a small AND BOLD (_b_) base font is preferable, allowing the SAME
// scale as Latin: that way the stroke weight matches and no division is needed.
//
// A truly bold font (b10_b) was tried and does NOT work at this size: the
// strokes merge and the kana lose detail, illegible even to a native reader.
// It goes back to unifont, which is the most legible, and the thin stroke is
// compensated in printT() by redrawing 1 px offset (pseudo-bold).
// japanese3 and not japanese1: subsets 1 and 2 include kana and kanji but NO
// CJK punctuation, and Arduino_GFX draws nothing when a glyph is missing (nor
// advances the cursor), so the 25 full-width ! and ? marks in the strings disappeared.
// It is the same unifont family: identical metrics header and a strict
// superset, checked glyph by glyph over the 226 codepoints the firmware
// uses. It costs 102 KB more flash and does not move a pixel.
// Korean needs a font with hangul, and CJK_FONT was a single #define.
// Instead of assuming one subset works for both scripts, the font is chosen
// per language. korean2 and not korean1: the Korean here uses 296 distinct
// syllables and all of them are in KS X 1001, the 2350-syllable set that is
// the measured size for korean2; korean1 has 478. The check looks at
// membership in KS X 1001, it does not walk the font's table, so it relies on
// korean2's set being exactly that; ASCII is assumed present.
// Not verified on the board: whether CJK_SIZE_DIV 2 suits korean2 as well as
// the Japanese one. The ascent, however, is measured at runtime in applyLangFont().
#define CJK_FONT_JA u8g2_font_unifont_t_japanese3
#define CJK_FONT_KO u8g2_font_unifont_t_korean2
#define CJK_SIZE_DIV 2
#pragma endregion

#pragma region Size, cursor and font
void setSize(uint8_t requestedSize) {
  // gTextSize stores the scale ACTUALLY applied, not the requested one: setCur()
  // multiplies the ascent by it and it has to match what is drawn.
  gTextSize = gCjkFont ? (requestedSize >= CJK_SIZE_DIV ? requestedSize / CJK_SIZE_DIV : 1) : requestedSize;
  gfx->setTextSize(gTextSize);
}

void setCur(int x, int y) {
  gfx->setCursor(x, y + gFontAscent * gTextSize);
}

// The ascent is measured instead of hard-coded: getTextBounds() returns y1 as
// the offset of the top edge relative to the cursor, negative with U8g2 fonts
// (which anchor at the baseline). That way setCur() can keep treating Y as
// "top" whatever the font, with no per-font magic constants.
void applyLangFont() {
  gCjkFont = LANG_IS_CJK(gLang);
  if (!gCjkFont) {
    gfx->setFont();            // classic 5x7, CP437: Y is already the top edge
    gfx->setUTF8Print(false);
    gFontAscent = 0;
    return;
  }
  gfx->setFont((gLang == LANG_KO) ? CJK_FONT_KO : CJK_FONT_JA);
  gfx->setUTF8Print(true);     // CJK strings are multibyte UTF-8
  int16_t boundsLeft, boundsTop;
  uint16_t boundsWidth, boundsHeight;
  uint8_t previousTextSize = gTextSize;
  gfx->setTextSize(1);
  gfx->getTextBounds("A", 0, 0, &boundsLeft, &boundsTop, &boundsWidth, &boundsHeight);
  gFontAscent = -boundsTop;    // negative y1: go up from the baseline
  gfx->setTextSize(previousTextSize);
}
#pragma endregion

#pragma region Measuring and fitting
// Arduino_GFX's classic font advances FONT_W px per character at size 1. That
// only works for one byte per character and fixed width: with a U8g2 font,
// strlen() does not count characters (UTF-8 is multibyte) and they are not all
// the same width. For the classic font it returns EXACTLY strlen * 6 * size.
uint16_t textW(const char *text, uint8_t size) {
  if (!gCjkFont) return (uint16_t)strlen(text) * FONT_W * size;
  int16_t boundsLeft, boundsTop;
  uint16_t boundsWidth, boundsHeight;
  gfx->getTextBounds(text, 0, 0, &boundsLeft, &boundsTop, &boundsWidth, &boundsHeight);
  return boundsWidth;
}

int centerX(const char *text, uint8_t size) { return CX - textW(text, size) / 2; }

// fitSize() that also reports the width it measured at the returned size, so
// the centering callers do not measure the same text again (CJK measuring walks
// the glyphs). measuredWidth is -1 when nothing was measured at that size.
static int fitSizeMeasuringWidth(const char *text, int requestedSize, int maxWidth, int *measuredWidth) {
  *measuredWidth = -1;
#if BOARD_IS_ROUND
  (void)text; (void)maxWidth;
  return requestedSize;
#else
  if (maxWidth <= 0) maxWidth = LCD_WIDTH - 8;
  for (; requestedSize > 1; requestedSize--) {
    setSize(requestedSize);
    int width = textW(text, requestedSize);
    if (width <= maxWidth) {
      *measuredWidth = width;
      break;
    }
  }
  return requestedSize;
#endif
}

int fitSize(const char *text, int requestedSize, int maxWidth) {
  int measuredWidth;
  return fitSizeMeasuringWidth(text, requestedSize, maxWidth, &measuredWidth);
}

int shrinkTextSizeToWidth(const char *text, int preferredSize, int maxWidth) {
  int textSize = preferredSize;
  for (; textSize > 1; textSize--) {
    setSize(textSize);
    if (textW(text, textSize) <= maxWidth) break;
  }
  setSize(textSize);
  return textSize;
}
#pragma endregion

#pragma region Drawing
// With CJK it redraws the text offset by 1 px: unifont at a reduced scale
// leaves 1 px strokes and looks faint, and a truly bold font at this size
// merges the strokes and ruins the kana. Redrawing thickens the stroke without
// distorting the glyph. Tested on the board by usakomint, who compared both
// options in Japanese. The keyboard is ASCII-only (on purpose), so a single
// character is never CJK and needs no thickening.
void printT(char character) { gfx->print(character); }

void printT(const char *text) {
  if (gCjkFont) {
    int16_t cursorX = gfx->getCursorX(), cursorY = gfx->getCursorY();
    gfx->print(text);
    gfx->setCursor(cursorX + 1, cursorY);
  }
  gfx->print(text);
}

void ctext(const char *text, int requestedSize, int y, int maxWidth) {
  int measuredWidth;
  int size = fitSizeMeasuringWidth(text, TS(requestedSize), maxWidth, &measuredWidth);
  setSize(size);
  int left = measuredWidth >= 0 ? CX - measuredWidth / 2 : centerX(text, size);
  setCur(left, y);
  printT(text);
}

void drawGlyphCenteredText(const char *text, int requestedSize, int y, int maxWidth) {
  int measuredWidth;
  int size = fitSizeMeasuringWidth(text, TS(requestedSize), maxWidth, &measuredWidth);
  setSize(size);
  int trailingColumn = gCjkFont ? 0 : gTextSize;
  int left = measuredWidth >= 0 ? CX - measuredWidth / 2 : centerX(text, size);
  setCur(left + trailingColumn / 2, y);
  printT(text);
}

void drawTextCenteredInBox(const char *text, int preferredSize, int boxLeft, int boxTop, int boxWidth, int boxHeight) {
  int textSize = shrinkTextSizeToWidth(text, preferredSize, boxWidth - 2 * SETTINGS_TEXT_SIDE_PAD);
  setCur(boxLeft + (boxWidth - (int)textW(text, textSize)) / 2, boxTop + (boxHeight - FONT_H * textSize) / 2);
  printT(text);
}
#pragma endregion

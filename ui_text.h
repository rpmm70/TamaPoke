#pragma once

// Text drawing ready for CJK fonts: size and cursor, the active language's
// font, measuring, centering and fitting. Y coordinates always mean the TOP of
// the text, whatever the font (setCur() compensates the U8g2 baseline).

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Classic font metrics
// The classic 5x7 font advances FONT_W px per character (including a blank
// trailing column) and is FONT_H px tall, both at text size 1
#define FONT_W 6
#define FONT_H 8
#pragma endregion

#pragma region Size, cursor and font
// Applies a text size (CJK fonts divide it, see gTextSize)
void setSize(uint8_t requestedSize);
// Cursor at (x, y) with y = top edge of the text
void setCur(int x, int y);
// Sets the font for the active language (classic unless CJK). The ONLY place that touches it.
void applyLangFont();
#pragma endregion

#pragma region Measuring and fitting
// Width in px of text at size (CJK: measured with the active font and size)
uint16_t textW(const char *text, uint8_t size);
// Cursor x that leaves the text centered on CX
int centerX(const char *text, uint8_t size);
// Size that fits in maxWidth. On the round board it changes nothing (returns
// requestedSize). On the rectangular one it steps the size down until it fits
// (maxWidth <= 0 means the screen width minus 8); it leaves the size it measured set.
int fitSize(const char *text, int requestedSize, int maxWidth);
// Largest size <= preferredSize at which text fits maxWidth, on BOTH boards.
// Leaves that size applied, like fitSize().
int shrinkTextSizeToWidth(const char *text, int preferredSize, int maxWidth);
#pragma endregion

#pragma region Drawing
// Prints honoring the active font (CJK: pseudo-bold by redrawing 1 px offset)
void printT(char character);
void printT(const char *text);
// Text centered on CX at top y: TS() reduces the size on the rectangular board
// and fitSize() lowers it further if it does not fit maxWidth (0 = screen width - 8)
void ctext(const char *text, int requestedSize, int y, int maxWidth = 0);
// Like ctext(), but centers the visible glyphs: the 5x7 font's blank trailing
// column would leave Latin text half a column left of CX
void drawGlyphCenteredText(const char *text, int requestedSize, int y, int maxWidth = 0);
// Prints text centered on both axes inside a box, shrunk to the box width minus
// SETTINGS_TEXT_SIDE_PAD per side. Height is taken as FONT_H px per size step:
// exact for the classic font, close enough for the CJK one.
void drawTextCenteredInBox(const char *text, int preferredSize, int boxLeft, int boxTop, int boxWidth, int boxHeight);
#pragma endregion

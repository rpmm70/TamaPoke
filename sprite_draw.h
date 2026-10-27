#pragma once

// Sprite drawing primitives shared by every screen: flash pixel maps, PMD
// multi-action sprites (anchored by the feet, integer or fractional zoom) and
// the gallery thumbnails.

#pragma region Includes
#include <Arduino.h>
#include "sdmon.h"  // PmdMon, PmdAct
#pragma endregion

#pragma region Flash pixel maps
// draws a scaled n x n pixel map (species.h SPR_*); silhouette=true paints it in SPRITE_OUTLINE_COLOR
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette);
#pragma endregion

#pragma region Indexed pixel blocks
// pixelWidth x pixelHeight palette indices (0xFF = transparent) at a whole scale,
// top-left at (left, top), same-color horizontal runs merged into one fillRect
// (pixel-identical to one fillRect per pixel); silhouette paints SPRITE_OUTLINE_COLOR
void drawIndexedPixelsAtWholeScale(const uint8_t *pixels, int pixelWidth, int pixelHeight, int left, int top, int scale,
                                   const uint16_t *palette, bool silhouette);
#pragma endregion

#pragma region PMD sprites
// total duration of an action (100 ms if its frame times add up to 0)
uint32_t pmdActTotalMs(const PmdAct &act);
// draws action actId of m anchored at its base (center-x cx, ground groundY) at the
// largest whole zoom <= maxS that fits; sil paints it as a silhouette.
// keepIdleScale: use the Idle pose's zoom even if this action's frame is taller
// (no shrinking mid-animation, e.g. the Hop pose shown while eating).
// jumpHeightPercent: how much of each frame's rise above the action's ground row is
// kept (100 = as drawn, 40 = a small hop); the sprite keeps its size either way
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                 bool keepIdleScale = false, uint8_t jumpHeightPercent = 100);
// same as drawPmdActM, then resized by sizePercent (100 = the usual whole-number zoom)
void drawPmdActResizedM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                        uint16_t sizePercent, bool keepIdleScale = false, uint8_t jumpHeightPercent = 100);
// drawPmdActM on the main creature's sprite (pmd)
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                bool keepIdleScale = false, uint8_t jumpHeightPercent = 100);
// current frame of an action, canvas top-left at (canvasLeft, canvasTop), at a fixed whole scale
void drawPmdFrameAtScale(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop, bool sil, uint8_t scale);
// scalePercent 250 = x2.5: source pixel pixelIndex covers screen coordinates from
// scaledPixelEdge(pixelIndex) up to scaledPixelEdge(pixelIndex + 1), so a fractional zoom tiles with no gaps
int scaledPixelEdge(int origin, int pixelIndex, uint16_t scalePercent);
#pragma endregion

#pragma region Gallery thumbnails (thumbs.bin entries)
// the one thumbnail drawer: centered in a square cell of side cellSize at the
// rational scale scaleNumerator / scaleDenominator (pixel boundaries rounded, so no
// gaps). allowShrink steps the numerator down (never below the denominator) until
// the thumbnail fits the cell. silhouette paints it in SPRITE_OUTLINE_COLOR.
void drawThumbnailInCell(const uint8_t *thumb, int cellLeft, int cellTop, int cellSize, int scaleNumerator,
                         int scaleDenominator, bool silhouette, bool allowShrink);
// thumbnail centered in a cell of side cell at whole scale s (the rect board
// shrinks s until it fits the cell); sil=true paints it in SPRITE_OUTLINE_COLOR
void drawThumbCell(const uint8_t *thumb, int x, int y, int s, bool sil, int cell);
// thumbnail centered in a gallery grid cell at the rational scale
// GALLERY_THUMB_SCALE_NUM/DEN (pixel boundaries rounded, so no gaps between pixels)
void drawGalleryGridThumb(const uint8_t *thumb, int cellX, int cellY, bool sil);
#pragma endregion

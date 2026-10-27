// Sprite drawing primitives shared by every screen (see sprite_draw.h).
// Every drawer merges horizontal runs of same-color pixels into one fillRect:
// the runs cover exactly the union of the per-pixel rectangles, so the output is
// pixel-identical to drawing one rectangle per source pixel, with far fewer calls.

#pragma region Includes
#include "sprite_draw.h"
#include "app_state.h"      // pmd, lastSpriteDrawnBounds
#include "board_display.h"  // gfx
#include "boards/layout.h"  // SY(), PMD_MIN_SCALE, GALLERY_CELL, GALLERY_THUMB_SCALE_NUM/DEN
#include "species.h"        // spriteColor, SPRITE_OUTLINE_COLOR
#pragma endregion

#pragma region Constants
// palette index of a transparent pixel in every indexed sprite format
static const uint8_t TRANSPARENT_PIXEL_INDEX = 0xFF;
// transparent cell in the flash pixel maps (species.h SPR_*)
static const char TRANSPARENT_MAP_CHAR = '.';
// PMD actions and thumbnails store their width in a uint8_t: at most 255 columns,
// so 256 column edges (one past the last column) always fit
static const int MAX_INDEXED_SPRITE_WIDTH = 255;
// zoom of a PMD sprite with no idle height to derive it from
static const uint8_t PMD_FALLBACK_SCALE = 5;
// the idle frame is zoomed to about this height
static const int PMD_IDLE_TARGET_HEIGHT = SY(170);
// actions whose frame is taller than this (attack) step their zoom down
static const int PMD_ACTION_MAX_HEIGHT = SY(250);
// sizePercent that keeps the whole-number zoom unchanged
static const uint16_t PMD_NATURAL_SIZE_PERCENT = 100;
// fallback duration of an action whose frame times add up to 0
static const uint32_t PMD_EMPTY_ACTION_MS = 100;
#pragma endregion

#pragma region Column edges
// screen x of every column boundary of the frame being drawn: column c covers
// [scaledColumnEdges[c], scaledColumnEdges[c + 1]). Computed once per frame.
static int16_t scaledColumnEdges[MAX_INDEXED_SPRITE_WIDTH + 1];

// origin + pixelIndex * scaleNumerator / scaleDenominator for every boundary 0..pixelCount
static void computeScaledColumnEdges(int pixelCount, int origin, int scaleNumerator, int scaleDenominator) {
  for (int pixelIndex = 0; pixelIndex <= pixelCount; pixelIndex++)
    scaledColumnEdges[pixelIndex] = (int16_t)(origin + pixelIndex * scaleNumerator / scaleDenominator);
}

static int16_t precomputedColumnEdge(int column) { return scaledColumnEdges[column]; }
#pragma endregion

#pragma region Indexed row drawing with merged runs
// Draws one row of indexed pixels (TRANSPARENT_PIXEL_INDEX skipped) between
// rowTop and rowTop + rowHeight, column c covering [columnEdge(c), columnEdge(c + 1)),
// with one fillRect per run of consecutive opaque pixels of the same color.
// Zero-width columns (fractional zoom) belong to the run but add no pixels,
// exactly like the per-pixel drawing skipped them. Returns false when the row has
// no opaque pixel; otherwise firstOpaqueColumn / lastOpaqueColumn are the
// outermost opaque columns.
template <typename ColorForPaletteIndex, typename ColumnEdge>
static bool drawIndexedRowRuns(const uint8_t *rowPixels, int pixelCount, int rowTop, int rowHeight,
                               ColorForPaletteIndex colorForPaletteIndex, ColumnEdge columnEdge, int &firstOpaqueColumn,
                               int &lastOpaqueColumn) {
  firstOpaqueColumn = -1;
  lastOpaqueColumn = -1;
  int column = 0;
  while (column < pixelCount) {
    uint8_t paletteIndex = rowPixels[column];
    if (paletteIndex == TRANSPARENT_PIXEL_INDEX) {
      column++;
      continue;
    }
    #pragma region Extend the run while the color stays the same
    uint16_t runColor = colorForPaletteIndex(paletteIndex);
    int runStartColumn = column;
    column++;
    while (column < pixelCount && rowPixels[column] != TRANSPARENT_PIXEL_INDEX &&
           colorForPaletteIndex(rowPixels[column]) == runColor)
      column++;
    #pragma endregion
    if (firstOpaqueColumn < 0) firstOpaqueColumn = runStartColumn;
    lastOpaqueColumn = column - 1;
    int runLeft = columnEdge(runStartColumn);
    int runWidth = columnEdge(column) - runLeft;
    if (runWidth > 0) gfx->fillRect(runLeft, rowTop, runWidth, rowHeight, runColor);
  }
  return firstOpaqueColumn >= 0;
}

void drawIndexedPixelsAtWholeScale(const uint8_t *pixels, int pixelWidth, int pixelHeight, int left, int top, int scale,
                                   const uint16_t *palette, bool silhouette) {
  auto colorForPaletteIndex = [palette, silhouette](uint8_t paletteIndex) -> uint16_t {
    return silhouette ? SPRITE_OUTLINE_COLOR : palette[paletteIndex];
  };
  auto wholeScaleColumnEdge = [left, scale](int column) -> int { return left + column * scale; };
  int firstOpaqueColumn, lastOpaqueColumn;
  for (int row = 0; row < pixelHeight; row++)
    drawIndexedRowRuns(pixels + row * pixelWidth, pixelWidth, top + row * scale, scale, colorForPaletteIndex,
                       wholeScaleColumnEdge, firstOpaqueColumn, lastOpaqueColumn);
}
#pragma endregion

#pragma region Flash pixel maps
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int row = 0; row < n; row++) {
    const char *rowCells = map[row];
    int column = 0;
    while (column < n) {
      char cell = rowCells[column];
      if (cell == TRANSPARENT_MAP_CHAR) {
        column++;
        continue;
      }
      #pragma region Extend the run while the color stays the same
      uint16_t runColor = silhouette ? SPRITE_OUTLINE_COLOR : spriteColor(cell);
      int runStartColumn = column;
      column++;
      while (column < n && rowCells[column] != TRANSPARENT_MAP_CHAR &&
             (silhouette ? SPRITE_OUTLINE_COLOR : spriteColor(rowCells[column])) == runColor)
        column++;
      #pragma endregion
      gfx->fillRect(x + runStartColumn * s, y + row * s, (column - runStartColumn) * s, s, runColor);
    }
  }
}
#pragma endregion

#pragma region PMD sprites
uint32_t pmdActTotalMs(const PmdAct &act) {
  uint32_t totalMs = 0;
  for (uint8_t frameIndex = 0; frameIndex < act.frames; frameIndex++) totalMs += act.ms[frameIndex];
  return totalMs ? totalMs : PMD_EMPTY_ACTION_MS;
}

// frame of an action shown at time elapsedMs (looping, or stuck on the last frame)
static uint8_t pmdFrameAt(const PmdAct &act, uint32_t elapsedMs, bool loop) {
  uint32_t totalMs = pmdActTotalMs(act);
  if (!loop && elapsedMs >= totalMs) return act.frames - 1;
  elapsedMs %= totalMs;
  uint8_t frameIndex = 0;
  // guard: never more iterations than frames, in case a corrupt .bin has ms[]=0
  // despite the sanitizing on load (defense in depth)
  for (uint8_t guard = 0; guard < act.frames && elapsedMs >= act.ms[frameIndex]; guard++) {
    elapsedMs -= act.ms[frameIndex];
    frameIndex = (frameIndex + 1) % act.frames;
  }
  return frameIndex;
}

static void drawPmdFrameAtScalePercent(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop,
                                       bool sil, uint16_t scalePercent);

void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                 bool keepIdleScale, uint8_t jumpHeightPercent) {
  drawPmdActResizedM(m, actId, cx, groundY, t, loop, sil, maxS, PMD_NATURAL_SIZE_PERCENT, keepIdleScale,
                     jumpHeightPercent);
}

// row+1 of the lowest visible pixel of a frame (0 when the frame is empty)
static uint8_t lowestOpaqueRowOfFrame(const uint8_t *framePixels, uint8_t w, uint8_t h) {
  for (int row = h - 1; row >= 0; row--)
    for (int column = 0; column < w; column++)
      if (framePixels[row * w + column] != TRANSPARENT_PIXEL_INDEX) return (uint8_t)(row + 1);
  return 0;
}

void drawPmdActResizedM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                        uint16_t sizePercent, bool keepIdleScale, uint8_t jumpHeightPercent) {
  const PmdAct &act = m.acts[actId];
  if (!act.frames) return;
  #pragma region Whole zoom: idle height target, clamped, stepped down for tall actions
  uint8_t baseScale = m.acts[PMD_IDLE].h ? PMD_IDLE_TARGET_HEIGHT / m.acts[PMD_IDLE].h : PMD_FALLBACK_SCALE;
  const uint8_t minimumScale = PMD_MIN_SCALE;
  if (baseScale < minimumScale) baseScale = minimumScale;
  if (baseScale > maxS) baseScale = maxS;
  uint8_t scale = baseScale;
  if (!keepIdleScale)
    while (scale > minimumScale && act.h * scale > PMD_ACTION_MAX_HEIGHT) scale--;  // actions with a large frame (attack)
  #pragma endregion
  // anchor by the feet (act.base), not by the canvas height: that way actions
  // with different padding (Hurt, Eat...) all sit at the same ground height
  uint16_t scalePercent = (uint16_t)scale * sizePercent;
  int canvasLeft = cx - act.w * scalePercent / 200;
  int canvasTop = groundY - (act.base ? act.base : act.h) * scalePercent / 100;
  #pragma region Lower jump: push the frame down by the part of its rise that is dropped
  if (jumpHeightPercent < 100 && act.base) {
    // the same frame is drawn right after, so in streaming mode this read is cached
    const uint8_t *framePixels = m.frame(actId, pmdFrameAt(act, t, loop));
    uint8_t frameBottom = framePixels ? lowestOpaqueRowOfFrame(framePixels, act.w, act.h) : 0;
    if (frameBottom && frameBottom < act.base) {
      int riseRows = act.base - frameBottom;
      canvasTop += riseRows * (100 - jumpHeightPercent) * scalePercent / 10000;
    }
  }
  #pragma endregion
  drawPmdFrameAtScalePercent(m, actId, canvasLeft, canvasTop, t, loop, sil, scalePercent);
}

void drawPmdFrameAtScale(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop, bool sil, uint8_t scale) {
  drawPmdFrameAtScalePercent(m, actId, canvasLeft, canvasTop, t, loop, sil, (uint16_t)scale * 100);
}

int scaledPixelEdge(int origin, int pixelIndex, uint16_t scalePercent) {
  return origin + pixelIndex * scalePercent / 100;
}

static void drawPmdFrameAtScalePercent(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop,
                                       bool sil, uint16_t scalePercent) {
  const PmdAct &act = m.acts[actId];
  if (!act.frames) return;
  uint8_t frameIndex = pmdFrameAt(act, t, loop);
  const uint8_t *framePixels = m.frame(actId, frameIndex);  // blob: direct pointer; streaming: reads 1 frame from the SD
  lastSpriteDrawnBounds.valid = false;
  if (!framePixels) return;

  #pragma region Rows of merged same-color runs, tracking the opaque pixel box
  computeScaledColumnEdges(act.w, canvasLeft, scalePercent, 100);
  const uint16_t *palette = m.pal;
  auto colorForPaletteIndex = [palette, sil](uint8_t paletteIndex) -> uint16_t {
    return sil ? SPRITE_OUTLINE_COLOR : palette[paletteIndex];
  };
  int opaqueColumnMin = act.w, opaqueColumnMax = -1, opaqueRowMin = act.h, opaqueRowMax = -1;
  int rowTop = scaledPixelEdge(canvasTop, 0, scalePercent);
  for (int row = 0; row < act.h; row++) {
    int rowBottom = scaledPixelEdge(canvasTop, row + 1, scalePercent);
    int rowHeight = rowBottom - rowTop;
    if (rowHeight > 0) {
      int firstOpaqueColumn, lastOpaqueColumn;
      if (drawIndexedRowRuns(framePixels + row * act.w, act.w, rowTop, rowHeight, colorForPaletteIndex,
                             precomputedColumnEdge, firstOpaqueColumn, lastOpaqueColumn)) {
        if (firstOpaqueColumn < opaqueColumnMin) opaqueColumnMin = firstOpaqueColumn;
        if (lastOpaqueColumn > opaqueColumnMax) opaqueColumnMax = lastOpaqueColumn;
        if (row < opaqueRowMin) opaqueRowMin = row;
        opaqueRowMax = row;
      }
    }
    rowTop = rowBottom;
  }
  #pragma endregion

  #pragma region Remember the opaque pixel box on screen (coin chase pickup)
  if (opaqueColumnMax >= 0) {
    lastSpriteDrawnBounds = { (int16_t)scaledPixelEdge(canvasLeft, opaqueColumnMin, scalePercent),
                              (int16_t)scaledPixelEdge(canvasTop, opaqueRowMin, scalePercent),
                              (int16_t)(scaledPixelEdge(canvasLeft, opaqueColumnMax + 1, scalePercent) - 1),
                              (int16_t)(scaledPixelEdge(canvasTop, opaqueRowMax + 1, scalePercent) - 1), true };
  }
  #pragma endregion
}

void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                bool keepIdleScale, uint8_t jumpHeightPercent) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS, keepIdleScale, jumpHeightPercent);
}
#pragma endregion

#pragma region Gallery thumbnails
void drawThumbnailInCell(const uint8_t *thumb, int cellLeft, int cellTop, int cellSize, int scaleNumerator,
                         int scaleDenominator, bool silhouette, bool allowShrink) {
  uint8_t thumbWidth = thumb[0], thumbHeight = thumb[1], paletteCount = thumb[2];
  const uint8_t *palette = thumb + 3;
  const uint8_t *pixels = palette + paletteCount * 2;

  #pragma region Optional shrink: step the scale down until the thumbnail fits the cell
  if (allowShrink) {
    while (scaleNumerator > scaleDenominator && (thumbWidth * scaleNumerator / scaleDenominator > cellSize ||
                                                 thumbHeight * scaleNumerator / scaleDenominator > cellSize))
      scaleNumerator--;
  }
  #pragma endregion

  #pragma region Centered in the cell, pixel boundaries rounded so there are no gaps
  int scaledWidth = thumbWidth * scaleNumerator / scaleDenominator;
  int scaledHeight = thumbHeight * scaleNumerator / scaleDenominator;
  int originX = cellLeft + (cellSize - scaledWidth) / 2;
  int originY = cellTop + (cellSize - scaledHeight) / 2;
  computeScaledColumnEdges(thumbWidth, originX, scaleNumerator, scaleDenominator);
  auto colorForPaletteIndex = [palette, silhouette](uint8_t paletteIndex) -> uint16_t {
    return silhouette ? SPRITE_OUTLINE_COLOR
                      : (uint16_t)(palette[paletteIndex * 2] | (palette[paletteIndex * 2 + 1] << 8));
  };
  int firstOpaqueColumn, lastOpaqueColumn;
  for (int row = 0; row < thumbHeight; row++) {
    int rowTop = row * scaleNumerator / scaleDenominator;
    int rowBottom = (row + 1) * scaleNumerator / scaleDenominator;
    drawIndexedRowRuns(pixels + row * thumbWidth, thumbWidth, originY + rowTop, rowBottom - rowTop, colorForPaletteIndex,
                       precomputedColumnEdge, firstOpaqueColumn, lastOpaqueColumn);
  }
  #pragma endregion
}

void drawThumbCell(const uint8_t *thumb, int x, int y, int s, bool sil, int cell) {
  // the rect board shrinks the scale until the thumbnail fits; the round one never does
  drawThumbnailInCell(thumb, x, y, cell, s, 1, sil, !BOARD_IS_ROUND);
}

void drawGalleryGridThumb(const uint8_t *thumb, int cellX, int cellY, bool sil) {
  drawThumbnailInCell(thumb, cellX, cellY, GALLERY_CELL, GALLERY_THUMB_SCALE_NUM, GALLERY_THUMB_SCALE_DEN, sil, false);
}
#pragma endregion

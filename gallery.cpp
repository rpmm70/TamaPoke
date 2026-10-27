// Pokedex gallery (see gallery.h): a filtered 4x4 grid of thumbnails and the
// detail view of one species. The grid is GALLERY_COLS x GALLERY_ROWS cells of
// GALLERY_CELL px at GALLERY_GRID_X/Y (boards/layout.h); the round board's grid
// is smaller so it clears the close button. Thumbnails are drawn at
// GALLERY_THUMB_SCALE_NUM/DEN.

#pragma region Includes
#include "gallery.h"
#include "app_state.h"      // pet, gallery state, galleryPmd
#include "audio.h"          // sfxPlay
#include "board_display.h"  // gfx, clearScreenToDayBackground
#include "boards/layout.h"  // GALLERY_*, CARD_ARROW_HIT_W, SX()/SY()
#include "colors.h"
#include "dex.h"            // DEX_TBL, dexName, DEX_COUNT
#include "i18n.h"           // T()
#include "sdmon.h"          // thumbs, PMD_IDLE
#include "sprite_draw.h"
#include "ui_text.h"
#include "ui_widgets.h"     // drawPageNavigation, pageNavigationHit, pageCountFor, drawBorderedBox, type chips
#pragma endregion

#pragma region Detail view and page dots layout (466-px design values, scaled)
// species header: size 3 when it fits GALLERY_DETAIL_HEADER_MAXW, else size 2 a little lower
static const int GALLERY_DETAIL_HEADER_LARGE_SIZE = 3;
static const int GALLERY_DETAIL_HEADER_SMALL_SIZE = 2;
static const int GALLERY_DETAIL_HEADER_LARGE_TOP = SY(56);
static const int GALLERY_DETAIL_HEADER_SMALL_TOP = SY(60);
// animated sprite: feet on this line, zoom limit
static const int GALLERY_DETAIL_SPRITE_GROUND_Y = SY(300);
static const uint8_t GALLERY_DETAIL_SPRITE_MAX_SCALE = 6;
// thumbnail fallback when the PMD sprite is missing: one cell at a whole scale
static const int GALLERY_DETAIL_THUMB_TOP = SY(135);
static const int GALLERY_DETAIL_THUMB_SCALE = 4;
// "caught" line under the sprite
static const int GALLERY_DETAIL_CAUGHT_TEXT_SIZE = 2;
static const int GALLERY_DETAIL_CAUGHT_Y = SY(392);
// page dots under the grid: spacing, line, filled (current) and outlined radii
static const int GALLERY_PAGE_DOT_STEP = SX(14);
static const int GALLERY_PAGE_DOT_Y = SY(436);
static const int GALLERY_PAGE_DOT_CURRENT_RADIUS = 4;
static const int GALLERY_PAGE_DOT_OTHER_RADIUS = 3;
// marks inside a grid cell (shiny star top right, dex number when there is no thumbnail)
static const int GALLERY_CELL_MARK_TEXT_SIZE = 2;
#pragma endregion

#pragma region Forward declarations
static void drawGalleryFilterButtons();
static bool galleryDetailHasNeighbor(int step);
#pragma endregion

#pragma region Filtered list and the cached detail position
// position of galleryDetail in the filtered list, cached until the list or the
// species shown changes (the detail view asks for it twice per frame)
static int16_t cachedDetailDex = -1;
static int cachedDetailListIndex = -1;

static bool dexPassesGalleryFilter(int16_t dex) {
  switch (galleryFilter) {
    case GALLERY_FILTER_RAISED: return pet.isRegistered(dex);
    case GALLERY_FILTER_CAUGHT: return pet.isCaught(dex);
    case GALLERY_FILTER_SHINY: return pet.isShinyRegistered(dex);
    default: return true;
  }
}

// Rebuilds only when galleryFilterDirty is set: a filter change, opening the
// gallery, every grid redraw (galleryDirty: page change, a catch, the serial
// command) and a catch set it. The detail view reuses the list as it is.
void rebuildGalleryFilteredList() {
  // a hatch or catch can register a species while the detail view is open (the egg
  // hatches on its own): a changed registered/caught total also invalidates the list
  static uint16_t registeredCountAtLastBuild = 0, caughtCountAtLastBuild = 0;
  uint16_t registeredCountNow = pet.registeredCount(), caughtCountNow = pet.caughtCount();
  if (registeredCountNow != registeredCountAtLastBuild || caughtCountNow != caughtCountAtLastBuild)
    galleryFilterDirty = true;
  if (!galleryFilterDirty) return;
  galleryFilterDirty = false;
  registeredCountAtLastBuild = registeredCountNow;
  caughtCountAtLastBuild = caughtCountNow;
  galleryFilteredCount = 0;
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
    if (dexPassesGalleryFilter(dex)) galleryFilteredDex[galleryFilteredCount++] = dex;
  cachedDetailDex = -1;
}

int galleryPageCount() {
  return pageCountFor(galleryFilteredCount, GALLERY_PAGE_SIZE);
}

// position of dex in the filtered list, -1 when it does not pass the filter
static int galleryFilteredIndexOf(int16_t dex) {
  for (int listIndex = 0; listIndex < galleryFilteredCount; listIndex++)
    if (galleryFilteredDex[listIndex] == dex) return listIndex;
  return -1;
}

static int galleryDetailListIndex() {
  rebuildGalleryFilteredList();
  if (cachedDetailDex != galleryDetail) {
    cachedDetailListIndex = galleryFilteredIndexOf(galleryDetail);
    cachedDetailDex = galleryDetail;
  }
  return cachedDetailListIndex;
}
#pragma endregion

#pragma region Rendering
static void renderGalleryDetail() {
  clearScreenToDayBackground();
  const DexEntry &entry = DEX_TBL[galleryDetail];
  // known = raised or caught in a wild battle: color, name and animation
  bool known = pet.isRegistered(galleryDetail) || pet.isCaught(galleryDetail);

  #pragma region Species header, auto-shrunk when long
  char head[32];
  snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
           pet.isShinyRegistered(galleryDetail) ? "*" : "", known ? dexName(galleryDetail) : "???");
  gfx->setTextColor(known ? entry.accent : UI_INK);
  // measuring instead of counting bytes; 13 characters at size 3 are the 234 px from before
  setSize(GALLERY_DETAIL_HEADER_LARGE_SIZE);
  int headerSize = (textW(head, GALLERY_DETAIL_HEADER_LARGE_SIZE) <= GALLERY_DETAIL_HEADER_MAXW)
                       ? GALLERY_DETAIL_HEADER_LARGE_SIZE
                       : GALLERY_DETAIL_HEADER_SMALL_SIZE;
  setSize(headerSize);
  setCur(centerX(head, headerSize),
         headerSize == GALLERY_DETAIL_HEADER_LARGE_SIZE ? GALLERY_DETAIL_HEADER_LARGE_TOP : GALLERY_DETAIL_HEADER_SMALL_TOP);
  printT(head);
  #pragma endregion

  #pragma region Type chips (shown for every species, known or not)
  int typesWidth = typeChipsTotalWidth(entry, GALLERY_TYPE_CHIP_STYLE);
  drawTypeChips(CX - typesWidth / 2, GALLERY_DETAIL_TYPES_Y, entry, false, GALLERY_TYPE_CHIP_STYLE);
  #pragma endregion

  #pragma region Sprite: animated in color when known, static silhouette when not
  if (galleryPmd.loaded) {
    drawPmdActM(galleryPmd, PMD_IDLE, CX, GALLERY_DETAIL_SPRITE_GROUND_Y, known ? millis() : 0, true, !known,
                GALLERY_DETAIL_SPRITE_MAX_SCALE);
  } else {
    const uint8_t *thumb = thumbs.get(galleryDetail);
    if (thumb) drawThumbCell(thumb, CX - GALLERY_CELL / 2, GALLERY_DETAIL_THUMB_TOP, GALLERY_DETAIL_THUMB_SCALE, !known, GALLERY_CELL);
  }
  #pragma endregion

  #pragma region Caught mark (species caught in a wild battle)
  if (pet.isCaught(galleryDetail)) {
    gfx->setTextColor(UI_BAR_OK);
    ctext(T(S_CAUGHT_MARK), GALLERY_DETAIL_CAUGHT_TEXT_SIZE, GALLERY_DETAIL_CAUGHT_Y, LCD_WIDTH - 2 * CARD_ARROW_HIT_W);
  }
  #pragma endregion

  drawPageNavigation(galleryDetailHasNeighbor(-1), galleryDetailHasNeighbor(1));
  gfx->flush();
}

static void drawGalleryGridCell(int16_t dex, int cellLeft, int cellTop) {
  const uint8_t *thumb = thumbs.get(dex);
  if (!thumb) {
    #pragma region No thumbnail: the dex number in grey
    char dexNumberText[6];
    snprintf(dexNumberText, sizeof(dexNumberText), "%d", dex);
    gfx->setTextColor(UI_TRACK);
    setSize(GALLERY_CELL_MARK_TEXT_SIZE);
    setCur(cellLeft + GALLERY_CELL * 24 / 80, cellTop + GALLERY_CELL * 32 / 80);  // offsets tuned for an 80 px cell
    printT(dexNumberText);
    return;
    #pragma endregion
  }
  bool raised = pet.isRegistered(dex);
  bool caught = pet.isCaught(dex);
  drawGalleryGridThumb(thumb, cellLeft, cellTop, !raised && !caught);  // known = raised or caught
  #pragma region Shiny star (top right of the cell)
  if (pet.isShinyRegistered(dex)) {
    gfx->setTextColor(UI_BAR_WARN);
    setSize(GALLERY_CELL_MARK_TEXT_SIZE);
    setCur(cellLeft + GALLERY_CELL * 62 / 80, cellTop + GALLERY_CELL * 4 / 80);  // offsets tuned for an 80 px cell
    printT("*");
  }
  #pragma endregion
  #pragma region Caught marker C: caught in a wild battle but never raised (bottom left of the cell)
  if (caught && !raised) {
    gfx->setTextColor(UI_BAR_OK);
    setSize(GALLERY_CAUGHT_MARK_SIZE);
    setCur(cellLeft + GALLERY_CELL * 4 / 80, cellTop + GALLERY_CELL * 58 / 80);
    printT("C");
  }
  #pragma endregion
}

void renderGallery() {
  if (galleryDetail) {  // detail view: always redrawn (animated)
    renderGalleryDetail();
    return;
  }

  if (!galleryDirty) return;  // the grid is static
  galleryDirty = false;
  // every grid redraw refreshes the filtered list: catches and dex registrations
  // reach the gallery through galleryDirty
  galleryFilterDirty = true;

  clearScreenToDayBackground();

  #pragma region Title and the raised / caught counts (unfiltered totals)
  gfx->setTextColor(UI_INK);
  drawGlyphCenteredText(T(S_POKEDEX_TITLE), GALLERY_TITLE_SIZE, GALLERY_TITLE_Y);
  char countsText[24];
  snprintf(countsText, sizeof(countsText), "R: %u  C: %u", pet.registeredCount(), pet.caughtCount());
  drawGlyphCenteredText(countsText, GALLERY_COUNTS_SIZE, GALLERY_COUNTS_Y);
  #pragma endregion

  drawGalleryFilterButtons();

  rebuildGalleryFilteredList();
  int pageCount = galleryPageCount();
  if (galleryPage > pageCount - 1) galleryPage = pageCount - 1;

  #pragma region Grid cells of the current page
  for (int row = 0; row < GALLERY_ROWS; row++) {
    for (int column = 0; column < GALLERY_COLS; column++) {
      int listIndex = galleryPage * GALLERY_PAGE_SIZE + row * GALLERY_COLS + column;
      if (listIndex >= galleryFilteredCount) break;
      drawGalleryGridCell(galleryFilteredDex[listIndex], GALLERY_GRID_X + column * GALLERY_CELL,
                          GALLERY_GRID_Y + row * GALLERY_CELL);
    }
  }
  #pragma endregion

  #pragma region Page dots: one per page of the filtered list, centered under the grid
  int dotsCenterX = GALLERY_GRID_X + GALLERY_COLS * GALLERY_CELL / 2;
  int firstDotX = dotsCenterX - ((pageCount - 1) * GALLERY_PAGE_DOT_STEP) / 2;
  for (int i = 0; i < pageCount; i++) {
    if (i == galleryPage) gfx->fillCircle(firstDotX + i * GALLERY_PAGE_DOT_STEP, GALLERY_PAGE_DOT_Y, GALLERY_PAGE_DOT_CURRENT_RADIUS, UI_INK);
    else gfx->drawCircle(firstDotX + i * GALLERY_PAGE_DOT_STEP, GALLERY_PAGE_DOT_Y, GALLERY_PAGE_DOT_OTHER_RADIUS, UI_INK);
  }
  #pragma endregion

  drawPageNavigation(galleryPage > 0, galleryPage < pageCount - 1);
  gfx->flush();
}
#pragma endregion

#pragma region Gallery filter buttons
static int galleryFilterButtonTop(int filterIndex) {
  return GALLERY_FILTER_Y + filterIndex * (GALLERY_FILTER_H + GALLERY_FILTER_GAP);
}

// stacked buttons left of the grid; the active filter is filled green with light text
static void drawGalleryFilterButtons() {
  static const StrId filterLabels[GALLERY_FILTER_COUNT] = { S_FILTER_ALL, S_FILTER_RAISED, S_FILTER_CAUGHT, S_FILTER_SHINY };

  #pragma region One text size for every label: the largest that fits the longest one
  int labelTextSize = GALLERY_FILTER_TEXT_SIZE;
  for (int filterIndex = 0; filterIndex < GALLERY_FILTER_COUNT; filterIndex++) {
    int fittingSize = shrinkTextSizeToWidth(T(filterLabels[filterIndex]), GALLERY_FILTER_TEXT_SIZE,
                                            GALLERY_FILTER_W - 2 * SETTINGS_TEXT_SIDE_PAD);
    if (fittingSize < labelTextSize) labelTextSize = fittingSize;
  }
  #pragma endregion

  for (int filterIndex = 0; filterIndex < GALLERY_FILTER_COUNT; filterIndex++) {
    int buttonTop = galleryFilterButtonTop(filterIndex);
    bool active = filterIndex == galleryFilter;
    drawBorderedBox(GALLERY_FILTER_X, buttonTop, GALLERY_FILTER_W, GALLERY_FILTER_H, GALLERY_FILTER_R,
                    active ? UI_BAR_OK : UI_WHITE, UI_INK);
    gfx->setTextColor(active ? UI_BG_DAY : UI_INK);
    drawTextCenteredInBox(T(filterLabels[filterIndex]), labelTextSize, GALLERY_FILTER_X, buttonTop,
                          GALLERY_FILTER_W, GALLERY_FILTER_H);
  }
}

// true when the tap hit a filter button: switches to it and goes back to page 1
static bool handleGalleryFilterTap(int16_t x, int16_t y) {
  for (int filterIndex = 0; filterIndex < GALLERY_FILTER_COUNT; filterIndex++) {
    if (!isTouchInsideRect(x, y, GALLERY_FILTER_X, galleryFilterButtonTop(filterIndex), GALLERY_FILTER_W, GALLERY_FILTER_H)) continue;
    if (galleryFilter != filterIndex) {
      galleryFilter = (GalleryFilter)filterIndex;
      galleryFilterDirty = true;
      galleryPage = 0;
      galleryDirty = true;
      sfxPlay(SFX_TAP);
    }
    return true;
  }
  return false;
}
#pragma endregion

#pragma region Detail view neighbors
// Shows the species `step` positions away (+1 next, -1 previous) in the detail
// view among those that pass the active filter, if there is one.
static void galleryShowNeighborSpecies(int step) {
  int neighborIndex = galleryDetailListIndex() + step;
  if (neighborIndex < 0 || neighborIndex >= galleryFilteredCount) return;
  int16_t neighborDex = galleryFilteredDex[neighborIndex];
  galleryDetail = neighborDex;
  galleryPage = neighborIndex / GALLERY_PAGE_SIZE;  // the grid shows this species' page when going back
  galleryPmd.load(neighborDex, pet.isShinyRegistered(neighborDex));
}

// detail arrows: is there a filtered species before / after the one shown?
static bool galleryDetailHasNeighbor(int step) {
  int detailIndex = galleryDetailListIndex();
  int neighborIndex = detailIndex + step;
  return detailIndex >= 0 && neighborIndex >= 0 && neighborIndex < galleryFilteredCount;
}
#pragma endregion

#pragma region Open, close and taps
void openGallery() {
  galleryOpen = true;
  galleryPage = 0;
  galleryDetail = 0;
  galleryDirty = true;
  galleryFilterDirty = true;  // the dex may have changed since the last visit
}

void closeGalleryToMainScreen() {
  galleryOpen = false;
  galleryDetail = 0;
  galleryPmd.unload();
}

void galleryTap(int16_t x, int16_t y) {
  int navigationStep;
  bool closeHit;
  if (galleryDetail) {
    #pragma region Detail view: only the X returns to the grid; the arrows switch species; other taps do nothing
    if (!pageNavigationHit(x, y, galleryDetailHasNeighbor(-1), galleryDetailHasNeighbor(1), navigationStep, closeHit)) return;
    if (!closeHit) {
      galleryShowNeighborSpecies(navigationStep);
      return;
    }
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
    #pragma endregion
  }
  rebuildGalleryFilteredList();
  #pragma region Grid: close X and page arrows
  if (pageNavigationHit(x, y, galleryPage > 0, galleryPage < galleryPageCount() - 1, navigationStep, closeHit)) {
    if (closeHit) {
      closeGalleryToMainScreen();
    } else {
      galleryPage += navigationStep;
      galleryDirty = true;
    }
    return;
  }
  #pragma endregion
  if (handleGalleryFilterTap(x, y)) return;
  #pragma region Grid cell: opens the detail view of that species
  // same reason as in the keyboard: filter before dividing
  if (x < GALLERY_GRID_X || y < GALLERY_GRID_Y) return;
  int column = (x - GALLERY_GRID_X) / GALLERY_CELL, row = (y - GALLERY_GRID_Y) / GALLERY_CELL;
  if (column >= GALLERY_COLS || row >= GALLERY_ROWS) return;
  int listIndex = galleryPage * GALLERY_PAGE_SIZE + row * GALLERY_COLS + column;
  if (listIndex >= galleryFilteredCount) return;
  int16_t dex = galleryFilteredDex[listIndex];
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
  #pragma endregion
}
#pragma endregion

// Creature info card (tap the name on the main screen): profile with the
// animated portrait, stats, progress and medals pages. See info_card.h.

#pragma region Includes
#include "info_card.h"
#include "app_state.h"      // pet, pmd, cardPage, cardOpen, cardPencil*, gCjkFont, gTextSize
#include "battle.h"         // battleMaxHpFor
#include "battle_ui.h"      // petBattleStats
#include "board_display.h"  // gfx, clearScreenToDayBackground
#include "boards/layout.h"  // CARD_*, MED_*, BAR_H_CARD, CLOSE_BTN_*, SX()
#include "colors.h"
#include "dex.h"            // DEX_TBL, dexName, DEX_COUNT
#include "hud.h"            // statusMsg
#include "i18n.h"           // T(), medalDesc, gLang
#include "keyboard.h"       // openKeyboard
#include "medals.h"         // MED_COUNT
#include "sprite_draw.h"    // drawPmdFrameAtScale
#include "ui_icons.h"       // drawPencilIcon, isTouchInsideCircle
#include "ui_text.h"
#include "ui_widgets.h"     // drawProgressBar, drawInsetProgressBar, drawPageNavigation, pageNavigationHit, chips
#pragma endregion

#pragma region Info card layout constants
// Stats page rows (page 1), designed in 466 px units
#define CARD_STATS_LABEL_X SX(96)        // labels start here
#define CARD_STATS_VALUE_X SX(330)       // numbers start here
#define CARD_STATS_BAR_RIGHT SX(310)     // every bar ends here, leaving space before the number
#define CARD_STATS_BAR_MIN_X SX(150)     // bars never start left of this (the Latin-script layout)
#define CARD_STATS_LABEL_BAR_GAP 12      // px between the widest label and the bars
#define CARD_STATS_BAR_TOP_OFFSET 2      // bar top below the row top
#define CARD_STATS_BAR_H 11
#define CARD_STATS_BAR_R 3
#define CARD_STATS_TEXT_SIZE 2
// stat bar scales: ATK / DEF / SPE fill at 260, weight at 100
#define CARD_STATS_BATTLE_STAT_BAR_MAX 260
#define CARD_STATS_WEIGHT_BAR_MAX 100
// profile page (page 0): bond bar scale and bar corner radius
#define CARD_PROFILE_BOND_MAX 100
#define CARD_PROFILE_BAR_R 3
// progress page (page 2): bar to the next level
#define CARD_PROGRESS_BAR_X SX(93)
#define CARD_PROGRESS_BAR_W SX(280)
#define CARD_PROGRESS_BAR_TRACK_R 6
#define CARD_PROGRESS_BAR_FILL_R 5
// text lines that must stay clear of the page arrows
#ifndef CARD_TEXT_MAX_W
#define CARD_TEXT_MAX_W (LCD_WIDTH - 2 * CARD_SIDE_MARGIN)
#endif
// medal pills (page 3)
#define CARD_MEDAL_RADIUS 10
// evolution blocked while the lowest need is under this
#define CARD_EVO_MIN_LOWEST_STAT 40
// page indicator dots
#define CARD_DOT_ACTIVE_R 5
#define CARD_DOT_INACTIVE_R 4
// CJK unifont glyphs are 16 px tall per scale step (the classic font: FONT_H)
#define CJK_GLYPH_HEIGHT 16
#pragma endregion

#pragma region Info card stats rows
// x where the info card bars start. It was fixed at 150, which was plenty for
// 3-character Latin labels but not for the wider Japanese ones, which ran into
// the bar. It is computed from the longest label so the four bars stay aligned
// in any language, with the original value as a floor (so nothing changes in
// the Latin-script languages). Measured once per language and cached.
// Requires text size 2 to already be set, because textW() needs it.
static int statBarX() {
  static int cachedLanguage = -1;
  static int cachedBarX = 0;
  if (cachedLanguage == (int)gLang) return cachedBarX;
  const StrId labelIds[] = { S_STAT_HP, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_VIN };
  int widestLabelWidth = 0;
  for (StrId labelId : labelIds) {
    int labelWidth = textW(T(labelId), CARD_STATS_TEXT_SIZE);
    if (labelWidth > widestLabelWidth) widestLabelWidth = labelWidth;
  }
  int barX = CARD_STATS_LABEL_X + widestLabelWidth + CARD_STATS_LABEL_BAR_GAP;
  cachedBarX = barX < CARD_STATS_BAR_MIN_X ? CARD_STATS_BAR_MIN_X : barX;
  cachedLanguage = (int)gLang;
  return cachedBarX;
}

// label on the left and the number on the right, with no bar (the HP row)
static void drawCardStatLabelAndNumber(int rowTop, const char *label, uint16_t value) {
  gfx->setTextColor(UI_INK);
  setSize(CARD_STATS_TEXT_SIZE);
  setCur(CARD_STATS_LABEL_X, rowTop);
  printT(label);
  char valueText[8];
  snprintf(valueText, sizeof(valueText), "%u", value);
  setCur(CARD_STATS_VALUE_X, rowTop);
  printT(valueText);
}

static void drawCardStat(int rowTop, const char *label, uint16_t value, uint16_t barMax, uint16_t barColor) {
  drawCardStatLabelAndNumber(rowTop, label, value);
  int barLeft = statBarX();
  int barWidth = CARD_STATS_BAR_RIGHT - barLeft;
  drawProgressBar(barLeft, rowTop + CARD_STATS_BAR_TOP_OFFSET, barWidth, CARD_STATS_BAR_H, CARD_STATS_BAR_R, value, barMax,
                  barColor);
}
#pragma endregion

#pragma region Info card page 0: portrait and stat row helpers (CARD_PROFILE_* in boards/layout.h)
// pencil next to the card name: as tall as a text line of this size, never below
// the minimum (renderCardProfile() sizes it for TS(3) text, whatever the name size)
static int cardPencilSizeForTextSize(int textSize) {
  return FONT_H * textSize < CARD_PENCIL_MIN_SIZE ? CARD_PENCIL_MIN_SIZE : FONT_H * textSize;
}

// Height of one text line at the scale setSize() last applied: 8 px per step
// with the classic font, 16 px per step with the CJK unifont (whose scale is
// already the requested size halved, so sizes 2 and up match the classic font).
static int cardTextLineHeight() {
  return (gCjkFont ? CJK_GLYPH_HEIGHT : FONT_H) * gTextSize;
}

// Smallest box (canvas pixel columns/rows) holding every visible pixel of all the
// idle frames. PMD canvases carry empty margins (and the creature is rarely
// centered inside them), so centering the canvas does not center the creature.
// Measured once per species and cached; reading frames is cheap with a blob and
// one SD read per frame when streaming.
static void measureIdleVisibleBounds(int &minColumn, int &maxColumn, int &minRow, int &maxRow) {
  const PmdAct &idle = pmd.acts[PMD_IDLE];
  static int cachedKey = -1;
  static int cachedMinColumn, cachedMaxColumn, cachedMinRow, cachedMaxRow;
  int key = (pet.speciesId * 2 + (pet.shiny ? 1 : 0)) * 65536 + idle.w * 256 + idle.h;
  if (key != cachedKey) {
    cachedMinColumn = idle.w;
    cachedMaxColumn = -1;
    cachedMinRow = idle.h;
    cachedMaxRow = -1;
    for (uint8_t frameIndex = 0; frameIndex < idle.frames; frameIndex++) {
      const uint8_t *frameData = pmd.frame(PMD_IDLE, frameIndex);
      if (!frameData) continue;
      for (int row = 0; row < idle.h; row++) {
        for (int column = 0; column < idle.w; column++) {
          if (frameData[row * idle.w + column] == 0xFF) continue;
          if (column < cachedMinColumn) cachedMinColumn = column;
          if (column > cachedMaxColumn) cachedMaxColumn = column;
          if (row < cachedMinRow) cachedMinRow = row;
          if (row > cachedMaxRow) cachedMaxRow = row;
        }
      }
    }
    if (cachedMaxColumn < 0) {  // nothing visible (or unreadable): use the whole canvas
      cachedMinColumn = 0;
      cachedMaxColumn = idle.w - 1;
      cachedMinRow = 0;
      cachedMaxRow = idle.h - 1;
    }
    cachedKey = key;
  }
  minColumn = cachedMinColumn;
  maxColumn = cachedMaxColumn;
  minRow = cachedMinRow;
  maxRow = cachedMaxRow;
}

// Idle portrait at the largest whole scale at which its VISIBLE pixels fit the
// portrait box (capped), with those visible pixels centered in the box. Not
// drawPmdAct(): its minimum scale of 2 on the round board would push the widest
// sprites (96 px canvas) out of the column.
static void drawCardProfilePortrait() {
  const PmdAct &idle = pmd.acts[PMD_IDLE];
  if (!pmd.loaded || !idle.frames) return;
  int minColumn, maxColumn, minRow, maxRow;
  measureIdleVisibleBounds(minColumn, maxColumn, minRow, maxRow);
  int visibleWidth = maxColumn - minColumn + 1;
  int visibleHeight = maxRow - minRow + 1;
  int scale = CARD_PROFILE_PORTRAIT_W / visibleWidth;
  if (CARD_PROFILE_PORTRAIT_H / visibleHeight < scale) scale = CARD_PROFILE_PORTRAIT_H / visibleHeight;
  if (scale > CARD_PROFILE_PORTRAIT_MAX_SCALE) scale = CARD_PROFILE_PORTRAIT_MAX_SCALE;
  if (scale < 1) scale = 1;
  int canvasLeft = CARD_PROFILE_PORTRAIT_X + (CARD_PROFILE_PORTRAIT_W - visibleWidth * scale) / 2 - minColumn * scale;
  int canvasTop = CARD_PROFILE_PORTRAIT_Y + (CARD_PROFILE_PORTRAIT_H - visibleHeight * scale) / 2 - minRow * scale;
  drawPmdFrameAtScale(pmd, PMD_IDLE, canvasLeft, canvasTop, millis(), true, false, scale);
}

#pragma region Status message layout cache
// How the status line was laid out: the text size and the space it was split
// at (-1 = one line). statusMsg() always returns a string constant, so the
// message pointer plus the language identify the text; the layout is only
// searched again when either changes (the search measures every split point).
struct StatusMessageLayout {
  const char *message;
  int language;
  int textSize;
  int splitIndex;
};
static StatusMessageLayout cachedStatusLayout = { nullptr, -1, 1, -1 };

// Tries the preferred size first and steps down, so a wrapped line is preferred
// to tiny text. workingText is restored before returning.
static void findStatusMessageLayout(char *workingText, int columnWidth, int &chosenTextSize, int &chosenSplitIndex) {
  for (int textSize = CARD_PROFILE_STATUS_SIZE; textSize >= 1; textSize--) {
    setSize(textSize);
    #pragma region Single line
    if (textW(workingText, textSize) <= columnWidth) {
      chosenTextSize = textSize;
      chosenSplitIndex = -1;
      return;
    }
    #pragma endregion

    #pragma region Two lines, split at the space that balances them best
    int bestSplitIndex = -1;
    int bestWidestLine = columnWidth + 1;
    for (int i = 0; workingText[i]; i++) {
      if (workingText[i] != ' ') continue;
      workingText[i] = '\0';
      int widestLine = textW(workingText, textSize);
      int secondLineWidth = textW(workingText + i + 1, textSize);
      workingText[i] = ' ';
      if (secondLineWidth > widestLine) widestLine = secondLineWidth;
      if (widestLine < bestWidestLine) {
        bestWidestLine = widestLine;
        bestSplitIndex = i;
      }
    }
    if (bestSplitIndex >= 0 && bestWidestLine <= columnWidth) {
      chosenTextSize = textSize;
      chosenSplitIndex = bestSplitIndex;
      return;
    }
    #pragma endregion
  }
  // nothing fit even at size 1: the single line anyway, centered on the column
  chosenTextSize = 1;
  chosenSplitIndex = -1;
}
#pragma endregion

// Creature status text (statusMsg()) centered under the portrait, in one line if
// it fits the column, else split at a space into two, else shrunk.
static void drawCardProfileStatusMessage() {
  const int columnWidth = CARD_PROFILE_PORTRAIT_W;
  const int columnCenterX = CARD_PROFILE_PORTRAIT_X + CARD_PROFILE_PORTRAIT_W / 2;
  const char *message = statusMsg();
  char workingText[48];
  snprintf(workingText, sizeof(workingText), "%s", message);
  gfx->setTextColor(UI_INK);

  #pragma region Layout: cached per (message, language)
  if (cachedStatusLayout.message != message || cachedStatusLayout.language != (int)gLang) {
    findStatusMessageLayout(workingText, columnWidth, cachedStatusLayout.textSize, cachedStatusLayout.splitIndex);
    cachedStatusLayout.message = message;
    cachedStatusLayout.language = (int)gLang;
  }
  const int textSize = cachedStatusLayout.textSize;
  setSize(textSize);
  #pragma endregion

  #pragma region One line
  if (cachedStatusLayout.splitIndex < 0) {
    setCur(columnCenterX - textW(workingText, textSize) / 2, CARD_PROFILE_STATUS_Y);
    printT(workingText);
    return;
  }
  #pragma endregion

  #pragma region Two lines
  workingText[cachedStatusLayout.splitIndex] = '\0';
  const char *secondLine = workingText + cachedStatusLayout.splitIndex + 1;
  setCur(columnCenterX - textW(workingText, textSize) / 2, CARD_PROFILE_STATUS_Y);
  printT(workingText);
  setCur(columnCenterX - textW(secondLine, textSize) / 2, CARD_PROFILE_STATUS_Y + cardTextLineHeight() + 2);
  printT(secondLine);
  #pragma endregion
}

// Label of a stat row, left-justified in the stats column and shrunk to its
// width on both boards. Returns the top of the value line under it.
static int drawCardProfileRowLabel(int rowTop, const char *label) {
  gfx->setTextColor(UI_LABEL);
  shrinkTextSizeToWidth(label, CARD_PROFILE_LABEL_SIZE, CARD_PROFILE_STATS_W);
  setCur(CARD_PROFILE_STATS_LEFT, rowTop);
  printT(label);
  return rowTop + cardTextLineHeight() + CARD_PROFILE_VALUE_GAP;
}

// Stat row whose value is text, right-justified on the line under the label
static void drawCardProfileTextRow(int rowTop, const char *label, const char *value) {
  int valueTop = drawCardProfileRowLabel(rowTop, label);
  gfx->setTextColor(UI_INK);
  int valueSize = shrinkTextSizeToWidth(value, CARD_PROFILE_VALUE_SIZE, CARD_PROFILE_STATS_W);
  setCur(CARD_PROFILE_STATS_RIGHT - textW(value, valueSize), valueTop);
  printT(value);
}

// Stat row whose value is a bar and its number: the number right-justified, the
// bar filling the rest of the line. The number slot is measured on maxValue,
// so the bar keeps the same length whatever the value.
static void drawCardProfileBarRow(int rowTop, const char *label, uint16_t value, uint16_t maxValue, uint16_t barColor) {
  int valueTop = drawCardProfileRowLabel(rowTop, label);
  char valueText[8], maxValueText[8];
  snprintf(valueText, sizeof(valueText), "%u", value);
  snprintf(maxValueText, sizeof(maxValueText), "%u", maxValue);
  int valueSize = shrinkTextSizeToWidth(maxValueText, CARD_PROFILE_VALUE_SIZE, CARD_PROFILE_STATS_W);
  int numberSlotWidth = textW(maxValueText, valueSize);
  int lineHeight = cardTextLineHeight();
  gfx->setTextColor(UI_INK);
  setCur(CARD_PROFILE_STATS_RIGHT - textW(valueText, valueSize), valueTop);
  printT(valueText);
  #pragma region Bar track and fill, vertically centered on the value line
  int barWidth = CARD_PROFILE_STATS_W - numberSlotWidth - CARD_PROFILE_BAR_GAP;
  int barTop = valueTop + (lineHeight - CARD_PROFILE_BAR_H) / 2;
  drawProgressBar(CARD_PROFILE_STATS_LEFT, barTop, barWidth, CARD_PROFILE_BAR_H, CARD_PROFILE_BAR_R, value, maxValue, barColor);
  #pragma endregion
}
#pragma endregion

#pragma region Info card pages
// page 0: profile (name, portrait on the left, stat rows on the right: level,
// streak (best), bond, favorite food, age)
static void renderCardProfile() {
  const DexEntry &speciesEntry = DEX_TBL[pet.speciesId];
  const char *displayName = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char headText[32];
  // no level here: it has its own stat row below
  snprintf(headText, sizeof(headText), "%s%s", pet.shiny ? "*" : "", displayName);
  gfx->setTextColor(speciesEntry.accent);
  // auto-shrink: same size and top as the Progress page title (TS(5) at
  // CARD_PROGRESS_TITLE_Y), stepped down one size at a time until the name plus
  // the pencil fit CARD_HEAD_MAXW (kept clear of the close button). The width is
  // MEASURED with textW() (strlen() counts bytes). The pencil keeps the size it
  // has next to TS(3) text whatever size the name ends up at.
  const int pencilSize = cardPencilSizeForTextSize(TS(3));
  int headTextSize = shrinkTextSizeToWidth(headText, TS(5), CARD_HEAD_MAXW - CARD_PENCIL_GAP - pencilSize);
  int headLineHeight = cardTextLineHeight();
  #pragma region Name and rename pencil, centered together as one group
  int headWidth = textW(headText, headTextSize);
  int groupLeft = CX - (headWidth + CARD_PENCIL_GAP + pencilSize) / 2;
  int headTop = CARD_PROGRESS_TITLE_Y;
  setCur(groupLeft, headTop);
  printT(headText);
  cardPencilLeft = groupLeft + headWidth + CARD_PENCIL_GAP;
  cardPencilTop = headTop + (headLineHeight - pencilSize) / 2;  // centered on the text line
  cardPencilSize = pencilSize;
  drawPencilIcon(gfx, cardPencilLeft, cardPencilTop, cardPencilSize, UI_INK);
  #pragma endregion
  #pragma region Real species under the nickname, just below the title line
  if (pet.nick[0]) {
    char speciesText[32];
    snprintf(speciesText, sizeof(speciesText), "(%s)", dexName(pet.speciesId));
    gfx->setTextColor(UI_TRACK);
    // measured, not counted in bytes
    drawTextCenteredOnCx(speciesText, CARD_PROFILE_SUBTITLE_SIZE, headTop + headLineHeight + CARD_PROFILE_SUBTITLE_GAP);
  }
  #pragma endregion

  #pragma region Animated portrait (left column)
  drawCardProfilePortrait();
  #pragma endregion

  #pragma region Status message under the portrait
  drawCardProfileStatusMessage();
  #pragma endregion

  #pragma region Stat rows (right column): level, streak (best), bond, favorite food, age
  int rowTop = CARD_PROFILE_ROW_Y0;
  char levelText[8];
  snprintf(levelText, sizeof(levelText), "%u", pet.level());
  drawCardProfileTextRow(rowTop, T(S_CARD_LEVEL), levelText);
  rowTop += CARD_PROFILE_ROW_STEP;

  char streakText[16];  // "65535 (65535)" at most
  snprintf(streakText, sizeof(streakText), "%u (%u)", pet.streak, pet.bestStreak);
  drawCardProfileTextRow(rowTop, T(S_CARD_STREAK_BEST), streakText);
  rowTop += CARD_PROFILE_ROW_STEP;

  drawCardProfileBarRow(rowTop, T(S_CARD_BOND), pet.bond, CARD_PROFILE_BOND_MAX, BOND_BAR_COLOR);
  rowTop += CARD_PROFILE_ROW_STEP;

  const char *favoriteBerry = !pet.berryKnown ? T(S_BERRY_UNK)
                              : pet.lovesBerry(0) ? T(S_BERRY_RED)
                              : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                                  : T(S_BERRY_GREEN);
  drawCardProfileTextRow(rowTop, T(S_CARD_FAV_FOOD), favoriteBerry);
  rowTop += CARD_PROFILE_ROW_STEP;

  char ageText[24];
  snprintf(ageText, sizeof(ageText), T(S_CARD_AGE_FMT), (unsigned long)pet.ageDays());
  drawCardProfileTextRow(rowTop, T(S_CARD_AGE), ageText);
  #pragma endregion
}

// page 1: stats (HP number, then 4 bars)
static void renderCardStats() {
  gfx->setTextColor(UI_INK);
  #pragma region Title (same style as the progress page)
  drawScreenTitle(T(S_CARD_STATS));
  #pragma endregion

  #pragma region Type chips centered under the title (an egg has no species yet)
  if (!pet.isEgg() && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    const DexEntry &speciesEntry = DEX_TBL[pet.speciesId];
    int typesWidth = typeChipsTotalWidth(speciesEntry, GALLERY_TYPE_CHIP_STYLE);
    drawTypeChips(CX - typesWidth / 2, CARD_STATS_TYPES_Y, speciesEntry, false, GALLERY_TYPE_CHIP_STYLE);
  }
  #pragma endregion

  #pragma region Stat rows, evenly spaced below the chips (CARD_STATS_* in boards/layout.h)
  int rowTop = CARD_STATS_ROW_Y0;
  // max HP the pet starts a battle with; an egg does not battle
  uint16_t maxHp = pet.isEgg() ? 0 : battleMaxHpFor(petBattleStats());
  drawCardStatLabelAndNumber(rowTop, T(S_STAT_HP), maxHp);
  rowTop += CARD_STATS_ROW_STEP;
  drawCardStat(rowTop, T(S_STAT_ATK), pet.atkStat(), CARD_STATS_BATTLE_STAT_BAR_MAX, UI_BAR_BAD);
  rowTop += CARD_STATS_ROW_STEP;
  drawCardStat(rowTop, T(S_STAT_DEF), pet.defStat(), CARD_STATS_BATTLE_STAT_BAR_MAX, WATER_BLUE_COLOR);
  rowTop += CARD_STATS_ROW_STEP;
  drawCardStat(rowTop, T(S_STAT_SPE), pet.speStat(), CARD_STATS_BATTLE_STAT_BAR_MAX, UI_BAR_WARN);
  rowTop += CARD_STATS_ROW_STEP;
  drawCardStat(rowTop, T(S_STAT_WGT), pet.weight, CARD_STATS_WEIGHT_BAR_MAX, WEIGHT_BAR_COLOR);
  rowTop += CARD_STATS_ROW_STEP;
  #pragma endregion

  #pragma region Battle record: this individual's wild battle wins and losses (no bar)
  drawCardStatLabelAndNumber(rowTop, T(S_STAT_WINS), pet.battleWins);
  rowTop += CARD_STATS_ROW_STEP;
  drawCardStatLabelAndNumber(rowTop, T(S_STAT_LOSSES), pet.battleLosses);
  #pragma endregion
}

// page 3: medals with descriptive labels
static void renderCardMedals() {
  gfx->setTextColor(UI_INK);
  #pragma region Title (same style as the progress page)
  drawScreenTitle(T(S_MEDALS));
  #pragma endregion

  for (int medalIndex = 0; medalIndex < MED_COUNT; medalIndex++) {
    int medalLeft = MED_X0 + (medalIndex % 2) * MED_COLSTEP, medalTop = MED_Y0 + (medalIndex / 2) * MED_STEP;
    bool earned = pet.hasMedal(1 << medalIndex);
    gfx->fillRoundRect(medalLeft, medalTop, MED_W, MED_H, CARD_MEDAL_RADIUS, earned ? UI_BAR_OK : UI_TRACK);
    #pragma region Earned mark
    if (earned) {
      gfx->fillCircle(medalLeft + MED_MARK_CX, medalTop + MED_H / 2, MED_MARK_R, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(medalLeft + MED_MARK_CX - 6, medalTop + MED_H / 2 - 9);
      printT("v");
    }
    #pragma endregion
    gfx->setTextColor(earned ? UI_BG_DAY : MEDAL_LOCKED_TEXT_COLOR);
    int descriptionSize = fitSize(medalDesc(medalIndex), TS(2), MED_W - MED_TXT_X - 6);
    setSize(descriptionSize);
    setCur(medalLeft + MED_TXT_X, medalTop + (MED_H - FONT_H * descriptionSize) / 2);
    printT(medalDesc(medalIndex));
  }
}

// page 2: progress (level, evolution, care mistakes) - surfaces mechanics that
// used to be invisible (how long until leveling up/evolving and why)
static void renderCardProgress() {
  const DexEntry &speciesEntry = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  // large title, shrunk to fit the circle / screen width (long translations)
  drawScreenTitle(T(S_PROGRESS));

  #pragma region Level under the title
  char levelText[10];
  snprintf(levelText, sizeof(levelText), T(S_LVL_FMT), pet.level());
  ctext(levelText, 3, CARD_PROGRESS_LEVEL_Y);
  #pragma endregion

  #pragma region Progress bar to the next level (1 level = 60 min of play)
  uint8_t minutesIntoLevel = pet.ageMinutes % MINUTES_PER_LEVEL;
  drawInsetProgressBar(CARD_PROGRESS_BAR_X, CARD_PROGRESS_BAR_Y, CARD_PROGRESS_BAR_W, BAR_H_CARD, CARD_PROGRESS_BAR_TRACK_R,
                       CARD_PROGRESS_BAR_FILL_R, minutesIntoLevel, MINUTES_PER_LEVEL, UI_BAR_OK);
  char nextLevelText[32];
  snprintf(nextLevelText, sizeof(nextLevelText), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - minutesIntoLevel, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  ctext(nextLevelText, 2, CARD_PROGRESS_BAR_Y + CARD_PROGRESS_NEXT_GAP, CARD_TEXT_MAX_W);  // clear of the page arrows
  #pragma endregion

  #pragma region Evolution status (these lines sit level with the page arrows)
  gfx->setTextColor(UI_LABEL);
  ctext(T(S_EVO_LABEL), 2, CARD_PROGRESS_EVO_LABEL_Y, CARD_TEXT_MAX_W);
  char evolutionBuffer[32];
  const char *evolutionText;
  uint16_t evolutionColor = UI_INK;
  if (speciesEntry.evolvesTo == 0) {
    evolutionText = T(S_FINAL_FORM);
  } else {
    int neededLevel = speciesEntry.evolveLevel + pet.careMistakes;
    if (pet.level() >= neededLevel) {
      if (pet.lowestStat() >= CARD_EVO_MIN_LOWEST_STAT) {
        evolutionText = T(S_EVO_READY);
        evolutionColor = UI_BAR_OK;
      } else {
        evolutionText = T(S_EVO_BLOCKED);
        evolutionColor = UI_BAR_BAD;
      }
    } else {
      snprintf(evolutionBuffer, sizeof(evolutionBuffer), T(S_EVO_IN_FMT), neededLevel - pet.level());
      evolutionText = evolutionBuffer;
    }
  }
  gfx->setTextColor(evolutionColor);
  ctext(evolutionText, 2, CARD_PROGRESS_EVO_VALUE_Y, CARD_TEXT_MAX_W);
  #pragma endregion

  #pragma region Care mistakes (delay evolution)
  char mistakesText[24];
  snprintf(mistakesText, sizeof(mistakesText), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  ctext(mistakesText, 2, CARD_PROGRESS_MISTAKES_Y);
  #pragma endregion
}
#pragma endregion

#pragma region Public API
// Page 0 has the animated portrait and must be drawn every frame; pages 1-3 are
// static: render() only calls this for them when consumeStaticScreenRedraw() allows.
void renderCard() {
  clearScreenToDayBackground();
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardProgress();
  else renderCardMedals();

  #pragma region Close button (top right) and page arrows on the side edges (none past the first or the last page)
  drawPageNavigation(cardPage > 0, cardPage < CARD_PAGE_COUNT - 1);
  #pragma endregion

  #pragma region Page indicator dots at the bottom, centered on CX
  for (int pageIndex = 0; pageIndex < CARD_PAGE_COUNT; pageIndex++) {
    int dotCenterX = CX + (2 * pageIndex - (CARD_PAGE_COUNT - 1)) * CARD_DOT_STEP / 2;
    if (pageIndex == cardPage) gfx->fillCircle(dotCenterX, CARD_DOTS_Y, CARD_DOT_ACTIVE_R, UI_INK);
    else gfx->drawCircle(dotCenterX, CARD_DOTS_Y, CARD_DOT_INACTIVE_R, UI_INK);
  }
  #pragma endregion
  gfx->flush();
}

// taps on the info card: close button, rename pencil (page 0) and page arrows.
// Anywhere else does nothing.
void handleInfoCardTap(int16_t touchX, int16_t touchY) {
  #pragma region Close button, drawn circle (wins over the padded pencil box)
  if (isTouchInsideCircle(touchX, touchY, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R)) {
    cardOpen = false;
    return;
  }
  #pragma endregion

  #pragma region Rename pencil next to the name (page 0)
  if (cardPage == 0 && cardPencilSize > 0 &&
      isTouchInsideRect(touchX, touchY, cardPencilLeft - CARD_PENCIL_HIT_PAD, cardPencilTop - CARD_PENCIL_HIT_PAD,
                        cardPencilSize + 2 * CARD_PENCIL_HIT_PAD, cardPencilSize + 2 * CARD_PENCIL_HIT_PAD)) {
    openKeyboard();
    return;
  }
  #pragma endregion

  #pragma region Close button with the generous finger radius, then the page arrows (a hidden arrow ignores taps)
  int pageStep = 0;
  bool closeHit = false;
  if (!pageNavigationHit(touchX, touchY, cardPage > 0, cardPage < CARD_PAGE_COUNT - 1, pageStep, closeHit)) return;
  if (closeHit) cardOpen = false;
  else cardPage += pageStep;
  #pragma endregion
}
#pragma endregion

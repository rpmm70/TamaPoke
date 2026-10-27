// Main screen heads-up display (see hud.h): header, needs bars, action buttons,
// poops, the medal celebration banner and the header messages.

#pragma region Includes
#include "hud.h"
#include "app_state.h"      // pet, buttons, bathUntil, clockUses24Hour, header hit boxes, inkColor
#include "board_display.h"  // gfx
#include "boards/layout.h"  // HDR_*, BAT_*, STREAK_*, CEL_*, NEEDS_*, POOP_*, HAMBURGER_*, POKEDEX_BTN_*, BTN_HALF
#include "colors.h"         // BATTERY_BOLT_COLOR
#include "countof.h"
#include "i18n.h"           // T(), medalName, gLang
#include "medals.h"         // MED_COUNT
#include "rtcbat.h"         // batPercent, batCharging
#include "species.h"        // SPR_POOP, SPR_ICON_POKEDEX
#include "sprite_draw.h"    // drawMap
#include "ui_text.h"
#include "ui_widgets.h"     // drawBorderedBox, drawInsetProgressBar, drawHamburgerIcon
#pragma endregion

#pragma region Constants
// battery icon: body size, terminal nub, level thresholds (percent)
static const int BATTERY_ICON_W = 24;
static const int BATTERY_ICON_H = 11;
static const int BATTERY_ICON_RADIUS = 2;
static const int BATTERY_OK_PERCENT = 40;
static const int BATTERY_LOW_PERCENT = 15;
// the level row keeps a size 2 text line (16 px) even though its text is smaller
static const int HEADER_LEVEL_ROW_LINE_HEIGHT = 2 * FONT_H;
static const int HEADER_LEVEL_BAR_TRACK_RADIUS = 4;
static const int HEADER_LEVEL_BAR_FILL_RADIUS = 2;
static const int HEADER_NAME_TEXT_SIZE = 3;
static const int HEADER_MESSAGE_TEXT_SIZE = 2;
// streak badge: flame plus its gap before the number, number text size and drop
static const int STREAK_FLAME_WIDTH = 22;
static const int STREAK_TEXT_SIZE = 2;
static const int STREAK_TEXT_DY = 2;
// celebration banner: line sizes and the room left on each side of a line
static const int CELEBRATION_TITLE_SIZE = 3;
static const int CELEBRATION_LINE_SIZE = 2;
static const int CELEBRATION_SIDE_ROOM = 16;
// needs bars: label size and its line height, bar radii, color thresholds
static const int NEEDS_LABEL_TEXT_SIZE = 2;
static const int NEEDS_LABEL_LINE_HEIGHT = NEEDS_LABEL_TEXT_SIZE * FONT_H;
static const int NEEDS_BAR_TRACK_RADIUS = 4;
static const int NEEDS_BAR_FILL_RADIUS = 3;
static const int NEEDS_MAX_VALUE = 100;
static const int NEEDS_OK_VALUE = 50;
static const int NEEDS_WARN_VALUE = 25;
// label-to-bar gap: 4 Latin letters at size 2, plus breathing room for wider labels
static const int NEEDS_LABEL_GAP_FLOOR = 48;
static const int NEEDS_LABEL_GAP_MARGIN = 8;
// action buttons: corner radius and the 16x16 icon at x2
static const int ACTION_BUTTON_RADIUS = 14;
static const int ACTION_BUTTON_ICON_SIZE = 16;
static const int ACTION_BUTTON_ICON_SCALE = 2;
static const int POOP_MAP_SIZE = 32;
static const int POKEDEX_ICON_SIZE = 16;
// status messages: a need below this value shows its warning
static const int STATUS_LOW_NEED_VALUE = 25;
static const int STATUS_CHUBBY_WEIGHT = 60;
static const uint32_t STATUS_SHINY_NEWS_MINUTES = 15;
#pragma endregion

#pragma region Header
static void drawBattery() {
  int batteryPercent = batPercent();
  if (batteryPercent < 0) return;  // no battery connected
  int left = BAT_X, top = BAT_Y, width = BATTERY_ICON_W, height = BATTERY_ICON_H;
  bool charging = batCharging();
  uint16_t batteryColor = charging ? UI_BAR_OK
                        : (batteryPercent >= BATTERY_OK_PERCENT) ? inkColor()
                        : (batteryPercent >= BATTERY_LOW_PERCENT) ? UI_BAR_WARN
                                                                  : UI_BAR_BAD;
  gfx->drawRoundRect(left, top, width, height, BATTERY_ICON_RADIUS, batteryColor);
  gfx->fillRect(left + width, top + 3, 3, 5, batteryColor);  // terminal
  if (charging) {
    #pragma region Charging bolt (zigzag) instead of the level bar
    int boltX = left + width / 2;
    gfx->fillTriangle(boltX + 3, top + 1, boltX - 4, top + 6, boltX + 1, top + 6, BATTERY_BOLT_COLOR);
    gfx->fillTriangle(boltX - 1, top + 5, boltX + 4, top + 5, boltX - 3, top + 10, BATTERY_BOLT_COLOR);
    #pragma endregion
  } else {
    int fillWidth = (width - 4) * batteryPercent / 100;
    if (fillWidth > 0) gfx->fillRect(left + 2, top + 2, fillWidth, height - 4, batteryColor);
  }
}

// Name centered on CX at the top; HDR_NAME_MAX_W leaves room for the hamburger,
// the streak badge, the clock and the battery.
static void drawHeaderName(const char *name, uint16_t nameColor) {
  gfx->setTextColor(nameColor);
  ctext(name, HEADER_NAME_TEXT_SIZE, HDR_NAME_Y, HDR_NAME_MAX_W);
  #pragma region Remember where the name landed (tapping it opens the info card)
  // ctext() left the size it actually used set; textW() measures with it
  headerNameWidth = textW(name, gTextSize);
  headerNameLeft = CX - headerNameWidth / 2;
  #pragma endregion
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  drawHeaderName(name, nameColor);
  headerLevelRowWidth = 0;  // no level row here: its tap box is off
  gfx->setTextColor(inkColor());
  ctext(msg, HEADER_MESSAGE_TEXT_SIZE, HDR_MSG_Y);
}

void drawHeaderWithLevel(const char *name, uint16_t nameColor) {
  drawBattery();
  drawHeaderName(name, nameColor);

  #pragma region Level text and progress bar, centered together on CX
  char levelText[12];
  snprintf(levelText, sizeof(levelText), T(S_LVL_FMT), pet.level());
  gfx->setTextColor(inkColor());
  setSize(HDR_LVL_TEXT_SIZE);
  int levelTextWidth = textW(levelText, HDR_LVL_TEXT_SIZE);
  int rowWidth = levelTextWidth + HDR_LVL_GAP + HDR_LVL_BAR_W;
  int rowLeft = CX - rowWidth / 2;
  headerLevelRowLeft = rowLeft;  // remembered for the tap that opens the info card
  headerLevelRowWidth = rowWidth;
  // the row keeps its 16 px line (a size 2 text line); the smaller text and the bar are centered on it
  setCur(rowLeft, HDR_MSG_Y + (HEADER_LEVEL_ROW_LINE_HEIGHT - FONT_H * HDR_LVL_TEXT_SIZE) / 2);
  printT(levelText);
  int barLeft = rowLeft + levelTextWidth + HDR_LVL_GAP;
  int barTop = HDR_MSG_Y + (HEADER_LEVEL_ROW_LINE_HEIGHT - HDR_LVL_BAR_H) / 2;
  uint32_t minutesIntoLevel = pet.ageMinutes % MINUTES_PER_LEVEL;
  drawInsetProgressBar(barLeft, barTop, HDR_LVL_BAR_W, HDR_LVL_BAR_H, HEADER_LEVEL_BAR_TRACK_RADIUS,
                       HEADER_LEVEL_BAR_FILL_RADIUS, (int)minutesIntoLevel, MINUTES_PER_LEVEL, UI_BAR_OK);
  #pragma endregion
}

void drawHamburgerButton() {
  drawHamburgerIcon(gfx, HAMBURGER_X, HAMBURGER_Y, HAMBURGER_W, HAMBURGER_H, inkColor());
}

void drawPokedexButton() {
  drawMap(SPR_ICON_POKEDEX, POKEDEX_ICON_SIZE, POKEDEX_BTN_X, POKEDEX_BTN_Y, POKEDEX_BTN_S, false);
}

// Source: pet.lastSeenEpoch, the same cache sceneHour() reads (0 = no clock yet).
void drawHeaderClock() {
  uint32_t epochSeconds = pet.lastSeenEpoch;
  if (!epochSeconds) return;
  int hour = (int)((epochSeconds / 3600) % 24);
  int minute = (int)((epochSeconds / 60) % 60);
  #pragma region Clock text in the chosen format
  char clockText[8];
  const char *meridiemText = "";  // "AM"/"PM" in 12h mode, one size smaller so the clock stays narrow
  if (clockUses24Hour) {
    snprintf(clockText, sizeof(clockText), "%02d:%02d", hour, minute);
  } else {
    snprintf(clockText, sizeof(clockText), "%d:%02d", hourOn12HourDial(hour), minute);
    meridiemText = hour >= 12 ? "PM" : "AM";
  }
  #pragma endregion
  #pragma region Measure with each size applied (a CJK font measures with the active size)
  const int meridiemSize = HDR_TIME_SIZE - 1;
  int meridiemTextWidth = 0;
  if (meridiemText[0]) {
    setSize(meridiemSize);
    meridiemTextWidth = (int)textW(meridiemText, meridiemSize);
  }
  int meridiemWidth = meridiemText[0] ? HDR_TIME_MERIDIEM_GAP + meridiemTextWidth : 0;
  setSize(HDR_TIME_SIZE);
  int clockLeft = HDR_TIME_RIGHT - meridiemWidth - (int)textW(clockText, HDR_TIME_SIZE);
  #pragma endregion
  gfx->setTextColor(inkColor());
  setCur(clockLeft, HDR_TIME_Y);
  printT(clockText);
  if (meridiemText[0]) {  // bottom-aligned with the digits (FONT_H px per text size step)
    setSize(meridiemSize);
    setCur(HDR_TIME_RIGHT - meridiemTextWidth, HDR_TIME_Y + FONT_H * (HDR_TIME_SIZE - meridiemSize));
    printT(meridiemText);
  }
}

// flame + streak number at the top right (under the clock and the battery),
// right-justified on STREAK_RIGHT: STREAK_FLAME_WIDTH for the flame and its gap, then the number
void drawStreakBadge() {
  if (pet.streak < 1) return;
  char streakText[6];
  snprintf(streakText, sizeof(streakText), "%u", pet.streak);
  setSize(STREAK_TEXT_SIZE);  // before textW(): a CJK font measures with the active size
  int x = STREAK_RIGHT - STREAK_FLAME_WIDTH - (int)textW(streakText, STREAK_TEXT_SIZE), y = STREAK_Y;
  #pragma region Flame: one teardrop silhouette (circle + tip triangle) with a smaller inner flame
  gfx->fillCircle(x + 8, y + 12, 6, UI_BAR_BAD);
  gfx->fillTriangle(x + 9, y, x + 2, y + 11, x + 14, y + 11, UI_BAR_BAD);
  gfx->fillCircle(x + 8, y + 13, 3, UI_BAR_WARN);
  gfx->fillTriangle(x + 8, y + 7, x + 5, y + 13, x + 11, y + 13, UI_BAR_WARN);
  #pragma endregion
  gfx->setTextColor(inkColor());
  setCur(x + STREAK_FLAME_WIDTH, y + STREAK_TEXT_DY);
  printT(streakText);
}
#pragma endregion

#pragma region Bottom panel and overlays
void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, POOP_MAP_SIZE, POOP_X0 + i * POOP_DX, POOP_Y, POOP_S, false);
  }
}

// Gap between the label and its bar in the needs row. It was fixed at 48 px,
// exactly what 4 Latin letters take at scale 2, with no margin. Japanese labels
// are wider (4 kana, ~64 px) and ran into the bar. It is derived from the longest
// label, with 48 as a floor so nothing changes in the Latin-script languages.
// Measured once per language (the labels and the font only change with it).
// Requires text size 2 to already be set, because textW() needs it.
static int barLabelGap() {
  static int cachedForLanguage = -1;
  static int cachedGap = NEEDS_LABEL_GAP_FLOOR;
  if (cachedForLanguage == (int)gLang) return cachedGap;
  static const StrId needsLabelIds[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG };
  int widestLabelWidth = 0;
  for (StrId labelId : needsLabelIds) {
    int labelWidth = textW(T(labelId), NEEDS_LABEL_TEXT_SIZE);
    if (labelWidth > widestLabelWidth) widestLabelWidth = labelWidth;
  }
  // below the original threshold it stays EXACTLY 48, so nothing moves in the
  // Latin-script languages; above it, space is added so the label can breathe
  cachedGap = widestLabelWidth <= NEEDS_LABEL_GAP_FLOOR ? NEEDS_LABEL_GAP_FLOOR : widestLabelWidth + NEEDS_LABEL_GAP_MARGIN;
  cachedForLanguage = (int)gLang;
  return cachedGap;
}

static void drawBar(int x, int y, const char *label, uint8_t value) {
  gfx->setTextColor(inkColor());
  setSize(NEEDS_LABEL_TEXT_SIZE);
  setCur(x, y);
  printT(label);
  int labelGap = barLabelGap();
  // the two columns start at NEEDS_LEFT_X / NEEDS_RIGHT_X (boards/layout.h): the
  // first one's bar has to end NEEDS_COLUMN_SPACING before the second one's
  // label, and both are drawn the same width
  int barWidth = (NEEDS_RIGHT_X - NEEDS_LEFT_X) - labelGap - NEEDS_COLUMN_SPACING;
  if (barWidth > NEEDS_BAR_W_MAX) barWidth = NEEDS_BAR_W_MAX;
  if (barWidth < NEEDS_BAR_W_MIN) barWidth = NEEDS_BAR_W_MIN;  // floor: keep the bar from disappearing
  const int barHeight = NEEDS_BAR_H;
  int barLeft = x + labelGap;
  int barTop = y + (NEEDS_LABEL_LINE_HEIGHT - barHeight) / 2;  // the bar is centered on its label line
  uint16_t fillColor = (value >= NEEDS_OK_VALUE) ? UI_BAR_OK : (value >= NEEDS_WARN_VALUE) ? UI_BAR_WARN : UI_BAR_BAD;
  drawInsetProgressBar(barLeft, barTop, barWidth, barHeight, NEEDS_BAR_TRACK_RADIUS, NEEDS_BAR_FILL_RADIUS, value,
                       NEEDS_MAX_VALUE, fillColor);
}

void drawBars() {
  drawBar(NEEDS_LEFT_X, NEEDS_ROW1_Y, T(S_BAR_FOOD), pet.fullness);
  drawBar(NEEDS_RIGHT_X, NEEDS_ROW1_Y, T(S_BAR_JOY), pet.joy);
  drawBar(NEEDS_LEFT_X, NEEDS_ROW2_Y, T(S_BAR_ENE), pet.energy);
  drawBar(NEEDS_RIGHT_X, NEEDS_ROW2_Y, T(S_BAR_HYG), pet.hygiene);
}

void drawButtons() {
  for (int i = 0; i < (int)countof(buttons); i++) {
    bool off = pet.sleeping && i != MAIN_BUTTON_LIGHT;  // while asleep only LIGHT works
    int buttonLeft = buttons[i].cx - BTN_HALF, buttonTop = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(buttonLeft, buttonTop, 2 * BTN_HALF, 2 * BTN_HALF, ACTION_BUTTON_RADIUS, UI_WHITE);
    gfx->drawRoundRect(buttonLeft, buttonTop, 2 * BTN_HALF, 2 * BTN_HALF, ACTION_BUTTON_RADIUS, inkColor());
    if (!off) {
      const int iconHalf = ACTION_BUTTON_ICON_SIZE * ACTION_BUTTON_ICON_SCALE / 2;
      drawMap(buttons[i].icon, ACTION_BUTTON_ICON_SIZE, buttons[i].cx - iconHalf + buttons[i].iconOffsetX,
              buttons[i].cy - iconHalf + buttons[i].iconOffsetY, ACTION_BUTTON_ICON_SCALE, false);
    }
  }
}

// temporary banner: new medal or streak milestone
void drawCelebration() {
  const char *titleLine = nullptr, *detailLine = nullptr;
  // 32 and not 20: the Japanese streak text already exceeds 20 bytes with a single
  // digit, and snprintf cut in the middle of a 3-byte character.
  char buf[32];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { detailLine = medalName(i); break; }
    titleLine = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    titleLine = T(S_GREAT);
    detailLine = buf;
  }
  if (!titleLine) return;
  drawBorderedBox(CEL_X, CEL_Y, CEL_W, CEL_H, CEL_R, UI_BAR_WARN, UI_INK);
  gfx->setTextColor(UI_INK);
  ctext(titleLine, CELEBRATION_TITLE_SIZE, CEL_L1Y, CEL_W - CELEBRATION_SIDE_ROOM);
  ctext(detailLine, CELEBRATION_LINE_SIZE, CEL_L2Y, CEL_W - CELEBRATION_SIDE_ROOM);
}
#pragma endregion

#pragma region Header messages
const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // universal onomatopoeia
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < STATUS_LOW_NEED_VALUE) return T(S_HUNGRY);
  if (pet.hygiene < STATUS_LOW_NEED_VALUE) return T(S_NEEDS_BATH);
  if (pet.energy < STATUS_LOW_NEED_VALUE) return T(S_EXHAUSTED);
  if (pet.joy < STATUS_LOW_NEED_VALUE) return T(S_SAD);
  if (pet.weight > STATUS_CHUBBY_WEIGHT) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < STATUS_SHINY_NEWS_MINUTES) return T(S_IS_SHINY);
  return T(S_HAPPY);
}
#pragma endregion

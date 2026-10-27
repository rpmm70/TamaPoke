#pragma once

// Board-independent part of the layout: screen center, the scaling macros from
// the original 466x466 design, and every position that is the SAME formula on
// both boards (centered boxes, grids, mirrored right-edge elements). The per-
// board tables (layout_round_466.h, layout_rect_320x240.h) hold the sizes and
// hand-placed positions these formulas read. Include boards/layout.h, not this.

#pragma region Includes
#include "board_select.h"
#pragma endregion

#pragma region Screen center
#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)
#pragma endregion

#pragma region Scaling from the original 466x466 design
// Many coordinates in drawScene() etc. are hand-written for the original
// 466x466 canvas. SX()/SY() rescale them proportionally; on the Waveshare
// (466x466) they are an exact no-op.
#define LAYOUT_REFERENCE_SIZE 466
#define SX(px) (((int32_t)(px) * LCD_WIDTH) / LAYOUT_REFERENCE_SIZE)
#define SY(px) (((int32_t)(px) * LCD_HEIGHT) / LAYOUT_REFERENCE_SIZE)
// uniform scale (by height) for drawings that must keep their proportions
#define SK(v) (((int32_t)(v) * LCD_HEIGHT) / LAYOUT_REFERENCE_SIZE)
#define SKM(v) (SK(v) < 1 ? 1 : SK(v))
// SK() for fractional values (ball physics): same ratio, without integer truncation
#define SKF(v) ((float)(v) * (float)LCD_HEIGHT / (float)LAYOUT_REFERENCE_SIZE)
#pragma endregion

#pragma region Main screen: action buttons and the bottom panel
// a single #define with the board test inside: PlatformIO's .ino conversion
// (-fpreprocessed -dD) sees both #if branches and would warn about a
// redefinition. Same token sequence as the copy still in TamaPoke.ino.
#define BTN_HALF (BOARD_IS_ROUND ? 26 : 18)  // 52x52 button; smaller on the rect board (much shorter screen)
#define BTN_HIT 36   // touch radius (a bit more generous)
// bottom panel: lower third (y=312..466 of 466); SY() rescales it
#define BOTTOM_PANEL_Y SY(312)
#pragma endregion

#pragma region Sprite zoom limits
#define GAME_PET_MAX_SCALE 3                  // ball game creature
#define PMD_MIN_SCALE (BOARD_IS_ROUND ? 2 : 1)  // at 320x240 a x2 zoom is already half the screen
#pragma endregion

#pragma region Header and pickers: centered from their widths
#define POKEDEX_BTN_X (HAMBURGER_X + HAMBURGER_W / 2 - 8 * POKEDEX_BTN_S)
#define FEED_W (FEED_ITEM_COUNT * FEED_ITEM)
#define FEED_X (CX - FEED_W / 2)
#define PLAY_MENU_W (PLAY_MENU_ITEM_COUNT * PLAY_MENU_ITEM)
#define PLAY_MENU_X (CX - PLAY_MENU_W / 2)
#define STARTER_ROW_X (CX - STARTER_ROW_W / 2)
#define LIFE_X0 (CX - LIFE_DX)
#pragma endregion

#pragma region Dialogs and CTA buttons: centered on CX
#define CONF_X (CX - CONF_W / 2)
#define CH_X (CX - CH_W / 2)
#define CH_BX (CX - CH_BW / 2)
#define CEL_X (CX - CEL_W / 2)
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#pragma endregion

#pragma region Settings screens: derived and board-independent values
// horizontal padding kept free on each side of a label inside its box
#define SETTINGS_TEXT_SIDE_PAD 3
// left edge of the section label box, centered on CX
#define SETTINGS_TITLE_X (CX - SETTINGS_TITLE_MAX_W / 2)
#define SETTINGS_MENU_ROW_X (CX - SETTINGS_MENU_ROW_W / 2)
#define SETTINGS_SOUND_PILL_X (CX - SETTINGS_SOUND_PILL_W / 2)
#define SETTINGS_BRIGHTNESS_BAR_X (CX - SETTINGS_BRIGHTNESS_BAR_W / 2)
// the + button mirrors the - button on the right edge (both CLK_BTN x CLK_BTN_H)
#define SETTINGS_BRIGHTNESS_PLUS_X (LCD_WIDTH - SETTINGS_BRIGHTNESS_MINUS_X - CLK_BTN)
// language grid: 2 columns centered on the screen
#define SETTINGS_LANG_CELL_X0 ((LCD_WIDTH - (SETTINGS_LANG_CELL_COL_STEP + SETTINGS_LANG_CELL_W)) / 2)
// WiFi "page/pages" text: vertically centered between the two page arrows (8 px per text size unit)
#define WIFI_PAGE_TEXT_Y ((WIFI_ARROW_UP_CY + WIFI_ARROW_DOWN_CY) / 2 - 4 * WIFI_PAGE_TEXT_SIZE)
#pragma endregion

#pragma region Time setting: the -/+ pair centered on CX, OK button
#define CLK_MINUS_X (CX - CLK_STEP_GAP / 2 - CLK_BTN)
#define CLK_PLUS_X (CX + CLK_STEP_GAP / 2)
// OK button centered, its "OK" label (TS(3)) centered inside it
#define CLK_OK_X (CX - CLK_OK_W / 2)
#define CLK_OK_TX (6 * TS(3))                      // half of "OK": 2 chars * 6 px * size / 2
#define CLK_OK_TY (CLK_OK_Y + (CLK_OK_H - 8 * TS(3)) / 2)
#pragma endregion

#pragma region Info card: arrows mirrored on the right edge, medals grid, stat rows width
#define CARD_ARROW_RIGHT_CX (LCD_WIDTH - CARD_ARROW_LEFT_CX)
#define CARD_ARROW_CY CY
// medals: 2 columns centered on the screen
#define MED_X0 ((LCD_WIDTH - (MED_COLSTEP + MED_W)) / 2)
#define CARD_PROFILE_STATS_W (CARD_PROFILE_STATS_RIGHT - CARD_PROFILE_STATS_LEFT)
#pragma endregion

#pragma region Pokedex gallery: grid size and the filter column left of the grid
#define GALLERY_COLS 4
#define GALLERY_ROWS 4
#define GALLERY_PAGE_SIZE (GALLERY_COLS * GALLERY_ROWS)
// 4 stacked filter buttons (All / Raised / Caught / Shiny), vertically centered on
// the grid, ending GALLERY_FILTER_GRID_GAP px left of it
#define GALLERY_FILTER_GRID_GAP 6
#define GALLERY_FILTER_STACK_H (4 * GALLERY_FILTER_H + 3 * GALLERY_FILTER_GAP)
#define GALLERY_FILTER_X (GALLERY_GRID_X - GALLERY_FILTER_GRID_GAP - GALLERY_FILTER_W)
#define GALLERY_FILTER_Y (GALLERY_GRID_Y + (GALLERY_ROWS * GALLERY_CELL - GALLERY_FILTER_STACK_H) / 2)
#pragma endregion

#pragma region Keyboard: 7 key columns centered on the screen
#define KB_X ((LCD_WIDTH - 7 * KB_W) / 2)
#pragma endregion

#pragma region Wild encounter prompt: box and buttons centered on CX
#define WILD_PROMPT_X (CX - WILD_PROMPT_W / 2)
#define WILD_PROMPT_BTN_X (CX - WILD_PROMPT_BTN_W / 2)
#pragma endregion

#pragma region Wild battle: wild column mirrors the pet column on the right edge
#define BATTLE_WILD_HP_X (LCD_WIDTH - BATTLE_PET_HP_X - BATTLE_HP_W)
#define BATTLE_WILD_CHIP_RIGHT (LCD_WIDTH - BATTLE_PET_CHIP_X)
#pragma endregion

#pragma region Wild battle: Attack / Dodge / Rest columns (Dodge centered, Rest mirrors Attack)
#define BATTLE_DODGE_X (CX - BATTLE_ACTION_W / 2)
#define BATTLE_REST_X (LCD_WIDTH - BATTLE_ATTACK_X - BATTLE_ACTION_W)
// the Quick / Attack / Heavy overlay sits in the same columns as the action buttons
#define BATTLE_QUICK_X BATTLE_ATTACK_X
#define BATTLE_NORMAL_X BATTLE_DODGE_X
#define BATTLE_HEAVY_X BATTLE_REST_X
#pragma endregion

#pragma region Wild battle result: Leave mirrors Catch, OK centered
#define BATTLE_LEAVE_X (LCD_WIDTH - BATTLE_CATCH_X - BATTLE_CATCH_W)
#define BATTLE_OK_X (CX - BATTLE_OK_W / 2)
#pragma endregion

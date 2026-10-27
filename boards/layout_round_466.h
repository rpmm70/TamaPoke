#pragma once

// Layout table for the round 466x466 board: the original's literal values.
// Menus and dialogs: ONE layout definition shared by drawing and touch
// detection (if a table changes, both change at once). Positions that are the
// same formula on both boards live in layout_common.h. Include boards/layout.h, not this.

#pragma region Includes
#include "layout_common.h"
#pragma endregion

#pragma region Screen size this table was designed for
static_assert(LCD_WIDTH == 466 && LCD_HEIGHT == 466, "layout_round_466.h is designed for a 466x466 screen");
static_assert(LCD_WIDTH == LAYOUT_REFERENCE_SIZE, "on the round board SX()/SY() must be a no-op");
#pragma endregion

#pragma region Scene geometry: horizon, bezel, minigame arena
// replace the original's CX/CY/HORIZON literals (233/233/232)
#define HORIZON 232
// full-screen background circle: the bezel's radius
#define BEZEL_RADIUS (LCD_WIDTH / 2 - 2)
// Minigame: the ball bounces off the bezel's circular wall
#define ARENA_WALL_R 205
#define ARENA_GROUND_Y 384
#pragma endregion

#pragma region Text size: identity
#define TS(n) (n)
#pragma endregion

#pragma region Header and battery
#define HDR_NAME_Y 52
#define HDR_MSG_Y 90
#define BAT_X (CX - 14)
#define BAT_Y 12
#pragma endregion

#pragma region Main screen header: hamburger (top left), clock (top right), streak badge under the clock, level row
// Everything stays inside the bezel circle (r 233): the farthest corners, the
// hamburger's top-left (68, 96) and the streak's top-right (398, 96), are 214 px from the center.
// The streak badge is right-justified on STREAK_RIGHT, mirroring the hamburger.
#define STREAK_RIGHT 398
#define STREAK_Y 96
#define HAMBURGER_X 68
#define HAMBURGER_Y 96
#define HAMBURGER_W 30
#define HAMBURGER_H 20
#define HAMBURGER_HIT_R 32
// Pokedex button: 16x16 icon at x2 under the hamburger, same center x (83);
// its bottom-left corner (67, 164) is 180 px from the center
#define POKEDEX_BTN_S 2
#define POKEDEX_BTN_Y 132
#define POKEDEX_BTN_HIT_R 26
#pragma region Sleep bubble: "Zzz" speech bubble by the head of the sleeping creature (drawSleepBubble)
#define SLEEP_BUBBLE_TEXT_SIZE 2
#define SLEEP_BUBBLE_PAD_X 8
#define SLEEP_BUBBLE_PAD_Y 6
#define SLEEP_BUBBLE_R 10
#define SLEEP_BUBBLE_TAIL 10        // tail base width and its drop to the head
#define SLEEP_BUBBLE_GAP 2          // bubble left edge past the anchor point
#define SLEEP_BUBBLE_HEAD_INSET 10  // anchor this far below the top of the sprite box
#define SLEEP_BUBBLE_MAX_RIGHT 400  // stays inside the bezel at head height
#define SLEEP_BUBBLE_MIN_TOP 120    // below the header and the level row
#pragma endregion
#define HDR_NAME_MAX_W (LCD_WIDTH - 80)
// clock: digits at size 3 (y 26..50, just above the name), AM/PM at size 2;
// its top-right corner (330, 26) is 229 px from the center
#define HDR_TIME_RIGHT 330
#define HDR_TIME_Y 26
#define HDR_TIME_SIZE 3
#define HDR_TIME_MERIDIEM_GAP 4
#define HDR_LVL_BAR_W 120
#define HDR_LVL_BAR_H 12
#define HDR_LVL_GAP 10
#define HDR_LVL_TEXT_SIZE 1  // "Lv.N" next to the level bar
#pragma endregion

#pragma region Starter choice
#define STARTER_TITLE_Y 68
#define STARTER_ROW_W 326
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
#define STARTER_NAME_DX 108
#define STARTER_NAME_DY 24
#define STARTER_TH_X 76
#define STARTER_TH_DY (-5)
#define STARTER_TH_CELL 80
#define STARTER_TH_S 3
#define STARTER_R 14
#pragma endregion

#pragma region Food picker: apple / hamburger / pizza / candy / coffee; corners stay inside the bezel (farthest 203 px)
#define FEED_ITEM 66
#define FEED_ITEM_COUNT 5
#define FEED_Y 288
#define FEED_H 64
#define FEED_ICON_S 3
#define FEED_R 14
#pragma endregion

#pragma region "Release?" dialog
#define CONF_Y 168
#define CONF_W 278
#define CONF_H 152
#define CONF_R 16
#define CONF_QY 28
#define CONF_BTN_REL_Y 84
#define CONF_BTN_H 52
#define CONF_BTN_W 100
#define CONF_BTN_MARGIN 24
#define CONF_BTN_TY 18
#pragma endregion

#pragma region Decision dialog (evolution / farewell)
#define CH_Y 156
#define CH_W 320
#define CH_H 188
#define CH_R 16
#define CH_QY 20
#define CH_BW 280
#define CH_B1_REL_Y 50
#define CH_B2_REL_Y 112
#define CH_BH 52
#define CH_BTY 18
#pragma endregion

#pragma region CTA buttons (evolve / farewell / runaway)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_Y 172
#define EVO_R 18
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_Y 176
#define FAR_R 16
#pragma endregion

#pragma region Settings screens: section label box width (titles: drawScreenTitle(), CARD_PROGRESS_TITLE_*)
#define SETTINGS_TITLE_MAX_W 260
#pragma endregion

#pragma region Settings menu: four rows (y 126-380, inside the circle) and the version line
// Row 0 starts below the TS(5) title (y 58..98) and the close button's hit circle
// (ends at y 122); the farthest row corner, (73, 380), is 217 px from the center.
#define SETTINGS_MENU_ROW_W 320
#define SETTINGS_MENU_ROW_Y 126
#define SETTINGS_MENU_ROW_H 56
#define SETTINGS_MENU_ROW_GAP 10
#define SETTINGS_MENU_ROW_R 14
#define SETTINGS_MENU_ARROW_INSET 22
#define SETTINGS_MENU_ARROW_SIZE 16
#define SETTINGS_MENU_VERSION_Y 436
#pragma endregion

#pragma region Settings menu row 0: WiFi (left) and Screen Off (right) buttons, icon then label
#define SETTINGS_MENU_SPLIT_GAP 10
#define SETTINGS_MENU_ICON_SIZE 24
#define SETTINGS_MENU_ICON_INSET 12
#pragma endregion

#pragma region WiFi screen: On/Off pill and Scan button (y 126-172), status line, 4 network rows, page arrows
// Rows x 76..356, y 206..394 (farthest corner (76, 394) is 225 px from the center);
// the page arrows column is centered on x 384 (its farthest point is 205 px from the center).
#define WIFI_TOGGLE_X 93
#define WIFI_TOGGLE_W 130
#define WIFI_SCAN_X 243
#define WIFI_SCAN_W 130
#define WIFI_BUTTON_Y 126
#define WIFI_BUTTON_H 46
#define WIFI_BUTTON_R 14
#define WIFI_STATUS_Y 182
#define WIFI_STATUS_SIZE 2
#define WIFI_STATUS_MAX_W 300
#define WIFI_LIST_X 76
#define WIFI_LIST_W 280
#define WIFI_LIST_Y 206
#define WIFI_ROW_H 44
#define WIFI_ROW_STEP 48
#define WIFI_ROWS_PER_PAGE 4
#define WIFI_ROW_R 10
#define WIFI_ROW_PAD 8
#define WIFI_ROW_ICON_SIZE 18
#define WIFI_ROW_SSID_SIZE 2
#define WIFI_ROW_SSID_DY 5
#define WIFI_ROW_DETAIL_SIZE 2
#define WIFI_ROW_DETAIL_DY 24
#define WIFI_ROW_CHECK_SIZE 20
#define WIFI_ARROW_CX 384
#define WIFI_ARROW_SIZE 18
#define WIFI_ARROW_UP_CY 228
#define WIFI_ARROW_DOWN_CY 372
#define WIFI_ARROW_HIT_W 50
#define WIFI_ARROW_HIT_H 70
#define WIFI_PAGE_TEXT_SIZE 2
#pragma endregion

#pragma region Audio / video: sound pill, brightness -/+ buttons (CLK_BTN squares) and bar
// Under the TS(5) title (y 58..98): label 122..146 (below the close button's hit
// circle, which ends at y 122), pill 158..210, label 240..264, -/+ row 276..334
// (its farthest corners, (63, 334) and (403, 334), are 198 px from the center)
#define SETTINGS_SOUND_LABEL_Y 122
#define SETTINGS_SOUND_PILL_Y 158
#define SETTINGS_SOUND_PILL_W 140
#define SETTINGS_SOUND_PILL_H 52
#define SETTINGS_PILL_R 14
#define SETTINGS_BRIGHTNESS_LABEL_Y 240
#define SETTINGS_BRIGHTNESS_ROW_Y 276
#define SETTINGS_BRIGHTNESS_MINUS_X 63
#define SETTINGS_BRIGHTNESS_BAR_W 192
#define SETTINGS_BRIGHTNESS_BAR_H 24
#pragma endregion

#pragma region Language list: 2 columns x 4 rows (y 126-372, corners inside the circle)
// Below the TS(5) title (y 58..98) and the close button's hit circle (ends at y 122);
// the farthest corners, (55, 372) and (411, 372), are 226 px from the center.
#define SETTINGS_LANG_CELL_COL_STEP 188
#define SETTINGS_LANG_CELL_W 168
#define SETTINGS_LANG_CELL_Y0 126
#define SETTINGS_LANG_CELL_ROW_STEP 64
#define SETTINGS_LANG_CELL_H 54
#define SETTINGS_LANG_CELL_R 12
#pragma endregion

#pragma region Time setting: digits, -/+ buttons, HOUR/MIN labels, toggles, OK
// under the TS(5) title (y 58..98): digits 120..168, -/+ 190..248,
// HOUR/MIN 256..272, toggles 290..334, OK 352..400 (its bottom corners are 195 px from the center)
#define CLK_DIG_Y 120
#define CLK_DIG_SIZE 6   // one size under the old TS(7)
#define CLK_BTN 58
#define CLK_BTN_Y 190
#define CLK_HM_X 104
#define CLK_HP_X 170
#define CLK_MM_X 252
#define CLK_MP_X 318
#define CLK_BTN_R 12
#define CLK_GLYPH_DX 17
#define CLK_GLYPH_DY 15
#define CLK_LBL_Y 256
#define CLK_MODE_Y 290
#define CLK_MODE_H 44
#define CLK_MODE_R 12
#define CLK_OK_Y 352
#define CLK_OK_W 200
#define CLK_OK_H 48
#define CLK_OK_R 14
#pragma endregion

#pragma region Medal / streak notice
#define CEL_Y 150
#define CEL_W 320
#define CEL_H 96
#define CEL_R 16
#define CEL_L1Y 176
#define CEL_L2Y 212
#pragma endregion

#pragma region Info card: medals, name and stats
#define CARD_HEAD_MAXW 198
#pragma endregion

#pragma region Shared close button (top-right; inside the bezel circle: 191 px from the center + radius 20)
#define CLOSE_BTN_CX 360
#define CLOSE_BTN_CY 90
#define CLOSE_BTN_R 20
#define CLOSE_BTN_HIT_R 32
#pragma endregion

#pragma region Info card: page arrows, page dots, rename pencil, main-screen name hit box
#define CARD_ARROW_LEFT_CX 17
#define CARD_ARROW_SIZE 18
#define CARD_ARROW_HIT_W 56
#define CARD_ARROW_HIT_H 90
#define CARD_SIDE_MARGIN 30
#define CARD_DOTS_Y 428
#define CARD_DOT_STEP 26
#define CARD_PENCIL_GAP 6
#define CARD_PENCIL_MIN_SIZE 16
#define CARD_PENCIL_HIT_PAD 14
#define HDR_NAME_HIT_PAD 12
#pragma endregion

#pragma region Info card page 0: portrait column (left) and five stat rows (right)
// Portrait box x 50..194, y 130..320, nudged 9 px left of the centre (x 131) of the
// free space between the left arrow hit box (x < 56) and the stat rows (x >= 206);
// its top-left corner is 210 px from the center, inside the 233 px bezel.
#define CARD_PROFILE_PORTRAIT_X 50
#define CARD_PROFILE_PORTRAIT_Y 130
#define CARD_PROFILE_PORTRAIT_W 144
#define CARD_PROFILE_PORTRAIT_H 190
#define CARD_PROFILE_PORTRAIT_MAX_SCALE 4
// Status message under the portrait: up to two lines of this text size from
// y 328 (the farthest corner, (50, 362), sits 224 px from the center)
#define CARD_PROFILE_STATUS_Y 328
#define CARD_PROFILE_STATUS_SIZE 2
// Rows x 206..404 (right arrow hit box starts at 410), y 104..370: the close
// button's hit circle ends at y 122 but only over x 328..392, where the first
// row holds a short right-justified value (y >= 130); the farthest corner
// (404, 370) sits 219 px from the center, and the page dots are at y 428.
#define CARD_PROFILE_STATS_LEFT 206
#define CARD_PROFILE_STATS_RIGHT 404
#define CARD_PROFILE_ROW_Y0 124
#define CARD_PROFILE_ROW_STEP 50
#define CARD_PROFILE_LABEL_SIZE 2
#define CARD_PROFILE_VALUE_SIZE 3
#define CARD_PROFILE_VALUE_GAP 4
#define CARD_PROFILE_BAR_H 12
#define CARD_PROFILE_BAR_GAP 8
// Nickname's "(SPECIES)" line, this far under the name (TS(5) name: y 58..98,
// subtitle y 104..120, above the stat rows at y 124 and the portrait at y 130)
#define CARD_PROFILE_SUBTITLE_SIZE 2
#define CARD_PROFILE_SUBTITLE_GAP 6
#pragma endregion

#pragma region Info card page 1 (stats): type chips under the title (y 58-98), then 7 rows of text
// Chips y 104..130; rows HP..LOSSES y 144..388 (row corners stay inside the circle), above the page dots (y 428)
#define CARD_STATS_TYPES_Y 104
#define CARD_STATS_ROW_Y0 144
#define CARD_STATS_ROW_STEP 38
#pragma endregion

#pragma region Info card page 3 (progress): vertical rhythm, tops of each text line / the bar
// Spread evenly from the title (y 58) to the last line (ends y 395), above the page dots (y 428)
#define CARD_PROGRESS_TITLE_Y 58
#define CARD_PROGRESS_TITLE_MAXW 300
#define CARD_PROGRESS_LEVEL_Y 112
#define CARD_PROGRESS_BAR_Y 187
#define CARD_PROGRESS_NEXT_GAP 32
#define CARD_PROGRESS_EVO_LABEL_Y 286
#define CARD_PROGRESS_EVO_VALUE_Y 312
#define CARD_PROGRESS_MISTAKES_Y 379
#pragma endregion

#pragma region Pokedex gallery: title, R/C counts, filter column, 4x4 grid below the hit circle of the close button (y <= 122)
// Grid x 148..404 (pushed right for the filters; the right arrow hit box starts at 410),
// y 124..380; thumbnails (40 px) are drawn at 3/2 scale (60 px). Its farthest corner is 225 px from the center.
#define GALLERY_CELL 64
#define GALLERY_GRID_X 148
#define GALLERY_GRID_Y 124
#define GALLERY_TITLE_Y 36
#define GALLERY_TITLE_SIZE 3
#define GALLERY_COUNTS_Y 70
#define GALLERY_COUNTS_SIZE 2
// filters: 4 stacked buttons x 62..142, y 169..335 (centered on the grid), right of the
// left arrow hit box (x < 56); farthest corner (62, 335) is 199 px from the center
#define GALLERY_FILTER_W 80
#define GALLERY_FILTER_H 34
#define GALLERY_FILTER_GAP 10
#define GALLERY_FILTER_R 10
#define GALLERY_FILTER_TEXT_SIZE 2
#define GALLERY_THUMB_SCALE_NUM 3
#define GALLERY_THUMB_SCALE_DEN 2
// "C" marker on a cell: species caught in a wild battle but never raised
#define GALLERY_CAUGHT_MARK_SIZE 2
// Detail header ends left of the close button (x 340)
#define GALLERY_DETAIL_HEADER_MAXW 190
// Detail type chips, centered under the header (header ends y 80, chips y 100..126)
#define GALLERY_DETAIL_TYPES_Y 100
#define GALLERY_TYPE_CHIP_TEXT_SIZE 2
#define GALLERY_TYPE_CHIP_H 26
#define GALLERY_TYPE_CHIP_PAD 10
#define GALLERY_TYPE_CHIP_R 8
#define GALLERY_TYPE_CHIP_GAP 8
#pragma endregion

#pragma region Info card page 2 (medals): 2x4 grid x 40-426, y 124-362 under the title (y 58-98)
// Top row starts below the close button's hit circle (y <= 122); the bottom row's
// corners (40, 362) and (426, 362) sit 232 px from the center, inside the 233 px bezel.
#define MED_COLSTEP 196
#define MED_W 190
#define MED_Y0 124
#define MED_STEP 64
#define MED_H 46
#define MED_MARK_CX 22
#define MED_MARK_R 11
#define MED_TXT_X 44
#define BAR_H_CARD 22
#pragma endregion

#pragma region Keyboard: 7 x 5 keys (x 44-422, y 150-410), text box left of the close button (x < 340)
#define KB_Y 150
#define KB_W 54
#define KB_H 52
#define KB_TITLE_Y 56
#define KB_BOX_X 83
#define KB_BOX_Y 84
#define KB_BOX_W 250
#define KB_BOX_H 40
#define KB_BOX_R 8
#define KB_BOX_TX 95
#define KB_BOX_TY 94
#pragma endregion

#pragma region Poop row
#define POOP_S 2
#define POOP_X0 34
#define POOP_DX 48
#define POOP_Y 244
#pragma endregion

#pragma region Main screen needs bars (4 bars, 2 columns x 2 rows, bottom panel y 312-466)
// Round: bottom row ends at y 361, inside the circle (chord x 38..428 at y 360);
// the right bar ends at x 422. The action buttons start at y 364.
#define NEEDS_LEFT_X 60
#define NEEDS_RIGHT_X 262
#define NEEDS_ROW1_Y 317
#define NEEDS_ROW2_Y 343
#define NEEDS_BAR_H 18
#define NEEDS_BAR_W_MAX 112
#define NEEDS_BAR_W_MIN 40
#define NEEDS_COLUMN_SPACING 42
#pragma endregion

#pragma region Ball game top: record line, then the lives (three dots, the middle one on CX)
#define BALL_RECORD_Y 36
#define LIFE_DX 28
#define LIFE_Y 64
#define LIFE_R 6
#pragma endregion

#pragma region Minigame score line (bottom, centered): ball game and coin chase
#define MINIGAME_SCORE_Y 404
#define MINIGAME_SCORE_TEXT_SIZE 2
#define MINIGAME_SCORE_MAX_W 260
#pragma endregion

#pragma region Ball game: ball, creature, heart
#define BALL_S 3
#define BALL_OFF 24
#define GAME_PET_Y (MINIGAME_SCORE_Y - 10)  // ball game: feet just above the score line
#define PET_HEART_SCALE 2  // "likes it" heart over the creature (32 px bitmap)
#pragma endregion

#pragma region Ball game: smallest touch radius around the ball (the radii from the fork are used as is)
#define BALL_HIT_RADIUS_MIN 0
#pragma endregion

#pragma region Play picker: ball / coin / glove / swords strip (food picker style, 4 items, centered under the play button)
#define PLAY_MENU_ITEM 66
#define PLAY_MENU_ITEM_COUNT 4
#define PLAY_MENU_Y 288
#define PLAY_MENU_H 64
#define PLAY_MENU_ICON_S 3
#define PLAY_MENU_R 14
#pragma endregion

#pragma region Coin chase: timer (top middle), coin and sprite sizes, play area inside the bezel
#define COIN_TIMER_Y 40
#define COIN_TIMER_TEXT_SIZE 4
#define COIN_RADIUS 14
#define COIN_PET_MAX_SCALE 2
#define COIN_PET_SIZE_PERCENT 125  // x2 zoom grown to x2.5
#define COIN_PET_BODY_OFFSET 45    // body center sits this far above the feet
#define COIN_PLAY_RADIUS 170       // body center stays inside this circle around (CX, CY)
#define COIN_PLAY_TOP 92           // below the timer and the close button
#define COIN_PLAY_BOTTOM 384       // above the score line
#define COIN_SPAWN_MIN_DISTANCE 120
#pragma endregion

#pragma region Wild encounter prompt: question, species line, FIGHT / LATER buttons (literals from the fork)
#define WILD_PROMPT_Y 156
#define WILD_PROMPT_W 302
#define WILD_PROMPT_H 178
#define WILD_PROMPT_R 18
#define WILD_PROMPT_TITLE_Y 176
#define WILD_PROMPT_TITLE_SIZE 3
#define WILD_PROMPT_NAME_Y 206
#define WILD_PROMPT_NAME_SIZE 2
#define WILD_PROMPT_BTN_W 280
#define WILD_PROMPT_BTN_H 44
#define WILD_PROMPT_BTN_R 12
#define WILD_PROMPT_BTN_TEXT_SIZE 2
#define WILD_PROMPT_FIGHT_Y 226
#define WILD_PROMPT_LATER_Y 278
#pragma endregion

#pragma region Wild battle: title, two info columns, run button (literals from the fork)
#define BATTLE_TITLE_Y 34
#define BATTLE_TITLE_SIZE 3
#define BATTLE_TITLE_MAX_W 300
#define BATTLE_NAME_Y 82
#define BATTLE_NAME_SIZE 2
#define BATTLE_NAME_MAX_W 160
#define BATTLE_PET_NAME_X 28
#define BATTLE_WILD_NAME_RIGHT 408
#define BATTLE_HP_Y 110
#define BATTLE_HP_W 150
#define BATTLE_HP_H 14
#define BATTLE_HP_R 4
#define BATTLE_PET_HP_X 28
#define BATTLE_CHIP_Y 130
#define BATTLE_CHIP_H 16
#define BATTLE_CHIP_R 5
#define BATTLE_CHIP_PAD 7
#define BATTLE_CHIP_GAP 4
#define BATTLE_PET_CHIP_X 28
// caught marker: pokeball icon left of the wild type chips
#define BATTLE_CAUGHT_DX 36
#define BATTLE_CAUGHT_Y 122
#define BATTLE_CAUGHT_S 2
#define BATTLE_RUN_X 188
#define BATTLE_RUN_Y 102
#define BATTLE_RUN_W 90
#define BATTLE_RUN_H 32
#define BATTLE_RUN_R 9
#define BATTLE_RUN_HIT_X 184
#define BATTLE_RUN_HIT_Y 100
#define BATTLE_RUN_HIT_W 98
#define BATTLE_RUN_HIT_H 36
#pragma endregion

#pragma region Wild battle: sprites (feet on BATTLE_GROUND_Y) and the thumbnail fallback
#define BATTLE_PET_CX 142
#define BATTLE_WILD_CX 328
#define BATTLE_GROUND_Y 272
#define BATTLE_SPRITE_MAX_S 3
#define BATTLE_PET_THUMB_X 94
#define BATTLE_WILD_THUMB_X 280
#define BATTLE_THUMB_Y 152
#define BATTLE_THUMB_S 3
#define BATTLE_THUMB_CELL 64
#pragma endregion

#pragma region Wild battle: moves of the last turn, one line centered under each sprite (y 318-334)
// boxes x 52..232 and 238..418, inside the circle at that height (chord x 16..450)
#define BATTLE_MOVE_Y 318
#define BATTLE_MOVE_SIZE 2
#define BATTLE_MOVE_W 180
#pragma endregion

#pragma region Wild battle: Attack / Dodge / Rest buttons and the Quick / Attack / Heavy overlay
// hit boxes = the drawn box grown by the *_HIT_PAD_* margins (the fork's generous areas);
// the overlay buttons sit in the same columns as the action buttons below them (layout_common.h)
#define BATTLE_ACTION_Y 358
#define BATTLE_ACTION_W 108
#define BATTLE_ACTION_H 58
#define BATTLE_ACTION_R 13
#define BATTLE_ATTACK_X 58
#define BATTLE_ACTION_HIT_PAD_X 12
#define BATTLE_ACTION_HIT_PAD_Y 14
// Quick / Normal / Heavy overlay: just above the Attack / Dodge / Rest row
#define BATTLE_ATTACK_MENU_GAP 8
#define BATTLE_ATTACK_MENU_Y (BATTLE_ACTION_Y - BATTLE_ATTACK_MENU_GAP - BATTLE_ATTACK_MENU_H)
#define BATTLE_ATTACK_MENU_W 108
#define BATTLE_ATTACK_MENU_H 46
#define BATTLE_ATTACK_MENU_R 12
#define BATTLE_ATTACK_MENU_HIT_PAD_X 6
#define BATTLE_ATTACK_MENU_HIT_PAD_Y 6
#define BATTLE_BTN_TEXT_SIZE 2
#pragma endregion

#pragma region Wild battle result: WIN / LOSS, rounds and damage, reward line, Catch / Leave or OK
// no panel behind the text on the round board (BATTLE_RESULT_PANEL_W 0)
#define BATTLE_RESULT_PANEL_X 0
#define BATTLE_RESULT_PANEL_Y 0
#define BATTLE_RESULT_PANEL_W 0
#define BATTLE_RESULT_PANEL_H 0
#define BATTLE_RESULT_PANEL_R 0
#define BATTLE_RESULT_TEXT_MAX_W 300
#define BATTLE_RESULT_TITLE_Y 300
#define BATTLE_RESULT_TITLE_SIZE 4
#define BATTLE_RESULT_ROUNDS_Y 334
#define BATTLE_RESULT_DAMAGE_Y 356
#define BATTLE_RESULT_LINE_SIZE 2
#define BATTLE_RESULT_INFO_Y 378
#define BATTLE_RESULT_INFO_SIZE 2
#define BATTLE_RESULT_BTN_Y 396
#define BATTLE_RESULT_BTN_H 52
#define BATTLE_RESULT_BTN_R 14
#define BATTLE_CATCH_X 76
#define BATTLE_CATCH_W 148
#define BATTLE_OK_W 230
#define BATTLE_OK_TEXT_SIZE 3
#define BATTLE_RESULT_HIT_PAD_Y 4
#pragma endregion

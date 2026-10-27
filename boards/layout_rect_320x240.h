#pragma once

// Layout table for the rectangular 320x240 board: a dedicated redesign with
// smaller boxes, smaller text sizes and without the slack the circle needed.
// Menus and dialogs: ONE layout definition shared by drawing and touch
// detection (if a table changes, both change at once). Positions that are the
// same formula on both boards live in layout_common.h. Include boards/layout.h, not this.

#pragma region Includes
#include "layout_common.h"
#pragma endregion

#pragma region Screen size this table was designed for
static_assert(LCD_WIDTH == 320 && LCD_HEIGHT == 240, "layout_rect_320x240.h is designed for a 320x240 screen");
#pragma endregion

#pragma region Scene geometry: horizon, minigame arena
#define HORIZON ((LCD_HEIGHT * 62) / 100)
// No circular wall in a rectangle: bounces off left/right/ceiling
#define ARENA_LEFT 10
#define ARENA_RIGHT (LCD_WIDTH - 10)
#define ARENA_TOP 4
#define ARENA_GROUND_Y (LCD_HEIGHT - 4)
#pragma endregion

#pragma region Text size: 7->5, 6/5/4->3, 3->2, rest unchanged
#define TS(n) ((n) >= 7 ? 5 : (n) >= 4 ? 3 : (n) == 3 ? 2 : (n))
#pragma endregion

#pragma region Header name and message
#define HDR_NAME_Y 6
#define HDR_MSG_Y 26
#pragma endregion

#pragma region Main screen header: hamburger (top left), clock (top right), streak badge under the clock, level row
// The battery (usually absent on the CYD) sits under the clock at the top right,
// and the streak badge under both, right-justified on STREAK_RIGHT.
#define BAT_X (LCD_WIDTH - 36)
#define BAT_Y 23
#define STREAK_RIGHT (LCD_WIDTH - 4)
#define STREAK_Y 38
#define HAMBURGER_X 6
#define HAMBURGER_Y 5
#define HAMBURGER_W 24
#define HAMBURGER_H 17
#define HAMBURGER_HIT_R 26
// Pokedex button: 16x16 icon at x2 under the hamburger, same center x (18), y 30..62
#define POKEDEX_BTN_S 2
#define POKEDEX_BTN_Y 30
#define POKEDEX_BTN_HIT_R 20
#pragma region Sleep bubble: "Zzz" speech bubble by the head of the sleeping creature (drawSleepBubble)
#define SLEEP_BUBBLE_TEXT_SIZE 2
#define SLEEP_BUBBLE_PAD_X 5
#define SLEEP_BUBBLE_PAD_Y 4
#define SLEEP_BUBBLE_R 6
#define SLEEP_BUBBLE_TAIL 6         // tail base width and its drop to the head
#define SLEEP_BUBBLE_GAP 1          // bubble left edge past the anchor point
#define SLEEP_BUBBLE_HEAD_INSET 6   // anchor this far below the top of the sprite box
#define SLEEP_BUBBLE_MAX_RIGHT (LCD_WIDTH - 4)
#define SLEEP_BUBBLE_MIN_TOP 44     // below the header, the level row and the streak badge
#pragma endregion
// name x 82..238, so it clears the widest clock ("12:59 PM": x 240..316)
#define HDR_NAME_MAX_W 156
// clock: digits at size 2 (y 4..20), AM/PM at size 1
#define HDR_TIME_RIGHT (LCD_WIDTH - 4)
#define HDR_TIME_Y 4
#define HDR_TIME_SIZE 2
#define HDR_TIME_MERIDIEM_GAP 2
#define HDR_LVL_BAR_W 100
#define HDR_LVL_BAR_H 10
#define HDR_LVL_GAP 8
#define HDR_LVL_TEXT_SIZE 1  // "Lv.N" next to the level bar
#pragma endregion

#pragma region Starter choice
#define STARTER_TITLE_Y 10
#define STARTER_ROW_W 280
#define STARTER_ROW_Y 36
#define STARTER_ROW_H 56
#define STARTER_ROW_GAP 6
#define STARTER_NAME_DX 68
#define STARTER_NAME_DY 20
#define STARTER_TH_X 24
#define STARTER_TH_DY 0
#define STARTER_TH_CELL 56
#define STARTER_TH_S 2
#define STARTER_R 10
#pragma endregion

#pragma region Food picker: apple / hamburger / pizza / candy / coffee (x 15-305)
#define FEED_ITEM 58
#define FEED_ITEM_COUNT 5
#define FEED_Y 150
#define FEED_H 44
#define FEED_ICON_S 2
#define FEED_R 10
#pragma endregion

#pragma region "Release?" dialog
#define CONF_Y 60
#define CONF_W 280
#define CONF_H 104
#define CONF_R 12
#define CONF_QY 12
#define CONF_BTN_REL_Y 60
#define CONF_BTN_H 34
#define CONF_BTN_W 100
#define CONF_BTN_MARGIN 28
#define CONF_BTN_TY 9
#pragma endregion

#pragma region Decision dialog (evolution / farewell)
#define CH_Y 36
#define CH_W 280
#define CH_H 140
#define CH_R 12
#define CH_QY 12
#define CH_BW 248
#define CH_B1_REL_Y 40
#define CH_B2_REL_Y 92
#define CH_BH 40
#define CH_BTY 12
#pragma endregion

#pragma region CTA buttons (evolve / farewell / runaway)
#define EVO_BTN_W 200
#define EVO_BTN_H 40
#define EVO_BTN_Y 70
#define EVO_R 10
#define FAR_BTN_W 296
#define FAR_BTN_H 36
#define FAR_BTN_Y 76
#define FAR_R 10
#pragma endregion

#pragma region Settings screens: section label box width (titles: drawScreenTitle(), CARD_PROGRESS_TITLE_*)
#define SETTINGS_TITLE_MAX_W 240
#pragma endregion

#pragma region Settings menu: four rows (y 44-210) and the version line (y 222-230)
// Row 0 starts below the TS(5) title (y 12..36) and the close button's hit circle (ends at y 38).
#define SETTINGS_MENU_ROW_W 260
#define SETTINGS_MENU_ROW_Y 44
#define SETTINGS_MENU_ROW_H 37
#define SETTINGS_MENU_ROW_GAP 6
#define SETTINGS_MENU_ROW_R 10
#define SETTINGS_MENU_ARROW_INSET 16
#define SETTINGS_MENU_ARROW_SIZE 10
#define SETTINGS_MENU_VERSION_Y (LCD_HEIGHT - 18)  // size-1 text ends 10 px above the bottom edge
#pragma endregion

#pragma region Settings menu row 0: icon then label (no WiFi on this board: Screen Off fills the row)
#define SETTINGS_MENU_SPLIT_GAP 8
#define SETTINGS_MENU_ICON_SIZE 14
#define SETTINGS_MENU_ICON_INSET 6
#pragma endregion

#pragma region WiFi screen: On/Off pill and Scan button (y 44-74), status line (y 80-88), 4 network rows (y 94-231)
// Rows x 8..276; the page arrows column (hit box x 278..318) sits on their right.
#define WIFI_TOGGLE_X 20
#define WIFI_TOGGLE_W 110
#define WIFI_SCAN_X 170
#define WIFI_SCAN_W 120
#define WIFI_BUTTON_Y 44
#define WIFI_BUTTON_H 30
#define WIFI_BUTTON_R 8
#define WIFI_STATUS_Y 80
#define WIFI_STATUS_SIZE 1
#define WIFI_STATUS_MAX_W 300
#define WIFI_LIST_X 8
#define WIFI_LIST_W 268
#define WIFI_LIST_Y 94
#define WIFI_ROW_H 32
#define WIFI_ROW_STEP 35
#define WIFI_ROWS_PER_PAGE 4
#define WIFI_ROW_R 6
#define WIFI_ROW_PAD 5
#define WIFI_ROW_ICON_SIZE 12
#define WIFI_ROW_SSID_SIZE 2
#define WIFI_ROW_SSID_DY 4
#define WIFI_ROW_DETAIL_SIZE 1
#define WIFI_ROW_DETAIL_DY 22
#define WIFI_ROW_CHECK_SIZE 14
#define WIFI_ARROW_CX 298
#define WIFI_ARROW_SIZE 14
#define WIFI_ARROW_UP_CY 110
#define WIFI_ARROW_DOWN_CY 216
#define WIFI_ARROW_HIT_W 40
#define WIFI_ARROW_HIT_H 50
#define WIFI_PAGE_TEXT_SIZE 1
#pragma endregion

#pragma region Audio / video: sound pill, brightness -/+ buttons (CLK_BTN x CLK_BTN_H) and bar
// Under the TS(5) title (y 12..36): label 50..66, pill 74..110, label 130..146, -/+ row 154..190
#define SETTINGS_SOUND_LABEL_Y 50
#define SETTINGS_SOUND_PILL_Y 74
#define SETTINGS_SOUND_PILL_W 100
#define SETTINGS_SOUND_PILL_H 36
#define SETTINGS_PILL_R 8
#define SETTINGS_BRIGHTNESS_LABEL_Y 130
#define SETTINGS_BRIGHTNESS_ROW_Y 154
#define SETTINGS_BRIGHTNESS_MINUS_X 40
#define SETTINGS_BRIGHTNESS_BAR_W 128
#define SETTINGS_BRIGHTNESS_BAR_H 16
#pragma endregion

#pragma region Language list: 2 columns x 4 rows (y 44-227, 42 px tall rows)
// Below the TS(5) title (y 12..36) and the close button's hit circle (ends at y 38).
#define SETTINGS_LANG_CELL_COL_STEP 145
#define SETTINGS_LANG_CELL_W 135
#define SETTINGS_LANG_CELL_Y0 44
#define SETTINGS_LANG_CELL_ROW_STEP 47
#define SETTINGS_LANG_CELL_H 42
#define SETTINGS_LANG_CELL_R 8
#pragma endregion

#pragma region Time setting: date and time lines, one -/+ pair, toggles, OK
// under the TS(5) title (y 12..36): date 46..70 (highlight from 43), time 78..102,
// -/+ 116..152, toggles 162..192, OK 202..232
#define CLK_DATE_Y 46
#define CLK_DATE_SIZE 3  // "2026-10-27": 180 px wide
#define CLK_DIG_Y 78
#define CLK_DIG_SIZE 3   // "14:35": 90 px wide
#define CLK_FIELD_PAD 3  // selected field highlight, beyond the glyphs
#define CLK_BTN 44       // -/+ buttons here and for the brightness: CLK_BTN wide, CLK_BTN_H tall
#define CLK_BTN_H 36
#define CLK_BTN_Y 116
#define CLK_STEP_GAP 24  // between the - and + buttons
#define CLK_BTN_R 8
#define CLK_GLYPH_DX 13
#define CLK_GLYPH_DY 7
#define CLK_FORMAT_X 36
#define CLK_MODE_W 96
// the AM/PM toggle mirrors the 12h/24h toggle on the right edge
#define CLK_AMPM_X (LCD_WIDTH - CLK_FORMAT_X - CLK_MODE_W)
#define CLK_MODE_Y 162
#define CLK_MODE_H 30
#define CLK_MODE_R 8
#define CLK_OK_Y 202
#define CLK_OK_W 140
#define CLK_OK_H 30
#define CLK_OK_R 8
#pragma endregion

#pragma region Medal / streak notice
#define CEL_Y 60
#define CEL_W 280
#define CEL_H 52
#define CEL_R 10
#define CEL_L1Y 66
#define CEL_L2Y 88
#pragma endregion

#pragma region Info card: medals, name and stats
#define CARD_HEAD_MAXW 250
#pragma endregion

#pragma region Shared close button (top-right corner)
#define CLOSE_BTN_CX (LCD_WIDTH - 16)
#define CLOSE_BTN_CY 16
#define CLOSE_BTN_R 12
#define CLOSE_BTN_HIT_R 22
#pragma endregion

#pragma region Info card: page arrows, page dots, rename pencil, main-screen name hit box
#define CARD_ARROW_LEFT_CX 12
#define CARD_ARROW_SIZE 14
#define CARD_ARROW_HIT_W 36
#define CARD_ARROW_HIT_H 64
#define CARD_SIDE_MARGIN 23
#define CARD_DOTS_Y (LCD_HEIGHT - 10)
#define CARD_DOT_STEP 18
#define CARD_PENCIL_GAP 4
#define CARD_PENCIL_MIN_SIZE 12
#define CARD_PENCIL_HIT_PAD 10
#define HDR_NAME_HIT_PAD 6
#pragma endregion

#pragma region Info card page 0: portrait column (left) and five stat rows (right)
// Portrait box x 33..125, y 56..192, nudged 6 px left of the centre (x 85) of the
// free space between the left arrow hit box (x < 36) and the stat rows (x >= 134).
#define CARD_PROFILE_PORTRAIT_X 33
#define CARD_PROFILE_PORTRAIT_Y 56
#define CARD_PROFILE_PORTRAIT_W 92
#define CARD_PROFILE_PORTRAIT_H 136
#define CARD_PROFILE_PORTRAIT_MAX_SCALE 3
// Status message under the portrait: up to two lines of this text size from y 196
#define CARD_PROFILE_STATUS_Y 196
#define CARD_PROFILE_STATUS_SIZE 1
// Rows x 134..282 (2 px left of the right arrow hit box, which starts at 284), y 50..224:
// the close button's hit circle ends at y 38 and the page dots start at y 225. Labels
// and values both use size 2 (16 px), so a row is 33 px in a 35 px step.
#define CARD_PROFILE_STATS_LEFT 134
#define CARD_PROFILE_STATS_RIGHT (LCD_WIDTH - CARD_ARROW_HIT_W - 2)
#define CARD_PROFILE_ROW_Y0 52
#define CARD_PROFILE_ROW_STEP 34
#define CARD_PROFILE_LABEL_SIZE 1
#define CARD_PROFILE_VALUE_SIZE 2
#define CARD_PROFILE_VALUE_GAP 2
#define CARD_PROFILE_BAR_H 8
#define CARD_PROFILE_BAR_GAP 6
// Nickname's "(SPECIES)" line, this far under the name (TS(5) = 3 name: y 12..36,
// subtitle y 39..47, above the stat rows at y 52 and the portrait at y 56)
#define CARD_PROFILE_SUBTITLE_SIZE 1
#define CARD_PROFILE_SUBTITLE_GAP 3
#pragma endregion

#pragma region Info card page 1 (stats): type chips under the title (y 12-36), then 7 rows of text
// Chips y 40..60; rows HP..LOSSES y 64..200, above the page dots (y 230)
#define CARD_STATS_TYPES_Y 40
#define CARD_STATS_ROW_Y0 64
#define CARD_STATS_ROW_STEP 20
#pragma endregion

#pragma region Info card page 3 (progress): vertical rhythm, tops of each text line / the bar
// Spread evenly from the title (y 12) to the last line (ends y 217), above the page dots (y 226)
#define CARD_PROGRESS_TITLE_Y 12
#define CARD_PROGRESS_TITLE_MAXW (LCD_WIDTH - 2 * CARD_SIDE_MARGIN)
#define CARD_PROGRESS_LEVEL_Y 42
#define CARD_PROGRESS_BAR_Y 83
#define CARD_PROGRESS_NEXT_GAP 16
#define CARD_PROGRESS_EVO_LABEL_Y 140
#define CARD_PROGRESS_EVO_VALUE_Y 160
#define CARD_PROGRESS_MISTAKES_Y 201
#pragma endregion

#pragma region Pokedex gallery: title, R/C counts, filter column, 4x4 grid (x 120-284, y 38-202), detail header width
// The grid ends where the right arrow hit box starts (x 284)
#define GALLERY_CELL 41
#define GALLERY_GRID_X (LCD_WIDTH - CARD_ARROW_HIT_W - GALLERY_COLS * GALLERY_CELL)
#define GALLERY_GRID_Y 38
#define GALLERY_TITLE_Y 4
#define GALLERY_TITLE_SIZE 3
#define GALLERY_COUNTS_Y 24
#define GALLERY_COUNTS_SIZE 1
// filters: 4 stacked buttons x 40..114, y 60..180 (centered on the grid), right of the
// left arrow hit box (x < 36)
#define GALLERY_FILTER_W 74
#define GALLERY_FILTER_H 24
#define GALLERY_FILTER_GAP 8
#define GALLERY_FILTER_R 6
#define GALLERY_FILTER_TEXT_SIZE 2
#define GALLERY_THUMB_SCALE_NUM 1
#define GALLERY_THUMB_SCALE_DEN 1
// "C" marker on a cell: species caught in a wild battle but never raised
#define GALLERY_CAUGHT_MARK_SIZE 1
#define GALLERY_DETAIL_HEADER_MAXW 234
// Detail type chips, centered under the header (size-3 header ends y 52, chips y 60..80)
#define GALLERY_DETAIL_TYPES_Y 60
#define GALLERY_TYPE_CHIP_TEXT_SIZE 2
#define GALLERY_TYPE_CHIP_H 20
#define GALLERY_TYPE_CHIP_PAD 6
#define GALLERY_TYPE_CHIP_R 5
#define GALLERY_TYPE_CHIP_GAP 6
#pragma endregion

#pragma region Info card page 2 (medals): 2x4 grid x 26-294, y 48-212 under the title (y 12-36)
// Narrowed (was x 12..308) so it stays clear of the page arrows; the bottom row
// ends above the page dots (y 230)
#define MED_COLSTEP 136
#define MED_W 132
#define MED_Y0 48
#define MED_STEP 44
#define MED_H 32
#define MED_MARK_CX 13
#define MED_MARK_R 8
#define MED_TXT_X 28
#define BAR_H_CARD 12
#pragma endregion

#pragma region Keyboard: 7 x 5 keys (x 13-307, y 62-232), text box left of the close button (x < 292)
#define KB_Y (LCD_HEIGHT - 5 * KB_H - 8)  // 5 key rows ending 8 px above the bottom edge
#define KB_W 42
#define KB_H 34
#define KB_TITLE_Y 8
#define KB_BOX_X 10
#define KB_BOX_Y 28
#define KB_BOX_W 280
#define KB_BOX_H 26
#define KB_BOX_R 6
#define KB_BOX_TX 16
#define KB_BOX_TY 32
#pragma endregion

#pragma region Poop row
#define POOP_S 1
#define POOP_X0 6
#define POOP_DX 34
#define POOP_Y SY(244)
#pragma endregion

#pragma region Main screen needs bars (4 bars, 2 columns x 2 rows, bottom panel y 160-240)
// Rect: rows y 162..196, the action buttons start at y 200; right bar ends at x 298.
#define NEEDS_LEFT_X 10
#define NEEDS_RIGHT_X 166
#define NEEDS_ROW1_Y 162
#define NEEDS_ROW2_Y 180
#define NEEDS_BAR_H 16
#define NEEDS_BAR_W_MAX 84
#define NEEDS_BAR_W_MIN 40
#define NEEDS_COLUMN_SPACING 24
#pragma endregion

#pragma region Ball game top: record line, then the lives (three dots, the middle one on CX)
#define BALL_RECORD_Y 10
#define LIFE_DX 14
#define LIFE_Y 34
#define LIFE_R 4
#pragma endregion

#pragma region Minigame score line (bottom, centered): ball game (coin chase has COIN_SCORE_Y)
#define MINIGAME_SCORE_Y (LCD_HEIGHT - 18)
#define MINIGAME_SCORE_TEXT_SIZE 2
#define MINIGAME_SCORE_MAX_W (LCD_WIDTH - 20)
#pragma endregion

#pragma region Ball game: ball, creature, heart
#define BALL_S 2
#define BALL_OFF 16
#define STAGE_FLOOR_TOP_Y 172  // stage floor, back edge
#define STAGE_FRONT_Y 204      // stage floor, front edge (the apron below carries the score)
#define GAME_PET_Y 194         // ball game: feet on the stage floor
#define PET_HEART_SCALE 1  // "likes it" heart over the creature (32 px bitmap)
#pragma endregion

#pragma region Ball game: smallest touch radius around the ball (resistive touch needs slack)
#define BALL_HIT_RADIUS_MIN 30
#pragma endregion

#pragma region Play picker: ball / coin / glove / swords strip (food picker style, 4 items of 64 px, centered)
#define PLAY_MENU_ITEM 64
#define PLAY_MENU_ITEM_COUNT 4
#define PLAY_MENU_Y 148
#define PLAY_MENU_H 48
#define PLAY_MENU_ICON_S 2
#define PLAY_MENU_R 10
#pragma endregion

#pragma region Training bag: seconds left (top middle), bag hook, boxing ring canvas, hit count (bottom)
#define SACK_TIMER_Y 8
#define SACK_TIMER_TEXT_SIZE 4     // TS() makes it 3 (24 px)
#define SACK_ROPE_TOP 66           // bag hook: the bag hangs centered on CY
#define RING_CANVAS_TOP_Y 168      // ring canvas, far edge (the ropes sit above it)
#define SACK_HINT_Y 180
#define SACK_SCORE_Y MINIGAME_SCORE_Y
#pragma endregion

#pragma region Coin chase: timer (top middle), coin and sprite sizes, play rectangle
#define COIN_TIMER_Y 12            // below the top fence rail
#define COIN_TIMER_TEXT_SIZE 4
#define COIN_SCORE_Y (LCD_HEIGHT - 30)  // above the bottom fence rail
#define COIN_RADIUS 9
#define COIN_PET_MAX_SCALE 1
#define COIN_PET_SIZE_PERCENT 125  // x1 zoom grown to x1.25
#define COIN_PET_BODY_OFFSET 25    // body center sits this far above the feet
#define COIN_PLAY_LEFT 24          // body center stays inside this rectangle
#define COIN_PLAY_RIGHT (LCD_WIDTH - 24)
#define COIN_PLAY_TOP 44           // below the timer and the close button
#define COIN_PLAY_BOTTOM (LCD_HEIGHT - 44)  // above the score line
#define COIN_SPAWN_MIN_DISTANCE 70
#pragma endregion

#pragma region Wild encounter prompt: 280x150 box centered (y 45-195), buttons 248x44 stacked
#define WILD_PROMPT_Y (CY - WILD_PROMPT_H / 2)
#define WILD_PROMPT_W 280
#define WILD_PROMPT_H 150
#define WILD_PROMPT_R 12
#define WILD_PROMPT_TITLE_Y 51
#define WILD_PROMPT_TITLE_SIZE 2
#define WILD_PROMPT_NAME_Y 73
#define WILD_PROMPT_NAME_SIZE 2
#define WILD_PROMPT_BTN_W 248
#define WILD_PROMPT_BTN_H 44
#define WILD_PROMPT_BTN_R 10
#define WILD_PROMPT_BTN_TEXT_SIZE 2
#define WILD_PROMPT_FIGHT_Y 95
#define WILD_PROMPT_LATER_Y 145
#pragma endregion

#pragma region Wild battle: title (y 2-18), two info columns (y 20-54), run button top right
// Columns x 6..156 (pet) and x 164..314 (wild): name (y 20..36), HP bar (y 37..42),
// type chips (y 43..53). The run button sits where the close X normally is; its
// hit box (x 250..320, y 0..40) is larger than the drawn pill.
#define BATTLE_TITLE_Y 2
#define BATTLE_TITLE_SIZE 2
#define BATTLE_TITLE_MAX_W 200
#define BATTLE_NAME_Y 20
#define BATTLE_NAME_SIZE 2
#define BATTLE_NAME_MAX_W 150
#define BATTLE_PET_NAME_X 6
#define BATTLE_WILD_NAME_RIGHT (LCD_WIDTH - BATTLE_PET_NAME_X)
#define BATTLE_HP_Y 37
#define BATTLE_HP_W 150
#define BATTLE_HP_H 5
#define BATTLE_HP_R 2
#define BATTLE_PET_HP_X 6
#define BATTLE_CHIP_Y 43
#define BATTLE_CHIP_H 10
#define BATTLE_CHIP_R 3
#define BATTLE_CHIP_PAD 3
#define BATTLE_CHIP_GAP 3
#define BATTLE_PET_CHIP_X 6
// caught marker: pokeball icon (16 px) left of the wild type chips, y 43..59
#define BATTLE_CAUGHT_DX 18
#define BATTLE_CAUGHT_Y 43
#define BATTLE_CAUGHT_S 1
#define BATTLE_RUN_X 262
#define BATTLE_RUN_Y 1
#define BATTLE_RUN_W 56
#define BATTLE_RUN_H 18
#define BATTLE_RUN_R 6
#define BATTLE_RUN_HIT_X 250
#define BATTLE_RUN_HIT_Y 0
#define BATTLE_RUN_HIT_W 70
#define BATTLE_RUN_HIT_H 40
#pragma endregion

#pragma region Wild battle: sprites at scale 2 with feet on y 150 over drawGameScene()
// each creature centered in its half of the screen, its thumbnail fallback centered on it
#define BATTLE_PET_CX (LCD_WIDTH / 4)
#define BATTLE_WILD_CX (LCD_WIDTH * 3 / 4)
#define BATTLE_GROUND_Y 150
#define BATTLE_SPRITE_MAX_S 2
#define BATTLE_PET_THUMB_X (BATTLE_PET_CX - BATTLE_THUMB_CELL / 2)
#define BATTLE_WILD_THUMB_X (BATTLE_WILD_CX - BATTLE_THUMB_CELL / 2)
#define BATTLE_THUMB_Y 65
#define BATTLE_THUMB_S 2
#define BATTLE_THUMB_CELL 80
#pragma endregion

#pragma region Wild battle: moves of the last turn, one line centered under each sprite (y 172-188)
// boxes x 4..156 and 164..316; text shrinks to size 1 when it does not fit
#define BATTLE_MOVE_Y 172
#define BATTLE_MOVE_SIZE 2
#define BATTLE_MOVE_W 152
#pragma endregion

#pragma region Wild battle: Attack / Dodge / Rest buttons (y 192-236) and the Quick / Attack / Heavy overlay (y 128-172)
// the overlay buttons sit in the same columns as the action buttons below them (layout_common.h)
#define BATTLE_ACTION_Y 192
#define BATTLE_ACTION_W 100
#define BATTLE_ACTION_H 44
#define BATTLE_ACTION_R 10
#define BATTLE_ATTACK_X 5
#define BATTLE_ACTION_HIT_PAD_X 2
#define BATTLE_ACTION_HIT_PAD_Y 2
// Quick / Normal / Heavy overlay: just above the Attack / Dodge / Rest row
#define BATTLE_ATTACK_MENU_GAP 6
#define BATTLE_ATTACK_MENU_Y (BATTLE_ACTION_Y - BATTLE_ATTACK_MENU_GAP - BATTLE_ATTACK_MENU_H)
#define BATTLE_ATTACK_MENU_W 100
#define BATTLE_ATTACK_MENU_H 44
#define BATTLE_ATTACK_MENU_R 10
#define BATTLE_ATTACK_MENU_HIT_PAD_X 2
#define BATTLE_ATTACK_MENU_HIT_PAD_Y 2
#define BATTLE_BTN_TEXT_SIZE 2
#pragma endregion

#pragma region Wild battle result: white panel over the sprites (y 96-176), buttons in the action row
#define BATTLE_RESULT_PANEL_X (CX - BATTLE_RESULT_PANEL_W / 2)
#define BATTLE_RESULT_PANEL_Y 96
#define BATTLE_RESULT_PANEL_W 240
#define BATTLE_RESULT_PANEL_H 80
#define BATTLE_RESULT_PANEL_R 10
#define BATTLE_RESULT_TEXT_MAX_W 232
#define BATTLE_RESULT_TITLE_Y 100
#define BATTLE_RESULT_TITLE_SIZE 3
#define BATTLE_RESULT_ROUNDS_Y 128
#define BATTLE_RESULT_DAMAGE_Y 140
#define BATTLE_RESULT_LINE_SIZE 1
#define BATTLE_RESULT_INFO_Y 154
#define BATTLE_RESULT_INFO_SIZE 2
#define BATTLE_RESULT_BTN_Y 192
#define BATTLE_RESULT_BTN_H 44
#define BATTLE_RESULT_BTN_R 10
#define BATTLE_CATCH_X 10
#define BATTLE_CATCH_W 145
#define BATTLE_OK_W 150
#define BATTLE_OK_TEXT_SIZE 2
#define BATTLE_RESULT_HIT_PAD_Y 2
#pragma endregion

#pragma once
#include "board_select.h"

// Layout constants derived from each board's screen shape.
// They replace the original's CX/CY/HORIZON literals (233/233/232,
// computed by hand for the round 466x466 canvas).

#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)

// Many coordinates in drawScene() etc. are hand-written for the original
// 466x466 canvas. SX()/SY() rescale them proportionally; on the
// Waveshare (466x466) they are an exact no-op.
#define SX(px) (((int32_t)(px) * LCD_WIDTH) / 466)
#define SY(px) (((int32_t)(px) * LCD_HEIGHT) / 466)

#if BOARD_IS_ROUND
  #define HORIZON 232
  // Minigame: the ball bounces off the bezel's circular wall
  #define ARENA_WALL_R 205
  #define ARENA_GROUND_Y 384
#else
  #define HORIZON ((LCD_HEIGHT * 62) / 100)
  // No circular wall in a rectangle: bounces off left/right/ceiling
  #define ARENA_LEFT 10
  #define ARENA_RIGHT (LCD_WIDTH - 10)
  #define ARENA_TOP 4
  #define ARENA_GROUND_Y (LCD_HEIGHT - 4)
#endif

// ---------------------------------------------------------------------------
// Menus and dialogs: ONE layout definition shared by drawing and touch
// detection (if a table changes, both change at once).
// Round = the original's literal values (466x466). Rectangular = a dedicated
// redesign for 320x240: smaller boxes, smaller text sizes, without the
// slack the circle needed.
// ---------------------------------------------------------------------------

// uniform scale (by height) for drawings that must keep their proportions
#define SK(v) (((int32_t)(v) * LCD_HEIGHT) / 466)
#define SKM(v) (SK(v) < 1 ? 1 : SK(v))
// SK() for fractional values (ball physics): same ratio, without integer truncation
#define SKF(v) ((float)(v) * (float)LCD_HEIGHT / 466.0f)

#if BOARD_IS_ROUND
  // text size: identity
  #define TS(n) (n)
  // header and battery
  #define HDR_NAME_Y 52
  #define HDR_MSG_Y 90
  #define BAT_X (CX - 14)
  #define BAT_Y 12
  //#region Main screen header: hamburger (top left), clock (top right), streak badge under the clock, level row
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
  #define POKEDEX_BTN_X (HAMBURGER_X + HAMBURGER_W / 2 - 8 * POKEDEX_BTN_S)
  #define POKEDEX_BTN_Y 132
  #define POKEDEX_BTN_HIT_R 26
  //#region Sleep bubble: "Zzz" speech bubble by the sleeping creature's head (drawSleepBubble)
  #define SLEEP_BUBBLE_TEXT_SIZE 2
  #define SLEEP_BUBBLE_PAD_X 8
  #define SLEEP_BUBBLE_PAD_Y 6
  #define SLEEP_BUBBLE_R 10
  #define SLEEP_BUBBLE_TAIL 10        // tail base width and its drop to the head
  #define SLEEP_BUBBLE_GAP 2          // bubble left edge past the anchor point
  #define SLEEP_BUBBLE_HEAD_INSET 10  // anchor this far below the top of the sprite box
  #define SLEEP_BUBBLE_MAX_RIGHT 400  // stays inside the bezel at head height
  #define SLEEP_BUBBLE_MIN_TOP 120    // below the header and the level row
  //#endregion
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
  //#endregion
  // starter choice
  #define STARTER_TITLE_Y 68
  #define STARTER_ROW_X 70
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
  // food picker: apple / hamburger / pizza / candy / coffee; corners stay inside the bezel (farthest 203 px)
  #define FEED_ITEM 66
  #define FEED_ITEM_COUNT 5
  #define FEED_W (FEED_ITEM_COUNT * FEED_ITEM)
  #define FEED_X (CX - FEED_W / 2)
  #define FEED_Y 288
  #define FEED_H 64
  #define FEED_ICON_S 3
  #define FEED_R 14
  // "release?" dialog
  #define CONF_X 94
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
  // decision dialog (evolution / farewell)
  #define CH_X 73
  #define CH_Y 156
  #define CH_W 320
  #define CH_H 188
  #define CH_R 16
  #define CH_QY 20
  #define CH_BX 93
  #define CH_BW 280
  #define CH_B1_REL_Y 50
  #define CH_B2_REL_Y 112
  #define CH_BH 52
  #define CH_BTY 18
  // CTA buttons (evolve / farewell / runaway)
  #define EVO_BTN_W 256
  #define EVO_BTN_H 64
  #define EVO_BTN_Y 172
  #define EVO_R 18
  #define FAR_BTN_W 408
  #define FAR_BTN_H 58
  #define FAR_BTN_Y 176
  #define FAR_R 16
  //#region Settings screens: section label box width (titles: drawScreenTitle(), CARD_PROGRESS_TITLE_*)
  #define SETTINGS_TITLE_MAX_W 260
  //#endregion
  //#region Settings menu: four rows (y 126..380, inside the circle) and the version line
  // Row 0 starts below the TS(5) title (y 58..98) and the close button's hit circle
  // (ends at y 122); the farthest row corner, (73, 380), is 217 px from the center.
  #define SETTINGS_MENU_ROW_X 73
  #define SETTINGS_MENU_ROW_W 320
  #define SETTINGS_MENU_ROW_Y 126
  #define SETTINGS_MENU_ROW_H 56
  #define SETTINGS_MENU_ROW_GAP 10
  #define SETTINGS_MENU_ROW_R 14
  #define SETTINGS_MENU_ARROW_INSET 22
  #define SETTINGS_MENU_ARROW_SIZE 16
  #define SETTINGS_MENU_VERSION_Y 436
  //#endregion
  //#region Audio / video: sound pill, brightness -/+ buttons (CLK_BTN squares) and bar
  // Under the TS(5) title (y 58..98): label 122..146 (below the close button's hit
  // circle, which ends at y 122), pill 158..210, label 240..264, -/+ row 276..334
  // (its farthest corners, (63, 334) and (403, 334), are 198 px from the center)
  #define SETTINGS_SOUND_LABEL_Y 122
  #define SETTINGS_SOUND_PILL_X 163
  #define SETTINGS_SOUND_PILL_Y 158
  #define SETTINGS_SOUND_PILL_W 140
  #define SETTINGS_SOUND_PILL_H 52
  #define SETTINGS_PILL_R 14
  #define SETTINGS_BRIGHTNESS_LABEL_Y 240
  #define SETTINGS_BRIGHTNESS_ROW_Y 276
  #define SETTINGS_BRIGHTNESS_MINUS_X 63
  #define SETTINGS_BRIGHTNESS_PLUS_X 345
  #define SETTINGS_BRIGHTNESS_BAR_X 137
  #define SETTINGS_BRIGHTNESS_BAR_W 192
  #define SETTINGS_BRIGHTNESS_BAR_H 24
  //#endregion
  //#region Language list: 2 columns x 4 rows (y 126..372, corners inside the circle)
  // Below the TS(5) title (y 58..98) and the close button's hit circle (ends at y 122);
  // the farthest corners, (55, 372) and (411, 372), are 226 px from the center.
  #define SETTINGS_LANG_CELL_X0 55
  #define SETTINGS_LANG_CELL_COL_STEP 188
  #define SETTINGS_LANG_CELL_W 168
  #define SETTINGS_LANG_CELL_Y0 126
  #define SETTINGS_LANG_CELL_ROW_STEP 64
  #define SETTINGS_LANG_CELL_H 54
  #define SETTINGS_LANG_CELL_R 12
  //#endregion
  // time setting, under the TS(5) title (y 58..98): digits 120..168, -/+ 190..248,
  // HOUR/MIN 256..272, toggles 290..334, OK 352..400 (its bottom corners are 195 px from the center)
  #define CLK_DIG_Y 120
  #define CLK_DIG_SIZE 6   // one size under the old TS(7)
  #define CLK_DIG_HALF 90  // half of "12:34" at CLK_DIG_SIZE: 5 chars * 6 px * 6 / 2
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
  #define CLK_PAIR_W (CLK_HP_X + CLK_BTN - CLK_HM_X)  // one -/+ pair, edge to edge (HOUR/MIN center under it)
  //#region Time setting: 12h/24h toggle under the hour pair, AM/PM toggle under the minute pair
  #define CLK_MODE_Y 290
  #define CLK_MODE_H 44
  #define CLK_MODE_R 12
  #define CLK_MODE_W CLK_PAIR_W
  #define CLK_FORMAT_X CLK_HM_X
  #define CLK_AMPM_X CLK_MM_X
  //#endregion
  #define CLK_OK_X 133
  #define CLK_OK_Y 352
  #define CLK_OK_W 200
  #define CLK_OK_H 48
  #define CLK_OK_R 14
  #define CLK_OK_TX 18
  #define CLK_OK_TY 364
  // medal / streak notice
  #define CEL_X 73
  #define CEL_Y 150
  #define CEL_W 320
  #define CEL_H 96
  #define CEL_R 16
  #define CEL_L1Y 176
  #define CEL_L2Y 212
  // info card: medals, name and stats
  #define CARD_HEAD_MAXW 198
  //#region Shared close button (top-right; inside the bezel circle: 191 px from the center + radius 20)
  #define CLOSE_BTN_CX 360
  #define CLOSE_BTN_CY 90
  #define CLOSE_BTN_R 20
  #define CLOSE_BTN_HIT_R 32
  //#endregion
  //#region Info card: page arrows, page dots, rename pencil, main-screen name hit box
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
  //#endregion
  //#region Info card page 0: portrait column (left) and five stat rows (right)
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
  //#endregion
  //#region Info card page 1 (stats): type chips under the title (y 58..98), then 7 rows of text
  // Chips y 104..130; rows HP..LOSSES y 144..388 (row corners stay inside the circle), above the page dots (y 428)
  #define CARD_STATS_TYPES_Y 104
  #define CARD_STATS_ROW_Y0 144
  #define CARD_STATS_ROW_STEP 38
  //#endregion
  //#region Info card page 3 (progress): vertical rhythm, tops of each text line / the bar
  // Spread evenly from the title (y 58) to the last line (ends y 395), above the page dots (y 428)
  #define CARD_PROGRESS_TITLE_Y 58
  #define CARD_PROGRESS_TITLE_MAXW 300
  #define CARD_PROGRESS_LEVEL_Y 112
  #define CARD_PROGRESS_BAR_Y 187
  #define CARD_PROGRESS_NEXT_GAP 32
  #define CARD_PROGRESS_EVO_LABEL_Y 286
  #define CARD_PROGRESS_EVO_VALUE_Y 312
  #define CARD_PROGRESS_MISTAKES_Y 379
  //#endregion
  //#region Pokedex gallery: title, R/C counts, filter column, 4x4 grid below the close button's hit circle (y <= 122)
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
  #define GALLERY_FILTER_X 62
  #define GALLERY_FILTER_W 80
  #define GALLERY_FILTER_Y 169
  #define GALLERY_FILTER_H 34
  #define GALLERY_FILTER_GAP 10
  #define GALLERY_FILTER_R 10
  #define GALLERY_FILTER_TEXT_SIZE 2
  #define GALLERY_THUMB_SCALE_NUM 3
  #define GALLERY_THUMB_SCALE_DEN 2
  // Detail header ends left of the close button (x 340)
  #define GALLERY_DETAIL_HEADER_MAXW 190
  // Detail type chips, centered under the header (header ends y 80, chips y 100..126)
  #define GALLERY_DETAIL_TYPES_Y 100
  #define GALLERY_TYPE_CHIP_TEXT_SIZE 2
  #define GALLERY_TYPE_CHIP_H 26
  #define GALLERY_TYPE_CHIP_PAD 10
  #define GALLERY_TYPE_CHIP_R 8
  #define GALLERY_TYPE_CHIP_GAP 8
  //#endregion
  //#region Info card page 2 (medals): 2x4 grid x 40..426, y 124..362 under the title (y 58..98)
  // Top row starts below the close button's hit circle (y <= 122); the bottom row's
  // corners (40, 362) and (426, 362) sit 232 px from the center, inside the 233 px bezel.
  #define MED_X0 40
  #define MED_COLSTEP 196
  #define MED_W 190
  #define MED_Y0 124
  #define MED_STEP 64
  #define MED_H 46
  //#endregion
  #define MED_MARK_CX 22
  #define MED_MARK_R 11
  #define MED_TXT_X 44
  #define BAR_H_CARD 22
  // keyboard
  #define KB_X 40
  #define KB_Y 150
  #define KB_W 64
  #define KB_H 52
  #define KB_TITLE_Y 56
  #define KB_BOX_X 83
  #define KB_BOX_Y 84
  #define KB_BOX_W 300
  #define KB_BOX_H 40
  #define KB_BOX_R 8
  #define KB_BOX_TX 95
  #define KB_BOX_TY 94
  #define KB_CH_DX 9
  #define KB_CH_DY 10
  #define KB_OK_DX 15
  // minigames / misc
  #define POOP_S 2
  #define POOP_X0 34
  #define POOP_DX 48
  #define POOP_Y 244
  //#region Main screen needs bars (4 bars, 2 columns x 2 rows, bottom panel y 312..466)
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
  //#endregion
  // ball game top: record line, then the lives (three dots, the middle one on CX)
  #define BALL_RECORD_Y 36
  #define LIFE_X0 (CX - LIFE_DX)
  #define LIFE_DX 28
  #define LIFE_Y 64
  #define LIFE_R 6
  //#region Minigame score line (bottom, centered): ball game and coin chase
  #define MINIGAME_SCORE_Y 404
  #define MINIGAME_SCORE_TEXT_SIZE 2
  #define MINIGAME_SCORE_MAX_W 260
  //#endregion
  #define BALL_S 3
  #define BALL_OFF 24
  #define GAME_PET_Y (MINIGAME_SCORE_Y - 10)  // ball game: feet just above the score line
  #define PET_HEART_SCALE 2  // "likes it" heart over the creature (32 px bitmap)
  //#region Ball game: smallest touch radius around the ball (the fork's radii are used as is)
  #define BALL_HIT_RADIUS_MIN 0
  //#endregion
  //#region Play picker: ball / coin / glove / swords strip (food picker style, 4 items, centered under the play button)
  #define PLAY_MENU_ITEM 66
  #define PLAY_MENU_ITEM_COUNT 4
  #define PLAY_MENU_W (PLAY_MENU_ITEM_COUNT * PLAY_MENU_ITEM)
  #define PLAY_MENU_X (CX - PLAY_MENU_W / 2)
  #define PLAY_MENU_Y 288
  #define PLAY_MENU_H 64
  #define PLAY_MENU_ICON_S 3
  #define PLAY_MENU_R 14
  //#endregion
  //#region Coin chase: timer (top middle), coin and sprite sizes, play area inside the bezel
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
  //#endregion
  //#region Wild encounter prompt: question, species line, FIGHT / LATER buttons (the fork's literals)
  #define WILD_PROMPT_X 82
  #define WILD_PROMPT_Y 156
  #define WILD_PROMPT_W 302
  #define WILD_PROMPT_H 178
  #define WILD_PROMPT_R 18
  #define WILD_PROMPT_TITLE_Y 176
  #define WILD_PROMPT_TITLE_SIZE 3
  #define WILD_PROMPT_NAME_Y 206
  #define WILD_PROMPT_NAME_SIZE 2
  #define WILD_PROMPT_BTN_X 93
  #define WILD_PROMPT_BTN_W 280
  #define WILD_PROMPT_BTN_H 44
  #define WILD_PROMPT_BTN_R 12
  #define WILD_PROMPT_BTN_TEXT_SIZE 2
  #define WILD_PROMPT_FIGHT_Y 226
  #define WILD_PROMPT_LATER_Y 278
  //#endregion
  //#region Wild battle: title, two info columns, run button (the fork's literals)
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
  #define BATTLE_WILD_HP_X 288
  #define BATTLE_CHIP_Y 130
  #define BATTLE_CHIP_H 16
  #define BATTLE_CHIP_R 5
  #define BATTLE_CHIP_PAD 7
  #define BATTLE_CHIP_GAP 4
  #define BATTLE_PET_CHIP_X 28
  #define BATTLE_WILD_CHIP_RIGHT 438
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
  //#endregion
  //#region Wild battle: sprites (feet on BATTLE_GROUND_Y) and the thumbnail fallback
  #define BATTLE_PET_CX 142
  #define BATTLE_WILD_CX 328
  #define BATTLE_GROUND_Y 272
  #define BATTLE_SPRITE_MAX_S 3
  #define BATTLE_PET_THUMB_X 94
  #define BATTLE_WILD_THUMB_X 280
  #define BATTLE_THUMB_Y 152
  #define BATTLE_THUMB_S 3
  #define BATTLE_THUMB_CELL 64
  //#endregion
  //#region Wild battle: last turn's moves, one line centered under each sprite (y 318..334)
  // boxes x 52..232 and 238..418, inside the circle at that height (chord x 16..450)
  #define BATTLE_MOVE_Y 318
  #define BATTLE_MOVE_SIZE 2
  #define BATTLE_MOVE_W 180
  //#endregion
  //#region Wild battle: Attack / Dodge / Rest buttons and the Quick / Heavy overlay
  // hit boxes = the drawn box grown by the *_HIT_PAD_* margins (the fork's generous areas)
  #define BATTLE_ACTION_Y 358
  #define BATTLE_ACTION_W 108
  #define BATTLE_ACTION_H 58
  #define BATTLE_ACTION_R 13
  #define BATTLE_ATTACK_X 58
  #define BATTLE_DODGE_X 179
  #define BATTLE_REST_X 300
  #define BATTLE_ACTION_HIT_PAD_X 12
  #define BATTLE_ACTION_HIT_PAD_Y 14
  #define BATTLE_ATTACK_MENU_Y 298
  #define BATTLE_ATTACK_MENU_W 150
  #define BATTLE_ATTACK_MENU_H 46
  #define BATTLE_ATTACK_MENU_R 12
  #define BATTLE_QUICK_X 74
  #define BATTLE_HEAVY_X 242
  #define BATTLE_ATTACK_MENU_HIT_PAD_X 8
  #define BATTLE_ATTACK_MENU_HIT_PAD_Y 6
  #define BATTLE_BTN_TEXT_SIZE 2
  //#endregion
  //#region Wild battle result: WIN / LOSS, rounds and damage, reward line, Catch / Leave or OK
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
  #define BATTLE_LEAVE_X 242
  #define BATTLE_CATCH_W 148
  #define BATTLE_OK_X 118
  #define BATTLE_OK_W 230
  #define BATTLE_OK_TEXT_SIZE 3
  #define BATTLE_RESULT_HIT_PAD_Y 4
  //#endregion
#else
  // 320x240: text 7->5, 6/5/4->3, 3->2, rest unchanged
  #define TS(n) ((n) >= 7 ? 5 : (n) >= 4 ? 3 : (n) == 3 ? 2 : (n))
  #define HDR_NAME_Y 6
  #define HDR_MSG_Y 26
  //#region Main screen header: hamburger (top left), clock (top right), streak badge under the clock, level row
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
  #define POKEDEX_BTN_X (HAMBURGER_X + HAMBURGER_W / 2 - 8 * POKEDEX_BTN_S)
  #define POKEDEX_BTN_Y 30
  #define POKEDEX_BTN_HIT_R 20
  //#region Sleep bubble: "Zzz" speech bubble by the sleeping creature's head (drawSleepBubble)
  #define SLEEP_BUBBLE_TEXT_SIZE 2
  #define SLEEP_BUBBLE_PAD_X 5
  #define SLEEP_BUBBLE_PAD_Y 4
  #define SLEEP_BUBBLE_R 6
  #define SLEEP_BUBBLE_TAIL 6         // tail base width and its drop to the head
  #define SLEEP_BUBBLE_GAP 1          // bubble left edge past the anchor point
  #define SLEEP_BUBBLE_HEAD_INSET 6   // anchor this far below the top of the sprite box
  #define SLEEP_BUBBLE_MAX_RIGHT (LCD_WIDTH - 4)
  #define SLEEP_BUBBLE_MIN_TOP 44     // below the header, the level row and the streak badge
  //#endregion
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
  //#endregion
  #define STARTER_TITLE_Y 10
  #define STARTER_ROW_X 20
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
  // food picker: apple / hamburger / pizza / candy / coffee (x 15..305)
  #define FEED_ITEM 58
  #define FEED_ITEM_COUNT 5
  #define FEED_W (FEED_ITEM_COUNT * FEED_ITEM)
  #define FEED_X (CX - FEED_W / 2)
  #define FEED_Y 150
  #define FEED_H 44
  #define FEED_ICON_S 2
  #define FEED_R 10
  #define CONF_X 20
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
  #define CH_X 20
  #define CH_Y 36
  #define CH_W 280
  #define CH_H 140
  #define CH_R 12
  #define CH_QY 12
  #define CH_BX 36
  #define CH_BW 248
  #define CH_B1_REL_Y 40
  #define CH_B2_REL_Y 92
  #define CH_BH 40
  #define CH_BTY 12
  #define EVO_BTN_W 200
  #define EVO_BTN_H 40
  #define EVO_BTN_Y 70
  #define EVO_R 10
  #define FAR_BTN_W 296
  #define FAR_BTN_H 36
  #define FAR_BTN_Y 76
  #define FAR_R 10
  //#region Settings screens: section label box width (titles: drawScreenTitle(), CARD_PROGRESS_TITLE_*)
  #define SETTINGS_TITLE_MAX_W 240
  //#endregion
  //#region Settings menu: four rows (y 44..210) and the version line (y 222..230)
  // Row 0 starts below the TS(5) title (y 12..36) and the close button's hit circle (ends at y 38).
  #define SETTINGS_MENU_ROW_X 30
  #define SETTINGS_MENU_ROW_W 260
  #define SETTINGS_MENU_ROW_Y 44
  #define SETTINGS_MENU_ROW_H 37
  #define SETTINGS_MENU_ROW_GAP 6
  #define SETTINGS_MENU_ROW_R 10
  #define SETTINGS_MENU_ARROW_INSET 16
  #define SETTINGS_MENU_ARROW_SIZE 10
  #define SETTINGS_MENU_VERSION_Y 222
  //#endregion
  //#region Audio / video: sound pill, brightness -/+ buttons (CLK_BTN squares) and bar
  // Under the TS(5) title (y 12..36): label 50..66, pill 74..110, label 130..146, -/+ row 154..198
  #define SETTINGS_SOUND_LABEL_Y 50
  #define SETTINGS_SOUND_PILL_X 110
  #define SETTINGS_SOUND_PILL_Y 74
  #define SETTINGS_SOUND_PILL_W 100
  #define SETTINGS_SOUND_PILL_H 36
  #define SETTINGS_PILL_R 8
  #define SETTINGS_BRIGHTNESS_LABEL_Y 130
  #define SETTINGS_BRIGHTNESS_ROW_Y 154
  #define SETTINGS_BRIGHTNESS_MINUS_X 40
  #define SETTINGS_BRIGHTNESS_PLUS_X 236
  #define SETTINGS_BRIGHTNESS_BAR_X 96
  #define SETTINGS_BRIGHTNESS_BAR_W 128
  #define SETTINGS_BRIGHTNESS_BAR_H 16
  //#endregion
  //#region Language list: 2 columns x 4 rows (y 44..227, 42 px tall rows)
  // Below the TS(5) title (y 12..36) and the close button's hit circle (ends at y 38).
  #define SETTINGS_LANG_CELL_X0 20
  #define SETTINGS_LANG_CELL_COL_STEP 145
  #define SETTINGS_LANG_CELL_W 135
  #define SETTINGS_LANG_CELL_Y0 44
  #define SETTINGS_LANG_CELL_ROW_STEP 47
  #define SETTINGS_LANG_CELL_H 42
  #define SETTINGS_LANG_CELL_R 8
  //#endregion
  // time setting, under the TS(5) title (y 12..36): digits 44..76 (glyphs end at 72), -/+ 80..124,
  // HOUR/MIN 130..146, toggles 154..184, OK 194..224
  #define CLK_DIG_Y 44
  #define CLK_DIG_SIZE 4   // one size under the old TS(7) = 5 (TS() has no 4 on this board)
  #define CLK_DIG_HALF 60  // half of "12:34" at CLK_DIG_SIZE: 5 chars * 6 px * 4 / 2
  #define CLK_BTN 44
  #define CLK_BTN_Y 80
  #define CLK_HM_X 36
  #define CLK_HP_X 88
  #define CLK_MM_X 188
  #define CLK_MP_X 240
  #define CLK_BTN_R 8
  #define CLK_GLYPH_DX 13
  #define CLK_GLYPH_DY 10
  #define CLK_LBL_Y 130
  #define CLK_PAIR_W (CLK_HP_X + CLK_BTN - CLK_HM_X)  // one -/+ pair, edge to edge (HOUR/MIN center under it)
  //#region Time setting: 12h/24h toggle under the hour pair, AM/PM toggle under the minute pair
  #define CLK_MODE_Y 154
  #define CLK_MODE_H 30
  #define CLK_MODE_R 8
  #define CLK_MODE_W CLK_PAIR_W
  #define CLK_FORMAT_X CLK_HM_X
  #define CLK_AMPM_X CLK_MM_X
  //#endregion
  #define CLK_OK_X 90
  #define CLK_OK_Y 194
  #define CLK_OK_W 140
  #define CLK_OK_H 30
  #define CLK_OK_R 8
  #define CLK_OK_TX 12
  #define CLK_OK_TY 201
  #define CEL_X 20
  #define CEL_Y 60
  #define CEL_W 280
  #define CEL_H 52
  #define CEL_R 10
  #define CEL_L1Y 66
  #define CEL_L2Y 88
  #define CARD_HEAD_MAXW 250
  //#region Shared close button (top-right corner)
  #define CLOSE_BTN_CX (LCD_WIDTH - 16)
  #define CLOSE_BTN_CY 16
  #define CLOSE_BTN_R 12
  #define CLOSE_BTN_HIT_R 22
  //#endregion
  //#region Info card: page arrows, page dots, rename pencil, main-screen name hit box
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
  //#endregion
  //#region Info card page 0: portrait column (left) and five stat rows (right)
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
  // Rows x 134..282 (right arrow hit box starts at 284), y 50..224: the close
  // button's hit circle ends at y 38 and the page dots start at y 225. Labels
  // and values both use size 2 (16 px), so a row is 33 px in a 35 px step.
  #define CARD_PROFILE_STATS_LEFT 134
  #define CARD_PROFILE_STATS_RIGHT 282
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
  //#endregion
  //#region Info card page 1 (stats): type chips under the title (y 12..36), then 7 rows of text
  // Chips y 40..60; rows HP..LOSSES y 64..200, above the page dots (y 230)
  #define CARD_STATS_TYPES_Y 40
  #define CARD_STATS_ROW_Y0 64
  #define CARD_STATS_ROW_STEP 20
  //#endregion
  //#region Info card page 3 (progress): vertical rhythm, tops of each text line / the bar
  // Spread evenly from the title (y 12) to the last line (ends y 217), above the page dots (y 226)
  #define CARD_PROGRESS_TITLE_Y 12
  #define CARD_PROGRESS_TITLE_MAXW (LCD_WIDTH - 2 * CARD_SIDE_MARGIN)
  #define CARD_PROGRESS_LEVEL_Y 42
  #define CARD_PROGRESS_BAR_Y 83
  #define CARD_PROGRESS_NEXT_GAP 16
  #define CARD_PROGRESS_EVO_LABEL_Y 140
  #define CARD_PROGRESS_EVO_VALUE_Y 160
  #define CARD_PROGRESS_MISTAKES_Y 201
  //#endregion
  //#region Pokedex gallery: title, R/C counts, filter column, 4x4 grid (x 120..284, y 38..202), detail header width
  // The grid ends where the right arrow hit box starts (x 284)
  #define GALLERY_CELL 41
  #define GALLERY_GRID_X 120
  #define GALLERY_GRID_Y 38
  #define GALLERY_TITLE_Y 4
  #define GALLERY_TITLE_SIZE 3
  #define GALLERY_COUNTS_Y 24
  #define GALLERY_COUNTS_SIZE 1
  // filters: 4 stacked buttons x 40..114, y 60..180 (centered on the grid), right of the
  // left arrow hit box (x < 36)
  #define GALLERY_FILTER_X 40
  #define GALLERY_FILTER_W 74
  #define GALLERY_FILTER_Y 60
  #define GALLERY_FILTER_H 24
  #define GALLERY_FILTER_GAP 8
  #define GALLERY_FILTER_R 6
  #define GALLERY_FILTER_TEXT_SIZE 2
  #define GALLERY_THUMB_SCALE_NUM 1
  #define GALLERY_THUMB_SCALE_DEN 1
  #define GALLERY_DETAIL_HEADER_MAXW 234
  // Detail type chips, centered under the header (size-3 header ends y 52, chips y 60..80)
  #define GALLERY_DETAIL_TYPES_Y 60
  #define GALLERY_TYPE_CHIP_TEXT_SIZE 2
  #define GALLERY_TYPE_CHIP_H 20
  #define GALLERY_TYPE_CHIP_PAD 6
  #define GALLERY_TYPE_CHIP_R 5
  #define GALLERY_TYPE_CHIP_GAP 6
  //#endregion
  //#region Info card page 2 (medals): 2x4 grid x 26..294, y 48..212 under the title (y 12..36)
  // Narrowed (was x 12..308) so it stays clear of the page arrows; the bottom row
  // ends above the page dots (y 230)
  #define MED_X0 26
  #define MED_COLSTEP 136
  #define MED_W 132
  #define MED_Y0 48
  #define MED_STEP 44
  #define MED_H 32
  //#endregion
  #define MED_MARK_CX 13
  #define MED_MARK_R 8
  #define MED_TXT_X 28
  #define BAR_H_CARD 12
  #define KB_X 10
  #define KB_Y 62
  #define KB_W 50
  #define KB_H 34
  #define KB_TITLE_Y 8
  #define KB_BOX_X 10
  #define KB_BOX_Y 28
  #define KB_BOX_W 300
  #define KB_BOX_H 26
  #define KB_BOX_R 6
  #define KB_BOX_TX 16
  #define KB_BOX_TY 32
  #define KB_CH_DX 6
  #define KB_CH_DY 8
  #define KB_OK_DX 12
  #define POOP_S 1
  #define POOP_X0 6
  #define POOP_DX 34
  #define POOP_Y SY(244)
  //#region Main screen needs bars (4 bars, 2 columns x 2 rows, bottom panel y 160..240)
  // Rect: rows y 162..196, the action buttons start at y 200; right bar ends at x 298.
  #define NEEDS_LEFT_X 10
  #define NEEDS_RIGHT_X 166
  #define NEEDS_ROW1_Y 162
  #define NEEDS_ROW2_Y 180
  #define NEEDS_BAR_H 16
  #define NEEDS_BAR_W_MAX 84
  #define NEEDS_BAR_W_MIN 40
  #define NEEDS_COLUMN_SPACING 24
  //#endregion
  // ball game top: record line, then the lives (three dots, the middle one on CX)
  #define BALL_RECORD_Y 10
  #define LIFE_X0 (CX - LIFE_DX)
  #define LIFE_DX 14
  #define LIFE_Y 34
  #define LIFE_R 4
  //#region Minigame score line (bottom, centered): ball game and coin chase
  #define MINIGAME_SCORE_Y (LCD_HEIGHT - 18)
  #define MINIGAME_SCORE_TEXT_SIZE 2
  #define MINIGAME_SCORE_MAX_W (LCD_WIDTH - 20)
  //#endregion
  #define BALL_S 2
  #define BALL_OFF 16
  #define GAME_PET_Y (MINIGAME_SCORE_Y - 4)  // ball game: feet just above the score line
  #define PET_HEART_SCALE 1  // "likes it" heart over the creature (32 px bitmap)
  //#region Ball game: smallest touch radius around the ball (resistive touch needs slack)
  #define BALL_HIT_RADIUS_MIN 30
  //#endregion
  //#region Play picker: ball / coin / glove / swords strip (food picker style, 4 items of 64 px, centered)
  #define PLAY_MENU_ITEM 64
  #define PLAY_MENU_ITEM_COUNT 4
  #define PLAY_MENU_W (PLAY_MENU_ITEM_COUNT * PLAY_MENU_ITEM)
  #define PLAY_MENU_X (CX - PLAY_MENU_W / 2)
  #define PLAY_MENU_Y 148
  #define PLAY_MENU_H 48
  #define PLAY_MENU_ICON_S 2
  #define PLAY_MENU_R 10
  //#endregion
  //#region Coin chase: timer (top middle), coin and sprite sizes, play rectangle
  #define COIN_TIMER_Y 6
  #define COIN_TIMER_TEXT_SIZE 4
  #define COIN_RADIUS 9
  #define COIN_PET_MAX_SCALE 1
  #define COIN_PET_SIZE_PERCENT 125  // x1 zoom grown to x1.25
  #define COIN_PET_BODY_OFFSET 25    // body center sits this far above the feet
  #define COIN_PLAY_LEFT 24          // body center stays inside this rectangle
  #define COIN_PLAY_RIGHT (LCD_WIDTH - 24)
  #define COIN_PLAY_TOP 44           // below the timer and the close button
  #define COIN_PLAY_BOTTOM (LCD_HEIGHT - 44)  // above the score line
  #define COIN_SPAWN_MIN_DISTANCE 70
  //#endregion
  //#region Wild encounter prompt: 280x150 box centered (y 45..195), buttons 248x44 stacked
  #define WILD_PROMPT_X 20
  #define WILD_PROMPT_Y 45
  #define WILD_PROMPT_W 280
  #define WILD_PROMPT_H 150
  #define WILD_PROMPT_R 12
  #define WILD_PROMPT_TITLE_Y 51
  #define WILD_PROMPT_TITLE_SIZE 2
  #define WILD_PROMPT_NAME_Y 73
  #define WILD_PROMPT_NAME_SIZE 2
  #define WILD_PROMPT_BTN_X 36
  #define WILD_PROMPT_BTN_W 248
  #define WILD_PROMPT_BTN_H 44
  #define WILD_PROMPT_BTN_R 10
  #define WILD_PROMPT_BTN_TEXT_SIZE 2
  #define WILD_PROMPT_FIGHT_Y 95
  #define WILD_PROMPT_LATER_Y 145
  //#endregion
  //#region Wild battle: title (y 2..18), two info columns (y 20..54), run button top right
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
  #define BATTLE_WILD_NAME_RIGHT 314
  #define BATTLE_HP_Y 37
  #define BATTLE_HP_W 150
  #define BATTLE_HP_H 5
  #define BATTLE_HP_R 2
  #define BATTLE_PET_HP_X 6
  #define BATTLE_WILD_HP_X 164
  #define BATTLE_CHIP_Y 43
  #define BATTLE_CHIP_H 10
  #define BATTLE_CHIP_R 3
  #define BATTLE_CHIP_PAD 3
  #define BATTLE_CHIP_GAP 3
  #define BATTLE_PET_CHIP_X 6
  #define BATTLE_WILD_CHIP_RIGHT 314
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
  //#endregion
  //#region Wild battle: sprites at scale 2 with feet on y 150 over drawGameScene()
  #define BATTLE_PET_CX 80
  #define BATTLE_WILD_CX 240
  #define BATTLE_GROUND_Y 150
  #define BATTLE_SPRITE_MAX_S 2
  #define BATTLE_PET_THUMB_X 40
  #define BATTLE_WILD_THUMB_X 200
  #define BATTLE_THUMB_Y 65
  #define BATTLE_THUMB_S 2
  #define BATTLE_THUMB_CELL 80
  //#endregion
  //#region Wild battle: last turn's moves, one line centered under each sprite (y 172..188)
  // boxes x 4..156 and 164..316; text shrinks to size 1 when it does not fit
  #define BATTLE_MOVE_Y 172
  #define BATTLE_MOVE_SIZE 2
  #define BATTLE_MOVE_W 152
  //#endregion
  //#region Wild battle: Attack / Dodge / Rest buttons (y 192..236) and the Quick / Heavy overlay (y 128..172)
  #define BATTLE_ACTION_Y 192
  #define BATTLE_ACTION_W 100
  #define BATTLE_ACTION_H 44
  #define BATTLE_ACTION_R 10
  #define BATTLE_ATTACK_X 5
  #define BATTLE_DODGE_X 110
  #define BATTLE_REST_X 215
  #define BATTLE_ACTION_HIT_PAD_X 2
  #define BATTLE_ACTION_HIT_PAD_Y 2
  #define BATTLE_ATTACK_MENU_Y 128
  #define BATTLE_ATTACK_MENU_W 148
  #define BATTLE_ATTACK_MENU_H 44
  #define BATTLE_ATTACK_MENU_R 10
  #define BATTLE_QUICK_X 8
  #define BATTLE_HEAVY_X 164
  #define BATTLE_ATTACK_MENU_HIT_PAD_X 2
  #define BATTLE_ATTACK_MENU_HIT_PAD_Y 2
  #define BATTLE_BTN_TEXT_SIZE 2
  //#endregion
  //#region Wild battle result: white panel over the sprites (y 96..176), buttons in the action row
  #define BATTLE_RESULT_PANEL_X 40
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
  #define BATTLE_LEAVE_X 165
  #define BATTLE_CATCH_W 145
  #define BATTLE_OK_X 85
  #define BATTLE_OK_W 150
  #define BATTLE_OK_TEXT_SIZE 2
  #define BATTLE_RESULT_HIT_PAD_Y 2
  //#endregion
#endif

#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define FAR_BTN_X (CX - FAR_BTN_W / 2)

//#region Settings screens: derived and board-independent values
// horizontal padding kept free on each side of a label inside its box
#define SETTINGS_TEXT_SIDE_PAD 3
// left edge of the section label box, centered on CX
#define SETTINGS_TITLE_X (CX - SETTINGS_TITLE_MAX_W / 2)
//#endregion

//#region Info card arrows: mirrored on the right edge, vertically centered
#define CARD_ARROW_RIGHT_CX (LCD_WIDTH - CARD_ARROW_LEFT_CX)
#define CARD_ARROW_CY CY
//#endregion

//#region Info card page 0: width of the stat rows column
#define CARD_PROFILE_STATS_W (CARD_PROFILE_STATS_RIGHT - CARD_PROFILE_STATS_LEFT)
//#endregion

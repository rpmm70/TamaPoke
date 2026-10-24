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
  // food picker
  #define FEED_X 101
  #define FEED_Y 288
  #define FEED_W 264
  #define FEED_H 64
  #define FEED_ITEM 66
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
  //#region Settings screens: shared title (menu, audio/video, time, language)
  #define SETTINGS_TITLE_Y 44
  #define SETTINGS_TITLE_MAX_W 260
  //#endregion
  //#region Settings menu: four rows (y 126..380, inside the circle) and the version line
  // Row 0 starts below the close button's hit circle (ends at y 122).
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
  #define SETTINGS_SOUND_LABEL_Y 112
  #define SETTINGS_SOUND_PILL_X 163
  #define SETTINGS_SOUND_PILL_Y 148
  #define SETTINGS_SOUND_PILL_W 140
  #define SETTINGS_SOUND_PILL_H 52
  #define SETTINGS_PILL_R 14
  #define SETTINGS_BRIGHTNESS_LABEL_Y 232
  #define SETTINGS_BRIGHTNESS_ROW_Y 270
  #define SETTINGS_BRIGHTNESS_MINUS_X 63
  #define SETTINGS_BRIGHTNESS_PLUS_X 345
  #define SETTINGS_BRIGHTNESS_BAR_X 137
  #define SETTINGS_BRIGHTNESS_BAR_W 192
  #define SETTINGS_BRIGHTNESS_BAR_H 24
  //#endregion
  //#region Language list: 2 columns x 4 rows (y 126..372, corners inside the circle)
  #define SETTINGS_LANG_CELL_X0 55
  #define SETTINGS_LANG_CELL_COL_STEP 188
  #define SETTINGS_LANG_CELL_W 168
  #define SETTINGS_LANG_CELL_Y0 126
  #define SETTINGS_LANG_CELL_ROW_STEP 64
  #define SETTINGS_LANG_CELL_H 54
  #define SETTINGS_LANG_CELL_R 12
  //#endregion
  // time setting (title: SETTINGS_TITLE_Y)
  #define CLK_DIG_Y 108
  #define CLK_DIG_HALF 105
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
  // info card: train button, medals, name and stats
  #define TRAIN_X 96
  #define TRAIN_Y 300
  #define TRAIN_W 274
  #define TRAIN_H 40
  #define TRAIN_TY 11
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
  //#region Pokedex gallery: 4x4 grid below the close button's hit circle (y <= 122), detail header width
  // Grid x 105..361, y 124..380; thumbnails (40 px) are drawn at 3/2 scale (60 px)
  #define GALLERY_CELL 64
  #define GALLERY_GRID_Y 124
  #define GALLERY_THUMB_SCALE_NUM 3
  #define GALLERY_THUMB_SCALE_DEN 2
  // Detail header ends left of the close button (x 340)
  #define GALLERY_DETAIL_HEADER_MAXW 190
  //#endregion
  #define MED_X0 28
  #define MED_COLSTEP 206
  #define MED_W 196
  #define MED_Y0 120
  #define MED_STEP 54
  #define MED_H 44
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
  #define LIFE_X0 180
  #define LIFE_DX 28
  #define LIFE_Y 104
  #define LIFE_R 6
  #define BALL_S 3
  #define BALL_OFF 24
  #define GAME_PET_Y 394
  #define PET_HEART_SCALE 2  // "likes it" heart over the creature (32 px bitmap)
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
  #define FEED_X 44
  #define FEED_Y 150
  #define FEED_W 232
  #define FEED_H 44
  #define FEED_ITEM 58
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
  //#region Settings screens: shared title (menu, audio/video, time, language)
  #define SETTINGS_TITLE_Y 8
  #define SETTINGS_TITLE_MAX_W 240
  //#endregion
  //#region Settings menu: four rows (y 42..212) and the version line
  // Row 0 starts below the close button's hit circle (ends at y 38).
  #define SETTINGS_MENU_ROW_X 30
  #define SETTINGS_MENU_ROW_W 260
  #define SETTINGS_MENU_ROW_Y 42
  #define SETTINGS_MENU_ROW_H 38
  #define SETTINGS_MENU_ROW_GAP 6
  #define SETTINGS_MENU_ROW_R 10
  #define SETTINGS_MENU_ARROW_INSET 16
  #define SETTINGS_MENU_ARROW_SIZE 10
  #define SETTINGS_MENU_VERSION_Y 222
  //#endregion
  //#region Audio / video: sound pill, brightness -/+ buttons (CLK_BTN squares) and bar
  #define SETTINGS_SOUND_LABEL_Y 38
  #define SETTINGS_SOUND_PILL_X 110
  #define SETTINGS_SOUND_PILL_Y 60
  #define SETTINGS_SOUND_PILL_W 100
  #define SETTINGS_SOUND_PILL_H 36
  #define SETTINGS_PILL_R 8
  #define SETTINGS_BRIGHTNESS_LABEL_Y 114
  #define SETTINGS_BRIGHTNESS_ROW_Y 140
  #define SETTINGS_BRIGHTNESS_MINUS_X 40
  #define SETTINGS_BRIGHTNESS_PLUS_X 236
  #define SETTINGS_BRIGHTNESS_BAR_X 96
  #define SETTINGS_BRIGHTNESS_BAR_W 128
  #define SETTINGS_BRIGHTNESS_BAR_H 16
  //#endregion
  //#region Language list: 2 columns x 4 rows (y 40..226, 42 px tall rows)
  #define SETTINGS_LANG_CELL_X0 20
  #define SETTINGS_LANG_CELL_COL_STEP 145
  #define SETTINGS_LANG_CELL_W 135
  #define SETTINGS_LANG_CELL_Y0 40
  #define SETTINGS_LANG_CELL_ROW_STEP 48
  #define SETTINGS_LANG_CELL_H 42
  #define SETTINGS_LANG_CELL_R 8
  //#endregion
  // time setting (title: SETTINGS_TITLE_Y)
  #define CLK_DIG_Y 30
  #define CLK_DIG_HALF 75
  #define CLK_BTN 44
  #define CLK_BTN_Y 78
  #define CLK_HM_X 36
  #define CLK_HP_X 88
  #define CLK_MM_X 188
  #define CLK_MP_X 240
  #define CLK_BTN_R 8
  #define CLK_GLYPH_DX 13
  #define CLK_GLYPH_DY 10
  #define CLK_LBL_Y 126
  #define CLK_PAIR_W (CLK_HP_X + CLK_BTN - CLK_HM_X)  // one -/+ pair, edge to edge (HOUR/MIN center under it)
  //#region Time setting: 12h/24h toggle under the hour pair, AM/PM toggle under the minute pair
  #define CLK_MODE_Y 150
  #define CLK_MODE_H 30
  #define CLK_MODE_R 8
  #define CLK_MODE_W CLK_PAIR_W
  #define CLK_FORMAT_X CLK_HM_X
  #define CLK_AMPM_X CLK_MM_X
  //#endregion
  #define CLK_OK_X 90
  #define CLK_OK_Y 192
  #define CLK_OK_W 140
  #define CLK_OK_H 30
  #define CLK_OK_R 8
  #define CLK_OK_TX 12
  #define CLK_OK_TY 199
  #define CEL_X 20
  #define CEL_Y 60
  #define CEL_W 280
  #define CEL_H 52
  #define CEL_R 10
  #define CEL_L1Y 66
  #define CEL_L2Y 88
  #define TRAIN_X 48
  #define TRAIN_Y 155
  #define TRAIN_W 224
  #define TRAIN_H 22
  #define TRAIN_TY 3
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
  //#region Pokedex gallery: 4x4 grid (x 78..242, y 43..207), detail header width
  #define GALLERY_CELL 41
  #define GALLERY_GRID_Y 43
  #define GALLERY_THUMB_SCALE_NUM 1
  #define GALLERY_THUMB_SCALE_DEN 1
  #define GALLERY_DETAIL_HEADER_MAXW 234
  //#endregion
  // medal grid narrowed (was x 12..308) so it stays clear of the page arrows
  #define MED_X0 26
  #define MED_COLSTEP 136
  #define MED_W 132
  #define MED_Y0 54
  #define MED_STEP 30
  #define MED_H 26
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
  #define LIFE_X0 SX(180)
  #define LIFE_DX 14
  #define LIFE_Y 60
  #define LIFE_R 4
  #define BALL_S 2
  #define BALL_OFF 16
  #define GAME_PET_Y (LCD_HEIGHT - 6)
  #define PET_HEART_SCALE 1  // "likes it" heart over the creature (32 px bitmap)
#endif

#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define FAR_BTN_X (CX - FAR_BTN_W / 2)

//#region Settings screens: derived and board-independent values
// horizontal padding kept free on each side of a label inside its box
#define SETTINGS_TEXT_SIDE_PAD 3
// left edge of the shared title box, centered on CX
#define SETTINGS_TITLE_X (CX - SETTINGS_TITLE_MAX_W / 2)
//#endregion

//#region Info card arrows: mirrored on the right edge, vertically centered
#define CARD_ARROW_RIGHT_CX (LCD_WIDTH - CARD_ARROW_LEFT_CX)
#define CARD_ARROW_CY CY
//#endregion

//#region Info card page 0: width of the stat rows column
#define CARD_PROFILE_STATS_W (CARD_PROFILE_STATS_RIGHT - CARD_PROFILE_STATS_LEFT)
//#endregion

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
  #define STREAK_X 26
  #define STREAK_Y 16
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
  //#region Settings menu: three rows (y 126..352, inside the circle) and the version line
  #define SETTINGS_MENU_ROW_X 73
  #define SETTINGS_MENU_ROW_W 320
  #define SETTINGS_MENU_ROW_Y 126
  #define SETTINGS_MENU_ROW_H 66
  #define SETTINGS_MENU_ROW_GAP 14
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
  #define CLK_HLBL_X 120
  #define CLK_MLBL_X 276
  //#region Time setting: 12h/24h switch (left) and AM/PM toggle (right), two segments each
  #define CLK_MODE_Y 290
  #define CLK_MODE_H 44
  #define CLK_MODE_R 12
  #define CLK_MODE_SEG_W 80
  #define CLK_FORMAT_X 63
  #define CLK_AMPM_X 243
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
  // Portrait box x 56..196, y 140..344: clear of the left arrow hit box (x < 56)
  // and its corners at <= 209 px from the center, inside the 233 px bezel.
  #define CARD_PROFILE_PORTRAIT_X 56
  #define CARD_PROFILE_PORTRAIT_Y 140
  #define CARD_PROFILE_PORTRAIT_W 140
  #define CARD_PROFILE_PORTRAIT_H 204
  #define CARD_PROFILE_PORTRAIT_MAX_SCALE 4
  // Rows x 206..404 (right arrow hit box starts at 410), y 124..368: the close
  // button's hit circle ends at y 122, the farthest corner (404, 368) sits 218 px
  // from the center, and the page dots are at y 428.
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
  #define POOP_X0 36
  #define POOP_DX 46
  #define POOP_Y 244
  #define LIFE_X0 180
  #define LIFE_DX 28
  #define LIFE_Y 104
  #define LIFE_R 6
  #define BALL_S 3
  #define BALL_OFF 24
  #define GAME_PET_Y 394
#else
  // 320x240: text 7->5, 6/5/4->3, 3->2, rest unchanged
  #define TS(n) ((n) >= 7 ? 5 : (n) >= 4 ? 3 : (n) == 3 ? 2 : (n))
  #define HDR_NAME_Y 6
  #define HDR_MSG_Y 26
  #define BAT_X (LCD_WIDTH - 36)
  #define BAT_Y 4
  #define STREAK_X 4
  #define STREAK_Y 44
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
  //#region Settings menu: three rows (y 36..208) and the version line
  #define SETTINGS_MENU_ROW_X 30
  #define SETTINGS_MENU_ROW_W 260
  #define SETTINGS_MENU_ROW_Y 36
  #define SETTINGS_MENU_ROW_H 52
  #define SETTINGS_MENU_ROW_GAP 8
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
  #define CLK_HLBL_X 40
  #define CLK_MLBL_X 192
  //#region Time setting: 12h/24h switch (left) and AM/PM toggle (right), two segments each
  #define CLK_MODE_Y 150
  #define CLK_MODE_H 30
  #define CLK_MODE_R 8
  #define CLK_MODE_SEG_W 65
  #define CLK_FORMAT_X 20
  #define CLK_AMPM_X 170
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
  // Portrait box x 36..128, y 56..216: clear of the left arrow hit box (x < 36).
  #define CARD_PROFILE_PORTRAIT_X 36
  #define CARD_PROFILE_PORTRAIT_Y 56
  #define CARD_PROFILE_PORTRAIT_W 92
  #define CARD_PROFILE_PORTRAIT_H 160
  #define CARD_PROFILE_PORTRAIT_MAX_SCALE 3
  // Rows x 134..282 (right arrow hit box starts at 284), y 52..222: the close
  // button's hit circle ends at y 38 and the page dots start at y 225. Labels
  // use size 1, so a CJK row (16 px label + 16 px value) fills its 34 px step.
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
  #define LIFE_X0 SX(180)
  #define LIFE_DX 14
  #define LIFE_Y 60
  #define LIFE_R 4
  #define BALL_S 2
  #define BALL_OFF 16
  #define GAME_PET_Y (LCD_HEIGHT - 6)
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

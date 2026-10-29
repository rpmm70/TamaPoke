#pragma once

// Game-wide state shared by the screen modules: every global variable, the
// small structs and enums they use, and the tuning constants of the screens.
// Definitions (with their initial values) live in app_state.cpp.

#pragma region Includes
#include <Arduino.h>
#include "boards/layout.h"   // CX, SX()/SY()/SK()/SKF(), LCD_WIDTH/LCD_HEIGHT
#include "colors.h"          // UI_INK, UI_INK_NIGHT
#include "dex.h"             // DEX_COUNT
#include "pet.h"             // Pet, BattleReward
#include "sdmon.h"           // SdMon, PmdMon, PMD_IDLE
#include "battle.h"          // BattleStats, BattleRuntime, BattleTurnResult
#include "wifi_manager.h"    // WIFI_SSID_MAX_LENGTH, WIFI_PASSWORD_MAX_LENGTH
#pragma endregion

#pragma region Firmware version
// Bump this number on every release (and manifest.json for the web installer).
// Shown on the settings screen and over serial at boot.
#define FW_VERSION "1.24"
#pragma endregion

#pragma region Static screen redraw
// Static screens (settings, keyboard, info card pages 1-3, a battle where nothing
// animates) are only redrawn when screenDirty is set (every touch event, screen
// change and language change sets it) or at least every STATIC_SCREEN_REFRESH_MS
// as a safety net for values that change on their own (clock, WiFi state).
#define STATIC_SCREEN_REFRESH_MS 1000
extern bool screenDirty;
extern uint32_t lastStaticScreenDrawMs;
inline void markScreenDirty() { screenDirty = true; }
// true when a static screen must be redrawn now (dirty, or the safety refresh is
// due); it then clears screenDirty and records nowMs as the last static draw
bool consumeStaticScreenRedraw(uint32_t nowMs);
#pragma endregion

#pragma region Text and night mode
// active CJK font: set by applyLangFont() for Japanese/Korean
extern bool gCjkFont;
// text scale ACTUALLY applied by setSize() (CJK fonts divide the requested size)
extern uint8_t gTextSize;
// px from the top edge to the baseline of the active font, 0 = classic font
extern int gFontAscent;
// real night (by the clock): set by render() for the main screen
extern bool gNight;
// night is 20:00 to 05:59 by the scene clock
bool isNightHour(int hour);
inline uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }
#pragma endregion

#pragma region Creature and its sprites
extern Pet pet;
// animated sprite from the SD for the current species (if the file exists)
extern SdMon mon;          // B/W sprite (fallback and minigame if there is no PMD)
extern PmdMon pmd;         // multi-action PMD sprite (main screen)
extern PmdMon evoPmd;      // previous form, only during the evolution flashing
extern PmdMon galleryPmd;  // large sprite for the gallery detail view and the wild battle opponent
extern int16_t monFor;     // species the loaded sprites belong to (-2 = none yet)
extern bool monShinyFor;

// on-screen creature behavior
enum PetBehaviorMode : uint8_t { PET_BEHAVIOR_IDLE = 0, PET_BEHAVIOR_WALK = 1, PET_BEHAVIOR_GESTURE = 2 };
struct PetBehavior {
  uint8_t mode = PET_BEHAVIOR_IDLE;  // PetBehaviorMode: idle, walk, one-shot gesture
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // start of the current animation
  uint32_t until = 0;   // end of the current state
  float x = CX, targetX = CX;
};
extern PetBehavior beh;

#define PET_GROUND SY(304)  // the pet's ground line
#define PET_CY SY(202)      // vertical center of the sprite

// screen box of the opaque pixels the creature drew last frame (coin pickup =
// the coin touching this box); filled by drawPmdFrameAtScale and drawMinigamePet
struct SpriteDrawnBounds { int16_t left, top, right, bottom; bool valid; };
extern SpriteDrawnBounds lastSpriteDrawnBounds;
#pragma endregion

#pragma region Pokedex gallery
extern bool galleryOpen;
extern bool galleryDirty;
extern int galleryPage;        // pages of GALLERY_PAGE_SIZE over the filtered list
extern int16_t galleryDetail;  // dex in detail view, 0 = grid
// All / Raised / Caught / Shiny buttons on the left of the grid
enum GalleryFilter : uint8_t { GALLERY_FILTER_ALL = 0, GALLERY_FILTER_RAISED, GALLERY_FILTER_CAUGHT, GALLERY_FILTER_SHINY,
                               GALLERY_FILTER_COUNT };
extern GalleryFilter galleryFilter;          // kept between visits (not saved to NVS)
extern int16_t galleryFilteredDex[DEX_COUNT];  // dex numbers that pass the filter, ascending
extern int16_t galleryFilteredCount;         // filled by rebuildGalleryFilteredList()
// set when the filtered list may be stale (filter change, catch, registration):
// the gallery rebuilds it lazily instead of on every frame
extern bool galleryFilterDirty;
#pragma endregion

#pragma region Screens and overlays
extern bool screenOff;  // short press of the PWR / BOOT button, or Screen Off in the settings menu
extern bool cardOpen;   // creature info card (tap the name on the main screen)
extern bool kbOpen;     // keyboard: renames the creature or asks for a WiFi password
extern uint8_t cardPage;  // 0 profile, 1 stats, 2 progress, 3 medals
#define CARD_PAGE_COUNT 4
extern uint32_t feedMenuUntil;  // food picker open until this millis
extern uint32_t playMenuUntil;  // play picker (ball game / coin chase / training bag / wild battle) open until this millis
#pragma endregion

#pragma region Keyboard state
enum KeyboardPurpose : uint8_t { KEYBOARD_FOR_NAME, KEYBOARD_FOR_WIFI_PASSWORD };
extern KeyboardPurpose keyboardPurpose;
extern char keyboardText[WIFI_PASSWORD_MAX_LENGTH + 1];  // fits a nickname (11) and a WPA passphrase (63)
extern uint8_t keyboardTextLength;
extern uint8_t keyboardTextMaxLength;
extern bool keyboardShiftOn;    // letters: upper case; 123 page: second symbols page
extern bool keyboardSymbolsOn;  // 123 page instead of the letters
#pragma endregion

#pragma region Hit boxes recorded while drawing (the touch code reads them)
extern int16_t headerNameLeft, headerNameWidth;          // name in drawHeader(): tap = info card
extern int16_t headerLevelRowLeft, headerLevelRowWidth;  // "Lv.N" + bar under the name: tap = info card too
extern int16_t cardPencilLeft, cardPencilTop;            // rename pencil on card page 0
extern int16_t cardPencilSize;                           // 0 = not drawn yet
#pragma endregion

#pragma region Settings screens state
// Settings menu (hamburger button, top left) and its sub-screens. Only one is open at a time;
// SETTINGS_CLOSED = main screen. The X button is the only way out (swipes are ignored).
enum SettingsScreen : uint8_t {
  SETTINGS_CLOSED,
  SETTINGS_MENU,
  SETTINGS_AUDIO_VIDEO,
  SETTINGS_TIME,
  SETTINGS_LANGUAGE,
  SETTINGS_WIFI
};
extern SettingsScreen settingsScreen;
extern uint8_t wifiListPage;                                     // page of the WiFi scan results
extern char wifiPasswordTargetSsid[WIFI_SSID_MAX_LENGTH + 1];  // network the password keyboard is for
extern int clockH, clockM;    // time being edited (always 0..23, also in 12h mode)
extern bool clockUses24Hour;  // time screen display format, persisted in NVS
// hour (0..23) as a 12-hour dial shows it: 0 -> 12, 13 -> 1 (time screen and header clock)
inline int hourOn12HourDial(int hour) {
  int hourInHalfDay = hour % 12;
  return hourInHalfDay == 0 ? 12 : hourInHalfDay;
}
// Brightness slider: level 0..SETTINGS_BRIGHTNESS_MAX maps linearly onto the
// panel range SCREEN_BRIGHTNESS_MIN..SCREEN_BRIGHTNESS_MAX (see
// brightnessForSettingsLevel). Persisted in NVS when the audio/video screen closes.
#define SETTINGS_BRIGHTNESS_MAX 10
#define SETTINGS_BRIGHTNESS_DEFAULT 7  // ~184, close to the old fixed USB brightness (180)
#define SCREEN_BRIGHTNESS_MIN 20       // level 0: dim but still readable
#define SCREEN_BRIGHTNESS_MAX 255      // level SETTINGS_BRIGHTNESS_MAX
extern uint8_t settingsBrightnessLevel;
extern uint8_t settingsBrightnessSavedLevel;  // last value in NVS (skips no-op writes)
#pragma endregion

#pragma region Bath scene
// shower over the creature (bath.cpp) and cleanup when it ends
extern uint32_t bathUntil;
extern bool bathPending;
#pragma endregion

#pragma region Ball game (keepy-uppy) state
extern bool gameOpen;
extern bool gameOver;           // results screen up (OK closes it)
extern uint8_t gameAttackGain;  // ATK training earned (results screen)
extern float ballX, ballY, ballVX, ballVY, gamePetX;
extern uint32_t lastGameStep;   // last stepGame() call: physics by real time, not per frame
extern uint8_t gameScore, gameMisses;
extern float hitX, hitY;        // last hit (impact ring)
extern uint32_t hitTime;
extern uint32_t ballLastHitAt;  // last accepted hit (hit cooldown)
extern bool gameNewHi;
#pragma endregion

#pragma region Ball tuning
// The fork's harder physics, tuned per frame at its 115 ms pace in 466 px units.
// stepGame() multiplies by k = frame time / BALL_FRAME_MS, and every distance,
// velocity and acceleration goes through SKF() (SK() without truncation), so the
// round board gets the fork's numbers exactly and the CYD the same feel at 240 px.
#define BALL_FRAME_MS 115.0f
#define BALL_FRAME_FACTOR_MAX 3.0f
// vertical tempo: vertical speeds are multiplied by this and gravity by its
// square, so the ball rises and falls this many times faster while every
// height (how high a tap sends it) stays the same. Horizontal motion is untouched.
#define BALL_VERTICAL_SPEED 1.4f
#define BALL_VERTICAL_ACCEL (BALL_VERTICAL_SPEED * BALL_VERTICAL_SPEED)
// respawn: horizontal speed grows with the score, plus jitter; it starts falling
#define BALL_RESPAWN_SPEED_BASE SKF(3.35f)
#define BALL_RESPAWN_SPEED_PER_POINT SKF(0.14f)
#define BALL_RESPAWN_SPEED_MAX SKF(8.4f)
#define BALL_RESPAWN_JITTER_STEP SKF(0.28f)  // times (random(9) - 4)
#define BALL_RESPAWN_VY (SKF(2.05f) * BALL_VERTICAL_SPEED)
// gravity: base + per point, two step-ups, capped
#define BALL_GRAVITY_BASE (SKF(1.14f) * BALL_VERTICAL_ACCEL)
#define BALL_GRAVITY_PER_POINT (SKF(0.046f) * BALL_VERTICAL_ACCEL)
#define BALL_GRAVITY_STEP1_SCORE 5
#define BALL_GRAVITY_STEP1 (SKF(0.14f) * BALL_VERTICAL_ACCEL)
#define BALL_GRAVITY_STEP2_SCORE 12
#define BALL_GRAVITY_STEP2 (SKF(0.18f) * BALL_VERTICAL_ACCEL)
#define BALL_GRAVITY_MAX (SKF(2.10f) * BALL_VERTICAL_ACCEL)
// wind: sine sway every frame plus a random kick (7 percent per 115 ms frame)
#define BALL_WIND_AMPLITUDE SKF(0.11f)
#define BALL_WIND_RATE 0.018f       // radians per ms
#define BALL_WIND_SCORE_PHASE_MS 97 // phase shift per point
#define BALL_KICK_CHANCE_PCT 7.0f
#define BALL_KICK_STEP SKF(0.22f)   // times (random(7) - 3)
// touch: radius shrinks at 8 and 20 points, never below BALL_HIT_RADIUS_MIN (layout.h)
#define BALL_HIT_RADIUS_START SK(50)
#define BALL_HIT_RADIUS_MID SK(44)
#define BALL_HIT_RADIUS_LATE SK(38)
#define BALL_HIT_COOLDOWN_MS 450
#define BALL_HIT_MIN_VY (SKF(-0.25f) * BALL_VERTICAL_SPEED)  // no hit while the ball still rises faster than this
// hit response: lift, drift toward the side opposite the finger, chaos, random reversal
// lift = base + per point * min(score, cap): each tap sends the ball a little
// higher than the last, outpacing the rising gravity, until the cap
#define BALL_LIFT_BASE (SKF(11.0f) * BALL_VERTICAL_SPEED)
#define BALL_LIFT_PER_POINT (SKF(0.4f) * BALL_VERTICAL_SPEED)
#define BALL_LIFT_SCORE_CAP 16
#define BALL_DRIFT_BASE 0.38f       // ratio of the finger offset (px) added to vx
#define BALL_DRIFT_STEP1_SCORE 6
#define BALL_DRIFT_STEP1 0.08f
#define BALL_DRIFT_STEP2_SCORE 14
#define BALL_DRIFT_STEP2 0.10f
#define BALL_CHAOS_STEP SKF(0.26f)  // times (random(17) - 8)
#define BALL_REVERSE_CHANCE_PCT 28  // vx = -vx * (0.72 .. 1.16)
#define BALL_MAX_VX SKF(11.5f)
// walls: side walls (and the round bezel) push vx out by 1.05, vertical by 0.88, plus noise
#define BALL_WALL_GAIN_X 1.05f
#define BALL_WALL_GAIN_Y 0.88f
#define BALL_WALL_NOISE_STEP SKF(0.18f)  // times (random(9) - 4)
#pragma endregion

#pragma region Coin chase minigame state
// Tap a spot and the creature walks there; each coin it touches is +1 point and
// +5 s. Ends at 0 s or 15 points. Layout (COIN_*): boards/layout.h
extern bool coinOpen;
extern bool coinOver;                     // results screen up (OK closes it)
extern float coinX, coinY;                // coin center
extern uint32_t coinSpawnedAt;            // millis when the current coin appeared
extern float coinPetX, coinPetFeetY;      // creature position (anchored by the feet)
extern float coinTargetX, coinTargetFeetY;  // where the last tap sent it
extern bool coinPetFacingLeft;
extern int32_t coinTimeLeftMs;
extern uint32_t lastCoinStep;             // last stepCoinGame() call: movement by real time
extern uint8_t coinPoints;
extern uint8_t coinSpeedGain;             // SPE training earned (results screen)
extern bool coinNewHi;
#pragma endregion

#pragma region Coin chase tuning
#define COIN_START_TIME_MS 20000L
#define COIN_BONUS_TIME_MS 2000L
#define COIN_MAX_TIME_MS 30000L    // the bonus never pushes the clock past 30 s
#define COIN_WIN_POINTS 15
#define COIN_STEP_MAX_MS 345       // abnormally late frame: do not take a huge jump
// walking speed in px/s from speStat(): linear between MIN and MAX, stat clamped at STAT_CAP
#define COIN_PET_SPEED_MIN SKF(110.0f)
#define COIN_PET_SPEED_MAX SKF(340.0f)
#define COIN_SPEED_STAT_CAP 250
#define COIN_SPAWN_ATTEMPTS 20
#define COIN_LIFETIME_MS 3000      // an untouched coin vanishes and another appears elsewhere
#pragma endregion

#pragma region Training bag state (trains ATK)
extern bool sackOpen;
extern uint32_t sackUntil;
extern bool sackOver;  // results screen up (OK closes it)
extern uint16_t sackHits;
extern float sackShake;
extern uint8_t sackGain;
extern bool sackNewHi;
#pragma endregion

#pragma region Wild battle state
// Manual turn-based battle against a wild species (battle.h). The wild sprite
// reuses galleryPmd: the gallery and a battle are never open together.
extern bool battleOpen;
extern bool battleResolved;        // the battle ended: result view
extern int16_t battleDex;
extern uint8_t battleLevel;
extern bool battleShiny;
extern BattleStats battlePlayer;
extern BattleStats battleEnemy;
extern BattleRuntime battleRun;
extern BattleTurnResult battleTurn;
extern BattleReward battleReward;
// true: accepted from the random wild prompt ("WILD BATTLE"); false: started
// from the play menu swords or a serial command ("BATTLE", lower catch chance)
extern bool battleFromWildEncounter;
// last turn's moves, drawn under each sprite. 64: a CJK label (3 bytes per
// character) plus a number
extern char battlePlayerMoveText[64];
extern char battleWildMoveText[64];
extern bool battleAttackMenuOpen;  // Quick / Attack / Heavy overlay over the action buttons
extern bool battleCatchOffered;
extern bool battleCatchTried;
extern bool battleCatchDone;
extern bool battleCatchSuccess;
extern bool battleRespectCatch;    // catch offered after a close loss
extern uint8_t battleCatchChance;
extern bool battleLowHpWarned;
#define BATTLE_RENDER_INTERVAL_MS 120  // two animated sprites: a bit lighter than the main screen
#pragma endregion

#pragma region Wild encounter prompt state
#define WILD_COOLDOWN_MS (20UL * 60UL * 1000UL)  // at most one prompt every 20 min
#define WILD_PROMPT_MS 20000UL                   // the prompt closes on its own after 20 s
#define WILD_CHECK_MS 60000UL                    // one roll per minute once the cooldown is over
#define WILD_SHINY_ODDS 4096                     // fixed 1 in 4096 (no step system here)
extern uint32_t wildPromptUntil;   // prompt open until this millis (0 = closed)
extern uint32_t nextWildEligible;  // 0 = not scheduled yet (first loop schedules it)
extern uint32_t lastWildCheck;
extern int16_t wildPromptDex;
extern uint8_t wildPromptLevel;
extern bool wildPromptShiny;
#pragma endregion

#pragma region Main screen action buttons
// icon buttons following the lower arc of the round screen (the outer ones
// sit higher so they do not stick out of the circle). On a rectangular
// board they go in a straight row spread near the bottom edge.
// BTN_HALF / BTN_HIT: boards/layout_common.h
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
  int8_t iconOffsetX, iconOffsetY;  // screen pixels added to the icon position (optical centering); omitted = 0
};
enum MainButtonIndex : uint8_t { MAIN_BUTTON_FOOD = 0, MAIN_BUTTON_PLAY, MAIN_BUTTON_LIGHT, MAIN_BUTTON_CLEAN, MAIN_BUTTON_COUNT };
extern Btn buttons[MAIN_BUTTON_COUNT];
#pragma endregion

#pragma region Picker items (food strip and play strip)
enum FeedPickerItem : uint8_t { FEED_PICK_APPLE = 0, FEED_PICK_BLUE_BERRY, FEED_PICK_GREEN_BERRY, FEED_PICK_CANDY,
                                FEED_PICK_COFFEE, FEED_PICK_COUNT };
enum PlayPickerItem : uint8_t { PLAY_PICK_BALL_GAME = 0, PLAY_PICK_COIN_CHASE, PLAY_PICK_TRAINING_BAG,
                                PLAY_PICK_WILD_BATTLE, PLAY_PICK_COUNT };
#pragma endregion

#pragma region Starter choice
// first game: Bulbasaur / Charmander / Squirtle, 3 rows
// STARTER_*, EVO_BTN_*, FAR_BTN_* and the rest of the menu boxes: boards/layout.h
#define STARTER_COUNT 3
extern const int16_t STARTER_DEX[STARTER_COUNT];
#pragma endregion

#pragma region Touch gesture and dialog state
extern bool wasPressed;
extern uint32_t lastRender;
// AMOLED protection: dimming on inactivity
extern uint32_t lastInteract;
extern uint8_t dimStage;        // 0 awake, 1 dimmed (90s), 2 nearly off (5min)
extern bool swallowGesture;     // the wake-up touch does not trigger anything
extern uint32_t holdStart;      // long press on the creature
extern uint32_t confirmUntil;   // "release?" dialog active until this millis
// decision dialog kind (choiceKind holds one of these)
enum ChoiceKind : uint8_t { CHOICE_NONE = 0, CHOICE_EVOLVE = 1, CHOICE_FAREWELL = 2 };
extern uint8_t choiceKind;      // ChoiceKind: 0 none, 1 evolution, 2 farewell
extern uint32_t choiceUntil;    // closes on its own at this millis
extern int16_t tX0, tY0, tXl, tYl;  // gesture in progress (start and last position)
extern uint32_t tStart;
extern bool holdFired;
#pragma endregion

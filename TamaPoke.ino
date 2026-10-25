// TamaPoke - pixel art tamagotchi inspired by gen 1
// Supports TWO boards (see boards/board_select.h):
//   - Waveshare ESP32-S3-Touch-AMOLED-1.75 (round 466x466): ORIGINAL behavior,
//     no build flags. Instructions below.
//   - ESP32-2432S028R "Cheap Yellow Display" / ELEGOO 2.8" (rect. 320x240): build
//     with -D BOARD_CYD_RECT (PlatformIO: pio run -e cyd-rect). See CYD_PORT_NOTES.md
//     for known limitations (memory, uncalibrated touch, no RTC/battery).
//
// (Instructions for the original Waveshare board:)
//
// Libraries (Library Manager or the Waveshare repo):
//   - "GFX Library for Arduino" (moononournation), with CO5300 QSPI support
//   - "SensorLib" (Lewis He), CST9217 touch driver
//
// Board: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// The sprites and the species table are generated with tools/sprites.py (emit).

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include <U8g2lib.h>  // CJK fonts (Japanese); see applyLangFont()
#include <Preferences.h>  // NVS: 12h/24h choice of the time screen
#include "boards/board_select.h"  // picks the board (round Waveshare vs rect. CYD)
#include "boards/layout.h"        // CX/CY/HORIZON/SX()/SY()/minigame area
#include "band_canvas.h"          // CYD framebuffer split into bands (no PSRAM)
#include "touch_hal.h"            // touch: capacitive (Waveshare) or resistive (CYD)
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"
#include "ui_icons.h"  // close button, pencil, page arrows and their hit tests
#include "battle.h"    // wild battles: turn engine, wild species/level picks, day phase

// Firmware version. Bump this number on every release (and manifest.json for
// the web installer). Shown on the settings screen and over serial at boot.
#define FW_VERSION "1.17"

// Secondary text (labels, version): darker than UI_TRACK, lighter than UI_INK.
// Here and not in species.h, which tools/sprites.py regenerates.
#define UI_LABEL C565(0x6B, 0x66, 0x58)

#if BOARD_IS_ROUND
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// Full framebuffer in PSRAM: we draw everything and then flush() (no flicker)
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
#else
// CYD: ILI9341 320x240 over 4-wire SPI on the HSPI bus (14/12/13/15), which is
// exactly what the library assumes by default for HSPI. rotation=1 -> landscape.
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, HSPI);
Arduino_ILI9341 *panel = new Arduino_ILI9341(bus, TFT_RST, 1 /*rotation*/, false /*ips*/);
// Full framebuffer (320x240x2 = 150 KB) in INTERNAL RAM: the CYD has no PSRAM
// and 150 KB in one piece never fits (largest free block ~110 KB), so
// BandCanvas splits it into 64-row bands (see band_canvas.h). If even the bands
// do not fit, setup() falls back to drawing directly on the panel (gfx = panel;
// works but flickers). The base type Arduino_GFX* is used so they can be
// swapped; flush() is virtual in the base.
static BandCanvas *fbCanvas = new BandCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
Arduino_GFX *gfx = fbCanvas;
#endif

// Brightness: on the CO5300 the panel itself controls it; on the CYD it is the backlight (PWM)
static void boardSetBrightness(uint8_t v) {
#if BOARD_IS_ROUND
  panel->setBrightness(v);
#else
  ledcWrite(TFT_BL, v);  // per-pin ledc API (ESP32 Arduino core 3.x)
#endif
}

// Full-screen background. On the round one it is a circle with the bezel's
// radius (231 px); on a rectangular one there is no bezel to mimic: fill everything.
inline void fillScreenBg(uint16_t color) {
#if BOARD_IS_ROUND
  gfx->fillCircle(CX, CY, 231, color);
#else
  gfx->fillScreen(color);
#endif
}
// active CJK font: always false for now, loading will come with
// Japanese/Korean/Chinese support (see PR #13)
bool gCjkFont = false;
Pet pet;

// animated sprite from the SD for the current species (if the file exists)
SdMon mon;          // B/W sprite (fallback and minigame if there is no PMD)
PmdMon pmd;         // multi-action PMD sprite (main screen)
PmdMon evoPmd;      // previous form, only during the evolution flashing
int16_t monFor = -2;
bool monShinyFor = false;

// on-screen creature behavior
struct {
  uint8_t mode = 0;     // 0 idle, 1 walk, 2 one-shot gesture
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // start of the current animation
  uint32_t until = 0;   // end of the current state
  float x = CX, targetX = CX;
} beh;
#define PET_GROUND SY(304)  // the pet's ground line
PmdMon galleryPmd;  // large sprite for the gallery detail view (PMD/TPK2, license-compliant)

// pokedex gallery
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 10 pages of 16
int16_t galleryDetail = 0;  // dex in detail view, 0 = grid

bool screenOff = false;       // short press of the PWR / BOOT button, or Screen Off in the settings menu
bool cardOpen = false;        // creature info card (tap the name on the main screen)
bool kbOpen = false;          // keyboard to rename the creature
char nameBuf[12] = "";
uint8_t nameLen = 0;
uint8_t cardPage = 0;         // 0 profile, 1 stats, 2 medals, 3 progress
#define CARD_PAGE_COUNT 4
//#region Hit boxes recorded while drawing (the touch code reads them)
int16_t headerNameLeft = 0, headerNameWidth = 0;  // name in drawHeader(): tap = info card
int16_t headerLevelRowLeft = 0, headerLevelRowWidth = 0;  // "Lv.N" + bar under the name: tap = info card too
int16_t cardPencilLeft = 0, cardPencilTop = 0;    // rename pencil on card page 0
int16_t cardPencilSize = 0;                       // 0 = not drawn yet
//#endregion
#pragma region Settings screens state
// Settings menu (hamburger button, top left) and its sub-screens. Only one is open at a time;
// SETTINGS_CLOSED = main screen. The X button is the only way out (swipes are ignored).
enum SettingsScreen : uint8_t {
  SETTINGS_CLOSED,
  SETTINGS_MENU,
  SETTINGS_AUDIO_VIDEO,
  SETTINGS_TIME,
  SETTINGS_LANGUAGE
};
SettingsScreen settingsScreen = SETTINGS_CLOSED;
int clockH = 12, clockM = 0;  // time being edited (always 0..23, also in 12h mode)
bool clockUses24Hour = true;  // time screen display format, persisted in NVS
// Brightness slider: level 0..SETTINGS_BRIGHTNESS_MAX maps linearly onto the
// panel range SCREEN_BRIGHTNESS_MIN..SCREEN_BRIGHTNESS_MAX (see
// brightnessForSettingsLevel). Persisted in NVS when the audio/video screen closes.
#define SETTINGS_BRIGHTNESS_MAX 10
#define SETTINGS_BRIGHTNESS_DEFAULT 7  // ~184, close to the old fixed USB brightness (180)
#define SCREEN_BRIGHTNESS_MIN 20       // level 0: dim but still readable
#define SCREEN_BRIGHTNESS_MAX 255      // level SETTINGS_BRIGHTNESS_MAX
uint8_t settingsBrightnessLevel = SETTINGS_BRIGHTNESS_DEFAULT;
uint8_t settingsBrightnessSavedLevel = SETTINGS_BRIGHTNESS_DEFAULT;  // last value in NVS (skips no-op writes)
#pragma endregion

// bath scene: foam over the creature and cleanup when it pops
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;   // food picker open until this millis
uint32_t playMenuUntil = 0;   // play picker (ball game / wild battle) open until this millis

// "keepy-uppy" minigame: keep the pokeball in the air
bool gameOpen = false;
bool gameOver = false;        // results screen up (OK closes it)
uint8_t gameAttackGain = 0;   // ATK training earned (results screen)
float ballX, ballY, ballVX, ballVY, gamePetX;
uint32_t lastGameStep = 0;  // last stepGame() call: physics by real time, not per frame
uint8_t gameScore, gameMisses;
float hitX, hitY;             // last hit (impact ring)
uint32_t hitTime = 0;
uint32_t ballLastHitAt = 0;   // last accepted hit (hit cooldown)
bool gameNewHi = false;

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
bool coinOpen = false;
bool coinOver = false;                     // results screen up (OK closes it)
float coinX, coinY;                        // coin center
uint32_t coinSpawnedAt = 0;                // millis when the current coin appeared
// screen box of the opaque pixels the creature drew last frame (coin pickup =
// the coin touching this box); filled by drawPmdFrameAtScale and drawMinigamePet
struct SpriteDrawnBounds { int16_t left, top, right, bottom; bool valid; };
SpriteDrawnBounds lastSpriteDrawnBounds = {};
float coinPetX, coinPetFeetY;              // creature position (anchored by the feet)
float coinTargetX, coinTargetFeetY;        // where the last tap sent it
bool coinPetFacingLeft = false;
int32_t coinTimeLeftMs = 0;
uint32_t lastCoinStep = 0;                 // last stepCoinGame() call: movement by real time
uint8_t coinPoints = 0;
uint8_t coinSpeedGain = 0;                 // SPE training earned (results screen)
bool coinNewHi = false;
#pragma endregion

#pragma region Coin chase tuning
#define COIN_START_TIME_MS 30000L
#define COIN_BONUS_TIME_MS 5000L
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

// training bag (trains strength)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

#pragma region Wild battle state
// Manual turn-based battle against a wild species (battle.h). The wild sprite
// reuses galleryPmd: the gallery and a battle are never open together.
bool battleOpen = false;
bool battleResolved = false;        // the battle ended: result view
int16_t battleDex = 0;
uint8_t battleLevel = 1;
bool battleShiny = false;
BattleStats battlePlayer = {};
BattleStats battleEnemy = {};
BattleRuntime battleRun = {};
BattleTurnResult battleTurn = {};
BattleReward battleReward = {};
// last turn's moves, drawn under each sprite. 64: a CJK label (3 bytes per
// character) plus a number
// true: accepted from the random wild prompt ("WILD BATTLE"); false: started
// from the play menu swords or a serial command ("BATTLE", lower catch chance)
bool battleFromWildEncounter = false;
char battlePlayerMoveText[64] = "";
char battleWildMoveText[64] = "";
bool battleAttackMenuOpen = false;  // Quick / Heavy overlay over the action buttons
bool battleCatchOffered = false;
bool battleCatchTried = false;
bool battleCatchDone = false;
bool battleCatchSuccess = false;
bool battleRespectCatch = false;    // catch offered after a close loss
uint8_t battleCatchChance = 0;
bool battleLowHpWarned = false;
#define BATTLE_RENDER_INTERVAL_MS 120  // two animated sprites: a bit lighter than the main screen
#pragma endregion

#pragma region Wild encounter prompt state
#define WILD_COOLDOWN_MS (20UL * 60UL * 1000UL)  // at most one prompt every 20 min
#define WILD_PROMPT_MS 20000UL                   // the prompt closes on its own after 20 s
#define WILD_CHECK_MS 60000UL                    // one roll per minute once the cooldown is over
#define WILD_SHINY_ODDS 4096                     // fixed 1 in 4096 (no step system here)
uint32_t wildPromptUntil = 0;   // prompt open until this millis (0 = closed)
uint32_t nextWildEligible = 0;  // 0 = not scheduled yet (first loop schedules it)
uint32_t lastWildCheck = 0;
int16_t wildPromptDex = 0;
uint8_t wildPromptLevel = 1;
bool wildPromptShiny = false;
#pragma endregion

// the 9 species with their own sprite in flash (fallback without SD): dex -> index
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

// CX/CY/HORIZON live in boards/layout.h (derived from the board)
#define PET_CY SY(202)  // vertical center of the sprite

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// icon buttons following the lower arc of the round screen (the outer ones
// sit higher so they do not stick out of the circle). On a rectangular
// board they go in a straight row spread near the bottom edge.
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
  int8_t iconOffsetX, iconOffsetY;  // screen pixels added to the icon position (optical centering); omitted = 0
};
// SPR_ICON_CONTROLLER (16x16) fills columns 0..15 and rows 3..12: its visual
// center matches the cell center, so it needs no optical offset.
#if BOARD_IS_ROUND
Btn buttons[4] = {
  { 140, 390, SPR_ICON_FOOD },   // eat
  { 202, 404, SPR_ICON_CONTROLLER },   // play (opens the play menu)
  { 264, 404, SPR_ICON_LIGHT },  // light
  { 326, 390, SPR_ICON_CLEAN },  // bath
};
#else
Btn buttons[4] = {
  { (int16_t)(LCD_WIDTH * 1 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_FOOD },
  { (int16_t)(LCD_WIDTH * 3 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_CONTROLLER },
  { (int16_t)(LCD_WIDTH * 5 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_LIGHT },
  { (int16_t)(LCD_WIDTH * 7 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_CLEAN },
};
#endif
// a single #define: PlatformIO's .ino conversion (-fpreprocessed -dD) sees both
// #if branches and would warn about a redefinition
#define BTN_HALF (BOARD_IS_ROUND ? 26 : 18)  // 52x52 button; smaller on the rect board (much shorter screen)
#define BTN_HIT 36   // touch radius (a bit more generous)

// egg cracks ('k' pixels over the sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
//#region Night mode stars
// x/y in 466-space (scaled with SX/SY), size in px drawn literally,
// brightness 0-16 (16 = full white, lower = lerp565 toward the local sky)
struct NightStar { uint16_t x, y; uint8_t size, brightness; };
static const NightStar STARS[] = {
  {120, 140, 4, 16}, {330, 130, 4, 16}, {385, 190, 2, 10}, { 75, 205, 2,  9},
  {275,  95, 3, 14}, {170, 100, 2, 16}, { 68, 125, 3, 12}, {410, 140, 2, 12},
  {225, 105, 2,  9}, {300, 170, 2,  8}, {235, 160, 3, 13}, {160, 185, 2, 11},
  {432, 205, 2,  8}, {112,  92, 2, 10},
};
// these stars dim to STAR_TWINKLE_DIM_BRIGHTNESS for one step out of every
// STAR_TWINKLE_CYCLE_STEPS, offset per star so they never dim together
static const uint8_t STAR_TWINKLE_INDICES[] = { 5, 7, 11 };
static const uint32_t STAR_TWINKLE_STEP_MS = 400;
static const uint8_t STAR_TWINKLE_CYCLE_STEPS = 7;
static const uint8_t STAR_TWINKLE_DIM_BRIGHTNESS = 5;
//#endregion

bool wasPressed = false;
// starter choice (first game): Bulbasaur / Charmander / Squirtle, 3 rows
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
// STARTER_*, EVO_BTN_*, FAR_BTN_* and the rest of the menu boxes: boards/layout.h
// (a single definition for both drawing and touch detection)
void ctext(const char *s, int n, int y, int maxW = 0);
void drawGlyphCenteredText(const char *text, int requestedSize, int y, int maxWidth = 0);
void drawThumbCell(const uint8_t *b, int x, int y, int s, bool sil, int cell);
int fitSize(const char *s, int n, int maxW);  // centered text (defined next to centerX)
// PlatformIO does not generate the prototype for functions returning 'const char *'
const char *eggMsg();
const char *statusMsg();
#pragma region Wild battle prototypes (defined after the minigame)
void startBattleWith(int16_t forcedDex, uint8_t forcedLevel, int8_t forcedShiny = -1, bool fromWildEncounter = false);
void startBattle();
void openWildPrompt(int16_t wildDex, uint32_t now);
void maybeOfferWildEncounter(uint32_t now);
void battleTap(int16_t x, int16_t y);
void renderBattle();
void drawWildPrompt();
void drawTextCenteredInBox(const char *text, int preferredSize, int boxLeft, int boxTop, int boxWidth, int boxHeight);
int shrinkTextSizeToWidth(const char *text, int preferredSize, int maxWidth);
#pragma endregion
// the CST9217 signals on the INT pin when touch data is available; we use it to
// avoid reading the I2C bus while the chip is asleep (that read hung for ~1s)
// gTouchIrq/touchIsr (CST9217) and the XPT2046 polling live in touch_hal_*.cpp
uint32_t lastRender = 0;
// AMOLED protection: dimming on inactivity
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 awake, 1 dimmed (90s), 2 nearly off (5min)
bool swallowGesture = false; // the wake-up touch does not trigger anything
uint32_t holdStart = 0;     // long press on the creature
uint32_t confirmUntil = 0;  // "release?" dialog active until this millis
uint8_t choiceKind = 0;     // decision dialog: 0 none, 1 evolution, 2 farewell
uint32_t choiceUntil = 0;   // closes on its own at this millis
int16_t tX0, tY0, tXl, tYl; // gesture in progress (start and last position)
uint32_t tStart = 0;
bool holdFired = false;

void setup() {
  Serial.setRxBufferSize(8192);  // the SD transfer arrives in 2 KB blocks
  Serial.begin(115200);
  // CRITICAL: without this, Serial.print BLOCKS the game when no serial
  // monitor is open on the host (the USB CDC TX buffer fills up and nobody
  // drains it) -> with timeout 0 the messages are dropped
#if BOARD_IS_ROUND
  Serial.setTxTimeoutMs(0);  // USB CDC only (S3); the CYD uses UART, whose TX does drain
#endif
  Serial.printf("TamaPoke fw v%s\n", FW_VERSION);
  loadLang();  // saved language (ES by default)
  loadClockFormatFromNvs();  // 12h/24h choice of the time screen
  loadBrightnessLevelFromNvs();  // brightness slider of the audio/video screen
#if BOARD_WAVESHARE_ROUND
  Wire.begin(IIC_SDA, IIC_SCL);
  // CST9217 (touch), AXP2101 (PMU) and PCF85063 (RTC) share this I2C bus.
  // Safety net for PMU/RTC (SensorLib does NOT honor this timeout for the
  // touch; the sleeping-touch hang is solved by gating on INT, see
  // handleTouch).
  Wire.setTimeOut(50);
#endif  // the CYD does not use I2C

  // CRITICAL: turn on the panel power (BLDO1=OLED VDD 3.3V) BEFORE initializing
  // the display. If the PMU reset (full drain), this rail stays OFF and the
  // screen looks black even though the rest of the board works.
  pmuEnablePanel();

  // QSPI at 80MHz (default 40): the framebuffer flush is the fps bottleneck
  // (~56ms at 40MHz). If the panel shows garbage, drop back to 40M.
#if BOARD_IS_ROUND
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() fallo");
  boardSetBrightness(180);
#else
  ledcAttach(TFT_BL, 5000, 8);
  boardSetBrightness(0);  // no white flash until there is an image
  if (!panel->begin(40000000)) Serial.println("gfx->begin() fallo");  // 40 MHz: ILI9341's reliable maximum
  Serial.printf("CYD heap: libre=%u mayor_bloque=%u (framebuffer necesita %u)\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)(LCD_WIDTH * LCD_HEIGHT * 2));
  // the panel is already initialized: GFX_SKIP_OUTPUT_BEGIN only allocates the
  // bands, and if they do not fit it draws straight to the panel
  bool useCanvas = fbCanvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  gfx = useCanvas ? (Arduino_GFX *)fbCanvas : (Arduino_GFX *)panel;
  if (useCanvas) Serial.printf("CYD: framebuffer en bandas OK, heap libre=%u\n", (unsigned)ESP.getFreeHeap());
  else Serial.println("CYD: framebuffer NO cabe -> dibujo directo (parpadeo)");
  boardSetBrightness(180);
#endif
  applyLangFont();  // font for the saved language (classic unless CJK)

  touchHalBegin();

  pet.begin();
  sdBegin();
  thumbs.load();

  // real clock: applies the time it was powered off
  rtcBegin();
  batBegin();
  pwrSetup();
  uint32_t e = rtcEpoch();
  if (e == 0) {
    // Without a backup cell, the PCF85063 loses the time when power is cut.
    // Always seeding a fixed date would make game time go BACKWARDS relative
    // to what was already saved, and that breaks the streak, the age and the
    // scene's time of day. It is seeded with the last time seen, if later, so
    // time never goes backwards. The minutes spent off are lost anyway
    // (without an RTC there is no way to know them), but nothing gets out of sync.
    uint32_t seed = 1767225600UL;             // truly blank RTC
    uint32_t seen = pet.savedEpoch();         // uses the NVS pet already has open
    if (seen > seed) seed = seen;
    rtcSetEpoch(seed);
    e = rtcEpoch();
    Serial.printf("RTC sin hora: sembrado en %u%s\n", seed,
                  seen > 1767225600UL ? " (desde la ultima hora guardada)" : "");
  }
  pet.syncClock(e);

  audioBegin();  // ES8311 + I2S + amplifier (plays a startup jingle)

  lastInteract = millis();
}

// loads/unloads the SD sprite when the species changes
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  // freshly received files may include a new thumbs.bin, which was only
  // loaded in setup(): without this it did not show in the gallery until a reboot
  if (sdDirty) { thumbs.unload(); thumbs.load(); }
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = CX;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // main: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // fallback: B/W
  }
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // the amplifier follows the sleep state (the call returns early if unchanged).
  // Here it covers every path: the light button, night sleep and sleep that
  // arrives applied from offline progression.
  audioSetSleeping(pet.sleeping);

  // plays a sound when the creature becomes ready to evolve (including the
  // case where it qualifies on waking up). canEvolveNow is false while asleep.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // somber warning when the creature is about to run away from neglect
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;

  handleTouch();
  handleSerial();
  ensureMon();
  maybeOfferWildEncounter(now);  // random wild prompt (only on the idle main screen)

  // short PWR press (BOOT button on the CYD): screen on/off
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    if (pwrShortPressed()) {
      if (screenOff) {
        screenOff = false;
        lastInteract = now;
      } else {
        turnScreenOff();
      }
    }
  }

  updateBrightness(now);

  // flushes the periodic autosave ONLY with the screen dimmed/off or while
  // sleeping: writing to NVS freezes both cores for ~1s (flash cache off), and
  // here there is no animation to interrupt nor a finger waiting for a response.
  // After 90s of inactivity the screen already dims, so it flushes right away;
  // active use still persists through each action's saves (eat/play/...).
  if (pet.savePending() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }

  // records the real time every 30 s (persisted on every game save)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // health heartbeat every 5 min (for the soak test; dropped if no monitor is attached)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u bat=%d%% mv=%d chg=%d usb=%d dim=%u off=%d\n",
                  (unsigned long)(now / 1000), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  batPercent(), batMillivolts(), batCharging() ? 1 : 0,
                  usbPresent() ? 1 : 0, dimStage, screenOff ? 1 : 0);
  }

  // 85 ms in game/bag: safe margin so the redraw does not stomp on the previous
  // frame's DMA transfer (at 40-65 ms it overlapped and caused black flashes;
  // with big sprites drawing takes longer, so some slack is left)
  // Nothing is drawn with the screen off: at brightness 0 nothing is visible,
  // but the redraw and DMA flush of the 466x466 framebuffer kept running at
  // 10 fps, and measured that is 70 ms of work out of every 100 (46 of them just
  // the flush). It is the board's biggest avoidable load. Each render repaints
  // the whole scene, so nothing is left half-done; and since lastRender stays
  // frozen, the first frame after waking comes out immediately.
  uint32_t renderIntervalMs = (gameOpen || coinOpen || sackOpen) ? 85 : battleOpen ? BATTLE_RENDER_INTERVAL_MS : 100;
  if (!screenOff && now - lastRender >= renderIntervalMs) {
    lastRender = now;
    render();
  }
}

// panel brightness for the user's slider level
uint8_t brightnessForSettingsLevel() {
  return SCREEN_BRIGHTNESS_MIN +
         (SCREEN_BRIGHTNESS_MAX - SCREEN_BRIGHTNESS_MIN) * settingsBrightnessLevel / SETTINGS_BRIGHTNESS_MAX;
}

// true when render() draws the plain main screen: no other screen and no dialog/menu over it
bool isMainScreenShowing() {
  return !pet.awaitingStarter() && !galleryOpen && !gameOpen && !coinOpen && !battleOpen && !sackOpen && !kbOpen &&
         settingsScreen == SETTINGS_CLOSED && !cardOpen && !feedMenuUntil && !playMenuUntil &&
         !wildPromptUntil && !confirmUntil && !choiceKind;
}

// Screen Off (settings menu, or the PWR / BOOT button): the panel goes dark on the
// next updateBrightness() and the creature goes to sleep (toggleLight already
// skips eggs and ceremonies). Turning the screen back on does not wake it.
// Round board: a touch or the PWR button wakes the screen. CYD: only the BOOT button.
void turnScreenOff() {
  screenOff = true;
  if (!pet.sleeping) pet.toggleLight();
}

// brightness based on the user's level, sleep + inactivity (AMOLED protection)
void updateBrightness(uint32_t now) {
  // visible events wake the screen on their own
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t normalBrightness = brightnessForSettingsLevel();
  // the sleep dim only applies to the main screen: any other screen or menu
  // opened while the creature sleeps comes back to the normal brightness
  bool sleepDim = pet.sleeping && isMainScreenShowing();
  uint8_t target = sleepDim ? 25 : normalBrightness;
  if (dimStage == 1) target = sleepDim ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (target > normalBrightness) target = normalBrightness;  // dimming never brightens a low setting
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    boardSetBrightness(target);
  }
}

// ---------- serial console (SD provisioning + debugging) ----------

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("especie #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.ageMinutes = (uint32_t)line.substring(4).toInt() * MINUTES_PER_LEVEL;
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // RTC only (simulate power-offs in testing)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simulates 20 egg rolls (does not change the game state)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // toggles shiny on the current one (testing)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simulates a new day of care (testing)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line.startsWith("BATTLE ")) {  // battle against a forced species (testing)
    int forcedDex = line.substring(7).toInt();
    if (forcedDex >= 1 && forcedDex <= DEX_COUNT) startBattleWith((int16_t)forcedDex, battleLevelFor(pet.level()));
    Serial.printf("battle=%d #%d %s lv=%u shiny=%d caught=%d\n", battleOpen, battleDex,
                  (battleDex >= 1 && battleDex <= DEX_COUNT) ? DEX_TBL[battleDex].name : "?",
                  battleLevel, battleShiny, pet.isCaught(battleDex));
    Serial.println("DONE");
  } else if (line == "BATTLE") {  // random wild battle (testing)
    startBattle();
    Serial.printf("battle=%d #%d %s lv=%u shiny=%d caught=%d\n", battleOpen, battleDex,
                  (battleDex >= 1 && battleDex <= DEX_COUNT) ? DEX_TBL[battleDex].name : "?",
                  battleLevel, battleShiny, pet.isCaught(battleDex));
    Serial.println("DONE");
  } else if (line == "WILD") {  // opens the wild encounter prompt now (testing)
    if (canStartWildBattle(pet.isEgg(), pet.sleeping, pet.ceremony))
      openWildPrompt(pickWildSpecies((uint8_t)random(100), dayPhaseFromHour(sceneHour())), millis());
    Serial.printf("wild=%d #%d lv=%u shiny=%d\n", wildPromptUntil != 0, wildPromptDex,
                  wildPromptLevel, wildPromptShiny);
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // audio test
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // forces the "ready to run away" state (button test)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    pet.factoryReset();     // wipes NVS and reboots -> new game (starter choice)
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/151:", pet.registeredCount());
    for (int i = 1; i <= 151; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u comida=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
  }
}

// ---------- touch input ----------

// The box follows the creature as it walks (beh.x), so it can be caressed
// anywhere it wanders; SX(123) is the half-width the fixed box used to have.
bool inPetZone(int16_t x, int16_t y) {
  return abs(x - (int)beh.x) < SX(123) && y > SY(95) && y < SY(310);
}

// the touch is resolved when the finger LIFTS, to tell a tap from a swipe
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz is plenty for a finger
  lastPoll = millis();
  // we only touch the bus if the chip signaled on INT or if the finger is still
  // down (the lift has to be detected). Reading the sleeping CST9217 hung ~1s
  // and froze the whole loop; SensorLib does not honor the Wire timeout.
  // (the INT gating and the CST9217 anti-hang "address-only cycle" now live
  // in touch_hal_waveshare.cpp, with their original comments)
  if (!touchHalHasEvent() && !wasPressed) return;
  int16_t x, y;
  bool pressed = touchHalGetPoint(&x, &y);

#if BOARD_CYD_RECT
  #pragma region Screen off on the CYD: touch is ignored, only the BOOT button turns it back on
  // (the resistive panel also reads stray presses, which must not light the screen)
  if (screenOff) {
    wasPressed = pressed;
    return;
  }
  #pragma endregion
#endif

  // training bag: each touch counts instantly (fast mashing)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (y < SY(72)) sackOpen = false;  // touching the top = quit
      else sackTap();
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // gesture starts
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // if it was dark, only wake up
    screenOff = false;
    lastInteract = millis();
  } else if (pressed) {  // still held down
    tXl = x;
    tYl = y;
    // long press without moving on the creature -> release dialog
    if (!holdFired && !swallowGesture && !galleryOpen && !cardOpen && !kbOpen && !battleOpen && !wildPromptUntil &&
        settingsScreen == SETTINGS_CLOSED && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // finger lifted: resolve gesture
    lastInteract = millis();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > SX(80) && abs(dy) < SY(70) && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// The settings menu opens with the hamburger button and the info card by tapping
// the creature's name (see onTap); the main screen has no swipe gestures.
void openClock();         // prototype
void openSettingsMenu();  // prototype

// swipe: dir +1 = to the right. Only acts inside the info card and the gallery.
void onSwipe(int dir) {
  if (pet.awaitingStarter()) return;  // blocked during the starter choice
  if (gameOpen || coinOpen || battleOpen || kbOpen || settingsScreen != SETTINGS_CLOSED) return;
  if (cardOpen) {  // inside the card: switch between the 4 pages
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // left advances
    cardPage = p < 0 ? 0 : (p > 3 ? 3 : p);
    return;
  }
  if (!galleryOpen) return;  // main screen: no swipe action
  if (galleryDetail) return;  // in detail: only the X goes back to the grid
  int np = galleryPage - dir;  // swiping left advances a page
  if (np < 0) {                // going back from the first = exit
    closeGalleryToMainScreen();
    return;
  }
  if (np > 9) np = 9;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
  }
}

void handleInfoCardTap(int16_t x, int16_t y);  // prototype (defined next to renderCard)

void onTap(int16_t x, int16_t y) {
  // Serial.printf("TOUCH %d %d\n", x, y);  // diagnostic (silenced: floods the log)
  if (pet.awaitingStarter()) {  // first game: pick a starter
    for (int i = 0; i < 3; i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= STARTER_ROW_X && x <= STARTER_ROW_X + STARTER_ROW_W && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (settingsScreen != SETTINGS_CLOSED) {
    handleSettingsTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // no buttons during the farewell
  if (cardOpen) {
    handleInfoCardTap(x, y);
    return;
  }
  if (gameOpen) {
    gameTap(x, y);
    return;
  }
  if (coinOpen) {
    coinGameTap(x, y);
    return;
  }
  if (battleOpen) {
    battleTap(x, y);
    return;
  }
  #pragma region Wild encounter prompt: FIGHT starts the battle, LATER dismisses, elsewhere does nothing
  if (wildPromptUntil) {
    if (!timeLeft(wildPromptUntil)) {
      wildPromptUntil = 0;
    } else if (isTouchInsideRect(x, y, WILD_PROMPT_BTN_X, WILD_PROMPT_FIGHT_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H)) {
      startBattleWith(wildPromptDex, wildPromptLevel, wildPromptShiny ? 1 : 0, true);
    } else if (isTouchInsideRect(x, y, WILD_PROMPT_BTN_X, WILD_PROMPT_LATER_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H)) {
      wildPromptUntil = 0;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  #pragma endregion
  if (choiceKind) {          // decision dialog: action button (top) / keep (bottom)
    bool b1 = (x >= CH_BX && x <= CH_BX + CH_BW && y >= CH_Y + CH_B1_REL_Y && y <= CH_Y + CH_B1_REL_Y + CH_BH);  // action
    bool b2 = (x >= CH_BX && x <= CH_BX + CH_BW && y >= CH_Y + CH_B2_REL_Y && y <= CH_Y + CH_B2_REL_Y + CH_BH);  // keep / stay together
    if (choiceKind == 1) {                 // evolution
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) pet.declineEvolve();
    } else if (choiceKind == 2) {          // farewell
      if (b1) pet.startFarewell();
      else if (b2) pet.declineFarewell();
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // "release?" dialog: YES / NO
    if (timeLeft(confirmUntil) && x >= CONF_X + CONF_BTN_MARGIN && x <= CONF_X + CONF_BTN_MARGIN + CONF_BTN_W &&
        y >= CONF_Y + CONF_BTN_REL_Y && y <= CONF_Y + CONF_BTN_REL_Y + CONF_BTN_H) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  if (feedMenuUntil) {       // food picker
    if (timeLeft(feedMenuUntil) && y >= FEED_Y && y <= FEED_Y + FEED_H && x >= FEED_X && x <= FEED_X + FEED_W) {
      int item = (x - FEED_X) / FEED_ITEM;
      if (item > FEED_ITEM_COUNT - 1) item = FEED_ITEM_COUNT - 1;  // the exact right edge gives one past the last
      if (item == 4) pet.feedCoffee();
      else if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
    return;
  }
  #pragma region Play picker: item 0 = ball game, item 1 = coin chase, item 2 = wild battle; any other tap closes it
  if (playMenuUntil) {
    bool insideStrip = isTouchInsideRect(x, y, PLAY_MENU_X, PLAY_MENU_Y, PLAY_MENU_W, PLAY_MENU_H);
    bool pickerStillOpen = timeLeft(playMenuUntil) > 0;
    playMenuUntil = 0;
    if (pickerStillOpen && insideStrip) {
      int item = (x - PLAY_MENU_X) / PLAY_MENU_ITEM;
      if (item > PLAY_MENU_ITEM_COUNT - 1) item = PLAY_MENU_ITEM_COUNT - 1;  // the exact right edge gives one past the last
      sfxPlay(SFX_TAP);
      if (item == 0) startGame();
      else if (item == 1) startCoinGame();
      else startBattle();
    }
    return;
  }
  #pragma endregion
  //#region Tapping the Pokedex button (under the hamburger) opens the gallery
  // Checked before the hamburger: their hit circles overlap a little, and a tap
  // in that strip is closer to the Pokedex icon.
  if (isTouchInsideCircle(x, y, POKEDEX_BTN_X + 8 * POKEDEX_BTN_S, POKEDEX_BTN_Y + 8 * POKEDEX_BTN_S, POKEDEX_BTN_HIT_R)) {
    sfxPlay(SFX_TAP);
    openGallery();
    return;
  }
  //#endregion
  //#region Tapping the hamburger button (top left) opens the settings menu
  // Reached only with no dialog open: starter choice, ceremony, choice, release
  // and food picker all returned above. Checked before the egg tap and the name box.
  if (isTouchInsideCircle(x, y, HAMBURGER_X + HAMBURGER_W / 2, HAMBURGER_Y + HAMBURGER_H / 2, HAMBURGER_HIT_R)) {
    sfxPlay(SFX_TAP);
    openSettingsMenu();
    return;
  }
  //#endregion
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  //#region Tapping the creature's name (header) opens the info card
  // Same blockers the old swipe-up had: starter choice, ceremony, egg, dialogs
  // and overlays all returned above. The box spans from a little above the name
  // down to the level line, which stays above the pet zone and every button.
  bool onHeaderName = headerNameWidth > 0 &&
      isTouchInsideRect(x, y, headerNameLeft - HDR_NAME_HIT_PAD, HDR_NAME_Y - HDR_NAME_HIT_PAD,
                        headerNameWidth + 2 * HDR_NAME_HIT_PAD, HDR_MSG_Y - (HDR_NAME_Y - HDR_NAME_HIT_PAD));
  // the level row under the name: 16 = text height at size 2
  bool onHeaderLevelRow = headerLevelRowWidth > 0 &&
      isTouchInsideRect(x, y, headerLevelRowLeft - HDR_NAME_HIT_PAD, HDR_MSG_Y,
                        headerLevelRowWidth + 2 * HDR_NAME_HIT_PAD, 16 + HDR_NAME_HIT_PAD);
  if (onHeaderName || onHeaderLevelRow) {
    cardOpen = true;
    cardPage = 0;
    return;
  }
  //#endregion
  // evolution button: opens the evolve/keep dialog
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // ending buttons (same box): runaway is immediate; farewell opens a dialog
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      sfxPlay(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) feedMenuUntil = millis() + 6000;
      } else if (i == 1) {
        if (!pet.sleeping) playMenuUntil = millis() + 6000;  // picker: ball game / wild battle
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // touching the creature = caress
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) sfxPlay(SFX_HEART);
  }
}

// ---------- render ----------

bool gNight = false;  // real night (by the clock): set by render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- background scene: type's biome + real RTC time ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
// HORIZON defined in boards/layout.h (232 on the round one; proportional on the CYD)

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// night stars over a sky gradient of skyHeight rows (drawScene and the minigame)
void drawNightStars(uint32_t nowMs, uint16_t skyTop, uint16_t skyBottom, int skyHeight) {
  uint32_t twinkleSlot = nowMs / STAR_TWINKLE_STEP_MS;
  for (int i = 0; i < (int)(sizeof(STARS) / sizeof(STARS[0])); i++) {
    const NightStar &star = STARS[i];

    //#region Apply deterministic twinkle
    int effectiveBrightness = star.brightness;
    for (int j = 0; j < (int)sizeof(STAR_TWINKLE_INDICES); j++) {
      if (STAR_TWINKLE_INDICES[j] == i && (twinkleSlot + j * 3) % STAR_TWINKLE_CYCLE_STEPS == 0)
        effectiveBrightness = STAR_TWINKLE_DIM_BRIGHTNESS;
    }
    //#endregion

    //#region Pick star color and draw
    uint16_t starColor = UI_WHITE;
    if (effectiveBrightness < 16) {
      uint16_t localSkyColor = lerp565(skyTop, skyBottom, SY(star.y), skyHeight);
      starColor = lerp565(localSkyColor, UI_WHITE, effectiveBrightness, 16);
    }
    gfx->fillRect(SX(star.x), SY(star.y), star.size, star.size, starColor);
    //#endregion
  }
}

// hour of the day 0-23 (from the real time cached every 30s; 13 if there is no clock)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// daytime ground of each biome (at night it is blended toward night blue)
static const uint16_t BIOME_SOIL[6] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 meadow
  C565(0xdc, 0xca, 0x94),  // 1 beach (sand)
  C565(0x4f, 0x8a, 0x55),  // 2 forest
  C565(0x8a, 0x55, 0x44),  // 3 volcano
  C565(0xa8, 0x90, 0x6a),  // 4 mountain
  C565(0xe6, 0xee, 0xf5),  // 5 snow
};

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * SX(250)) % SX(560)) - SX(40);
    int cy = SY(70) + k * SY(34);
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // dawn
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // day
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // dusk

  // sky in bands
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, LCD_WIDTH, 8, lerp565(top, bot, y, HORIZON));

  // sun or moon (positions hand-placed for 466 px: SX()/SY() rescale them; on
  // the CYD they are approximate, pending eyeball tweaking on a real board)
  if (night) {
    gfx->fillCircle(SX(360), SY(78), 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(SX(370), SY(72), 22, lerp565(top, bot, SY(78), HORIZON));  // crescent
    drawNightStars(now, top, bot, HORIZON);
  } else if (h < 18) {
    gfx->fillCircle(SX(360), SY(84), 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(CX, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // setting sun
  }

  // biome soil (the beach sea is drawn with the biome details, over the hill)
  uint16_t soil = BIOME_SOIL[biome < 6 ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);

  // ground
  gfx->fillRect(0, HORIZON, LCD_WIDTH, LCD_HEIGHT - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, LCD_WIDTH + 120, 60, 30, hill);

  // biome details
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 0) {  // meadow: flowering bushes plus grass tufts
    //#region Meadow palette (day colors, blended toward night blue after dark)
    const uint16_t nightBlue = C565(0x0c, 0x12, 0x24);
    uint16_t bushShadeColor = C565(0x3c, 0x8a, 0x4c);
    uint16_t bushHighlightColor = C565(0x58, 0xb8, 0x68);
    uint16_t flowerColors[3] = { C565(0xf0, 0x8a, 0xa4), C565(0xff, 0xd9, 0x5e), C565(0xff, 0xff, 0xff) };
    if (night) {
      bushShadeColor = lerp565(bushShadeColor, nightBlue, 10, 16);
      bushHighlightColor = lerp565(bushHighlightColor, nightBlue, 10, 16);
      for (int colorIndex = 0; colorIndex < 3; colorIndex++)
        flowerColors[colorIndex] = lerp565(flowerColors[colorIndex], nightBlue, 10, 16);
    }
    //#endregion

    //#region Meadow layout (x and radius in 466-space, flower offsets in tenths of the bush radius)
    struct MeadowBushLayout { int16_t centerX466, radius466; int8_t mirror, flowerCount, colorShift; };
    static const MeadowBushLayout bushLayouts[4] = {
      { 62, 26, 1, 4, 0 },
      { 128, 17, -1, 3, 1 },
      { 338, 21, 1, 3, 2 },
      { 412, 28, -1, 4, 1 },
    };
    static const int8_t flowerOffsets[4][2] = { { -6, -12 }, { 4, -17 }, { 9, -8 }, { -12, -5 } };
    const int bushBaseY = HORIZON + SY(8);
    const int largeFlowerSize = SY(6);
    const int smallFlowerSize = SY(4);
    //#endregion

    //#region Bushes: two side lobes, main dome, flat base, highlight, flowers
    for (const MeadowBushLayout &bushLayout : bushLayouts) {
      const int bushCenterX = SX(bushLayout.centerX466);
      const int bushRadius = SY(bushLayout.radius466);
      const int mirror = bushLayout.mirror;

      //#region Bush body
      gfx->fillCircle(bushCenterX + mirror * (-8 * bushRadius / 10), bushBaseY + (-6 * bushRadius / 10),
                      7 * bushRadius / 10, bushShadeColor);
      gfx->fillCircle(bushCenterX + mirror * (8 * bushRadius / 10), bushBaseY + (-5 * bushRadius / 10),
                      6 * bushRadius / 10, bushShadeColor);
      gfx->fillCircle(bushCenterX, bushBaseY - bushRadius, bushRadius, bushShadeColor);
      gfx->fillRect(bushCenterX - bushRadius, bushBaseY - 4 * bushRadius / 10,
                    2 * bushRadius, 4 * bushRadius / 10 + 1, bushShadeColor);
      gfx->fillCircle(bushCenterX + mirror * (-3 * bushRadius / 10), bushBaseY + (-13 * bushRadius / 10),
                      4 * bushRadius / 10, bushHighlightColor);
      //#endregion

      //#region Flowers (alternating 3x3 and 2x2 on the CYD)
      for (int flowerIndex = 0; flowerIndex < bushLayout.flowerCount; flowerIndex++) {
        const int flowerSize = (flowerIndex % 2 == 0) ? largeFlowerSize : smallFlowerSize;
        const int flowerX = bushCenterX + mirror * (flowerOffsets[flowerIndex][0] * bushRadius / 10) - flowerSize / 2;
        const int flowerY = bushBaseY + (flowerOffsets[flowerIndex][1] * bushRadius / 10) - flowerSize / 2;
        gfx->fillRect(flowerX, flowerY, flowerSize, flowerSize,
                      flowerColors[(flowerIndex + bushLayout.colorShift) % 3]);
      }
      //#endregion
    }
    //#endregion

    //#region Grass tufts in front (bottom-aligned, shorter side blades)
    const int tuftBaseY = HORIZON + SY(24);
    const int sideBladeHeight = SY(12);
    const int centerBladeHeight = SY(18);
    const int bladeSpacing = SX(7);
    for (int tuftX466 : { 96, 160, 306, 452 }) {
      const int tuftX = SX(tuftX466);
      for (int blade = -1; blade <= 1; blade++) {
        const int bladeHeight = (blade == 0) ? centerBladeHeight : sideBladeHeight;
        gfx->fillRect(tuftX + blade * bladeSpacing, tuftBaseY - bladeHeight, 2, bladeHeight, dk);
      }
    }
    //#endregion
  } else if (biome == 1) {  // beach: sea rolling toward the shore, wash edge, palm tree
    //#region Beach colors (night uses the night sea colors; the palm blends toward night blue)
    const uint16_t nightBlue = C565(0x0c, 0x12, 0x24);
    const uint16_t seaColor = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    const uint16_t foamColor = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
    const uint16_t farSeaColor = lerp565(seaColor, C565(0x10, 0x18, 0x20), 4, 16);
    const uint16_t shallowColor = lerp565(seaColor, foamColor, 6, 16);
    const uint16_t wetSandColor = lerp565(soil, dk, 6, 16);
    uint16_t trunkColor = C565(0xa8, 0x7a, 0x4a);
    uint16_t trunkRingColor = C565(0x7a, 0x54, 0x30);
    uint16_t frondDarkColor = C565(0x2e, 0x7d, 0x3a);
    uint16_t frondLightColor = C565(0x5c, 0xb0, 0x4c);
    uint16_t coconutColor = C565(0x6b, 0x45, 0x22);
    if (night) {
      trunkColor = lerp565(trunkColor, nightBlue, 10, 16);
      trunkRingColor = lerp565(trunkRingColor, nightBlue, 10, 16);
      frondDarkColor = lerp565(frondDarkColor, nightBlue, 10, 16);
      frondLightColor = lerp565(frondLightColor, nightBlue, 10, 16);
      coconutColor = lerp565(coconutColor, nightBlue, 10, 16);
    }
    //#endregion

    //#region Shoreline geometry (CYD: sea 122-137 deep, 138-143 shallow, wash edge 144-151, wet sand to 152)
    const int seaTopY = HORIZON - SY(52);
    const int shallowTopY = HORIZON - SY(20);
    const int waterRestY = HORIZON - SY(8);
    const int washRange = SY(14);
    const int wetSandHeight = SY(18);
    const int foamRimHeight = SY(4);
    const int tongueRadius = SY(10);
    //#endregion

    //#region Sea body, far-sea band, wet sand and shallow strip
    gfx->fillRect(0, seaTopY, LCD_WIDTH, shallowTopY - seaTopY, seaColor);
    gfx->fillRect(0, seaTopY, LCD_WIDTH, SY(4), farSeaColor);
    gfx->fillRect(0, waterRestY, LCD_WIDTH, wetSandHeight, wetSandColor);
    gfx->fillRect(0, shallowTopY, LCD_WIDTH, waterRestY - shallowTopY, shallowColor);
    //#endregion

    //#region Wash tongues: five columns whose foam edge washes up and pulls back
    const uint32_t washPeriodMs = 2000, washRiseMs = 700, columnLagMs = 130;
    static const int8_t columnDepthTwelfths[5] = { 10, 13, 9, 12, 11 };
    const int columnCount = 5;
    const int columnWidth = (LCD_WIDTH + columnCount - 1) / columnCount;
    for (int columnIndex = 0; columnIndex < columnCount; columnIndex++) {
      //#region Smoothstep wash amount 0..256 for this column
      const uint32_t washPhaseMs = (now + columnIndex * columnLagMs) % washPeriodMs;
      const int32_t rampAmount = washPhaseMs < washRiseMs
        ? (int32_t)(washPhaseMs * 256 / washRiseMs)
        : (int32_t)((washPeriodMs - washPhaseMs) * 256 / (washPeriodMs - washRiseMs));
      const int32_t washAmount = rampAmount * rampAmount * (768 - 2 * rampAmount) / 65536;
      //#endregion

      //#region Foam rim, then shallow water inset above it
      const int edgeY = waterRestY + (int)(washRange * columnDepthTwelfths[columnIndex] * washAmount / (12 * 256));
      const int columnX = columnIndex * columnWidth;
      const int tongueHeight = edgeY - shallowTopY;
      gfx->fillRoundRect(columnX, shallowTopY, columnWidth, tongueHeight + foamRimHeight, tongueRadius, foamColor);
      gfx->fillRoundRect(columnX, shallowTopY, columnWidth, tongueHeight, tongueRadius, shallowColor);
      //#endregion
    }
    //#endregion

    //#region Wave crests travelling toward the shore (accelerating, widening, brightening)
    const uint32_t crestPeriodMs = 6000;
    const int crestCount = 3;
    const int crestTopY = seaTopY + SY(6);
    const int crestSpan = waterRestY - SY(2) - crestTopY;
    static const int16_t crestCenters466[3][2] = { { 90, 300 }, { 200, 410 }, { 40, 250 } };
    for (int crestIndex = 0; crestIndex < crestCount; crestIndex++) {
      const uint32_t crestPhaseMs = (now + crestIndex * (crestPeriodMs / crestCount)) % crestPeriodMs;
      const int32_t crestProgress = (int32_t)(crestPhaseMs * 256 / crestPeriodMs);
      const int crestY = crestTopY + (int)(crestSpan * crestProgress * crestProgress / 65536);
      const int crestHalfWidth = SX(14) + (int)(SX(40) * crestProgress / 256);
      const int crestThickness = crestProgress > 128 ? SY(4) : SY(2);
      const uint16_t crestColor = lerp565(seaColor, foamColor, 3 + (int)(crestProgress * 11 / 256), 16);
      for (int segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
        gfx->fillRect(SX(crestCenters466[crestIndex][segmentIndex]) - crestHalfWidth, crestY,
                      2 * crestHalfWidth, crestThickness, crestColor);
      }
    }
    //#endregion

    //#region Palm trunk: eight tapered segments leaning left, darker ring on each
    const int trunkBaseX = SX(124), trunkBaseY = HORIZON + SY(18);
    const int crownX = SX(100), crownY = SY(128);
    const int trunkSegmentCount = 8;
    const int segmentHeight = (trunkBaseY - crownY) / trunkSegmentCount;
    const int trunkLean = trunkBaseX - crownX;
    for (int segmentIndex = 0; segmentIndex < trunkSegmentCount; segmentIndex++) {
      const int segmentCenterX = trunkBaseX - trunkLean * segmentIndex * segmentIndex / (trunkSegmentCount * trunkSegmentCount);
      const int segmentWidth = SX(14) - (SX(14) - SX(9)) * segmentIndex / (trunkSegmentCount - 1);
      const int segmentTopY = trunkBaseY - (segmentIndex + 1) * segmentHeight;
      gfx->fillRect(segmentCenterX - segmentWidth / 2, segmentTopY, segmentWidth, segmentHeight + 1, trunkColor);
      gfx->fillRect(segmentCenterX - segmentWidth / 2, segmentTopY, segmentWidth, SY(3), trunkRingColor);
    }
    //#endregion

    //#region Palm crown: hub, six drooping fronds (two triangles each), coconuts
    gfx->fillCircle(crownX, crownY, SY(8), frondDarkColor);
    // { midDx, midDy, tipDx, tipDy, useLightGreen } offsets from the crown in 466-space
    static const int8_t fronds466[6][5] = {
      { -38, -14, -66, 16, 0 }, { 40, -14, 70, 14, 0 }, { -26, -22, -52, -12, 1 },
      { 28, -22, 54, -10, 1 }, { -22, 4, -34, 34, 1 }, { 22, 4, 38, 32, 0 },
    };
    const int frondThickness = SY(6);
    const int frondTipThickness = SY(4);
    for (const auto &frond : fronds466) {
      const uint16_t frondColor = frond[4] ? frondLightColor : frondDarkColor;
      const int midX = crownX + SX(frond[0]), midY = crownY + SY(frond[1]);
      const int tipX = crownX + SX(frond[2]), tipY = crownY + SY(frond[3]);
      gfx->fillTriangle(crownX, crownY - frondThickness, crownX, crownY + frondThickness, midX, midY, frondColor);
      gfx->fillTriangle(midX, midY - frondTipThickness, midX, midY + frondTipThickness, tipX, tipY, frondColor);
    }
    const int coconutRadius = SY(7);
    gfx->fillCircle(crownX - SX(8), crownY + SY(8), coconutRadius, coconutColor);
    gfx->fillCircle(crownX + SX(7), crownY + SY(10), coconutRadius, coconutColor);
    gfx->fillCircle(crownX - SX(1), crownY + SY(15), coconutRadius, coconutColor);
    //#endregion
  } else if (biome == 2) {  // forest: hazy back row, two leafy trees, darker front row of conifers
    //#region Forest colors
    uint16_t hazeTarget = night ? C565(0x3a, 0x48, 0x66) : C565(0xb8, 0xd4, 0xcc);
    uint16_t backRowColor = lerp565(dk, hazeTarget, night ? 5 : 6, 16);
    uint16_t frontRowColor = lerp565(dk, C565(0x10, 0x28, 0x18), 4, 16);
    uint16_t trunkColor = lerp565(dk, C565(0x3a, 0x26, 0x18), 9, 16);
    uint16_t canopyColor = lerp565(dk, C565(0x6a, 0x96, 0x3a), night ? 3 : 6, 16);
    //#endregion

    //#region Conifer tier shapes (per tier, bottom first: baseRise60, apexRise60, halfWidth16)
    static const uint8_t TWO_TIER_SHAPE[2][3] = { { 0, 46, 16 }, { 28, 60, 12 } };
    static const uint8_t THREE_TIER_SHAPE[3][3] = { { 0, 34, 16 }, { 22, 50, 12 }, { 40, 60, 8 } };
    //#endregion

    //#region Forest tree tables (466-space)
    // conifers: centerX, height, baseHalfWidth, tierCount, baseOffsetY, isFrontRow
    static const int16_t FOREST_CONIFERS[10][6] = {
      { 40, 95, 22, 2, -2, 0 }, { 112, 130, 26, 3, -1, 0 }, { 200, 62, 18, 2, -2, 0 },   // back row (hazy)
      { 268, 74, 20, 2, -1, 0 }, { 300, 96, 22, 3, -2, 0 }, { 436, 90, 22, 2, -1, 0 },
      { 72, 140, 34, 3, 4, 1 }, { 150, 175, 36, 3, 6, 1 }, { 340, 160, 34, 3, 5, 1 },    // front row (dark)
      { 422, 118, 30, 2, 3, 1 },
    };
    const int BACK_ROW_CONIFER_COUNT = 6;
    // leafy trees: centerX, height, canopyRadius, baseOffsetY
    static const int16_t FOREST_DECIDUOUS[2][4] = { { 24, 78, 22, 5 }, { 384, 92, 24, 4 } };
    //#endregion

    //#region Trees: back row conifers, then leafy trees, then front row conifers
    for (int pass = 0; pass < 3; pass++) {
      //#region Pass 1: leafy trees (thin trunk, two-circle canopy)
      if (pass == 1) {
        for (int i = 0; i < 2; i++) {
          int treeX = SX(FOREST_DECIDUOUS[i][0]);
          int groundY = HORIZON + FOREST_DECIDUOUS[i][3];
          int treeTopY = groundY - SY(FOREST_DECIDUOUS[i][1]);
          int canopyRadius = SX(FOREST_DECIDUOUS[i][2]);
          int canopyCenterY = treeTopY + canopyRadius;
          gfx->fillRect(treeX - 1, canopyCenterY, 3, groundY - canopyCenterY, trunkColor);
          gfx->fillCircle(treeX, canopyCenterY, canopyRadius, canopyColor);
          gfx->fillCircle(treeX + canopyRadius * 6 / 10, canopyCenterY + canopyRadius * 5 / 10,
                          canopyRadius * 7 / 10, canopyColor);
        }
        continue;
      }
      //#endregion

      //#region Pass 0 and 2: conifers of the matching row
      int firstConifer = (pass == 0) ? 0 : BACK_ROW_CONIFER_COUNT;
      int lastConifer = (pass == 0) ? BACK_ROW_CONIFER_COUNT : 10;
      for (int i = firstConifer; i < lastConifer; i++) {
        int treeX = SX(FOREST_CONIFERS[i][0]);
        int treeHeight = SY(FOREST_CONIFERS[i][1]);
        int baseHalfWidth = SX(FOREST_CONIFERS[i][2]);
        int tierCount = FOREST_CONIFERS[i][3];
        int groundY = HORIZON + FOREST_CONIFERS[i][4];
        bool isFrontRow = FOREST_CONIFERS[i][5] != 0;
        uint16_t bodyColor = isFrontRow ? frontRowColor : backRowColor;

        //#region Trunk (front row only): tiers start a few pixels above the ground
        int trunkHeight = isFrontRow ? SY(8) : 0;
        if (isFrontRow) gfx->fillRect(treeX - 1, groundY - trunkHeight - 1, 3, trunkHeight + 1, trunkColor);
        int tierGroundY = groundY - trunkHeight;
        int tierSpanHeight = treeHeight - trunkHeight;
        //#endregion

        //#region Tiers, bottom first
        const uint8_t (*tierShape)[3] = (tierCount == 3) ? THREE_TIER_SHAPE : TWO_TIER_SHAPE;
        for (int j = 0; j < tierCount; j++) {
          int tierBaseY = tierGroundY - tierSpanHeight * tierShape[j][0] / 60;
          int tierApexY = tierGroundY - tierSpanHeight * tierShape[j][1] / 60;
          int tierHalfWidth = baseHalfWidth * tierShape[j][2] / 16;
          gfx->fillTriangle(treeX, tierApexY, treeX - tierHalfWidth, tierBaseY, treeX + tierHalfWidth, tierBaseY, bodyColor);
        }
        //#endregion
      }
      //#endregion
    }
    //#endregion
  } else if (biome == 3) {  // volcano: flat-topped cone, lava down the left slope, rocks and embers
    //#region Cone geometry (466-space, scaled with SX/SY)
    const int craterY = HORIZON - SY(120);
    const int craterLeftX = SX(82), craterRightX = SX(118);
    const int baseLeftX = SX(10), baseRightX = SX(180);
    const int litFaceTopX = SX(108), litFaceBaseX = SX(126);
    //#endregion

    //#region Volcano colors (the cone darkens at night, lava and glow stay bright)
    const uint16_t nightBlue = C565(0x16, 0x1c, 0x30);
    uint16_t coneShadowColor = C565(0x5e, 0x44, 0x3e);
    uint16_t coneLitColor = C565(0x7a, 0x58, 0x4c);
    uint16_t smokeColor = C565(0xa8, 0xa0, 0x9c);
    if (night) {
      coneShadowColor = lerp565(coneShadowColor, nightBlue, 9, 16);
      coneLitColor = lerp565(coneLitColor, nightBlue, 9, 16);
      smokeColor = C565(0x4a, 0x3c, 0x4c);
    }
    const uint16_t lavaColor = C565(0xff, 0x5a, 0x1e);
    const uint16_t lavaCoreColor = C565(0xff, 0xb0, 0x30);
    const uint16_t dripColor = C565(0xff, 0xe8, 0x80);
    static const uint8_t flickerSteps[4] = { 0, 1, 3, 2 };
    const uint16_t glowColor = lerp565(C565(0xff, 0xa8, 0x40), C565(0xff, 0xf0, 0xa0), flickerSteps[(now / 150) % 4], 3);
    //#endregion

    //#region Smoke puffs rising from the crater (drawn first so the glow and cone sit in front)
    for (int puffIndex = 0; puffIndex < 2; puffIndex++) {
      int puffRise = (int)((now / 80 + puffIndex * SY(24)) % SY(48));
      int puffX = SX(100) + puffIndex * SX(6) + puffRise / 3;
      int puffY = craterY - SY(12) - puffRise;
      gfx->fillCircle(puffX, puffY, SY(6) + puffRise / 6, smokeColor);
    }
    //#endregion

    //#region Crater glow halo (the cone covers its lower half)
    gfx->fillRoundRect(craterLeftX, craterY - SY(10), craterRightX - craterLeftX, SY(14), SY(6), glowColor);
    //#endregion

    //#region Cone: flat-topped trapezoid as two triangles, plus a lit right face
    gfx->fillTriangle(craterLeftX, craterY, baseLeftX, HORIZON, baseRightX, HORIZON, coneShadowColor);
    gfx->fillTriangle(craterLeftX, craterY, craterRightX, craterY, baseRightX, HORIZON, coneShadowColor);
    gfx->fillTriangle(litFaceTopX, craterY, craterRightX, craterY, baseRightX, HORIZON, coneLitColor);
    gfx->fillTriangle(litFaceTopX, craterY, baseRightX, HORIZON, litFaceBaseX, HORIZON, coneLitColor);
    gfx->drawFastHLine(craterLeftX, craterY, craterRightX - craterLeftX + 1, glowColor);
    //#endregion

    //#region Lava stream down the left slope (parallelogram segments, tapering tip)
    const int lavaPointX[4] = { (int)SX(84), (int)SX(70), (int)SX(54), (int)SX(36) };
    const int lavaPointY[4] = { craterY, (int)(HORIZON - SY(84)), (int)(HORIZON - SY(52)), (int)(HORIZON - SY(20)) };
    const int lavaWidth[4] = { (int)SX(10), (int)SX(8), (int)SX(7), 0 };
    for (int segmentIndex = 0; segmentIndex < 3; segmentIndex++) {
      int topX = lavaPointX[segmentIndex], topY = lavaPointY[segmentIndex], topWidth = lavaWidth[segmentIndex];
      int bottomX = lavaPointX[segmentIndex + 1], bottomY = lavaPointY[segmentIndex + 1], bottomWidth = lavaWidth[segmentIndex + 1];
      gfx->fillTriangle(topX, topY, topX + topWidth, topY, bottomX + bottomWidth, bottomY, lavaColor);
      if (bottomWidth > 0) gfx->fillTriangle(topX, topY, bottomX, bottomY, bottomX + bottomWidth, bottomY, lavaColor);
    }
    //#endregion

    //#region Bright lava core on the upper two segments
    const int coreTopX = SX(87), coreMidX = SX(72), coreTipX = SX(56), coreWidth = SX(4);
    gfx->fillTriangle(coreTopX, lavaPointY[0], coreTopX + coreWidth, lavaPointY[0], coreMidX + coreWidth, lavaPointY[1], lavaCoreColor);
    gfx->fillTriangle(coreTopX, lavaPointY[0], coreMidX, lavaPointY[1], coreMidX + coreWidth, lavaPointY[1], lavaCoreColor);
    gfx->fillTriangle(coreMidX, lavaPointY[1], coreMidX + coreWidth, lavaPointY[1], coreTipX, lavaPointY[2], lavaCoreColor);
    //#endregion

    //#region Animated drip sliding down the stream centerline
    int dripPhase = (int)((now / 25) % 90);
    int dripSegment = dripPhase / 30, dripStep = dripPhase % 30;
    int dripStartX = lavaPointX[dripSegment] + lavaWidth[dripSegment] / 2;
    int dripEndX = lavaPointX[dripSegment + 1] + lavaWidth[dripSegment + 1] / 2;
    int dripX = dripStartX + (dripEndX - dripStartX) * dripStep / 30;
    int dripY = lavaPointY[dripSegment] + (lavaPointY[dripSegment + 1] - lavaPointY[dripSegment]) * dripStep / 30;
    gfx->fillRect(dripX - 1, dripY - 1, 3, 3, dripColor);
    //#endregion

    //#region Ground rocks and embers (embers glow day and night)
    gfx->fillTriangle(SX(70), HORIZON, SX(40), HORIZON + 30, SX(100), HORIZON + 30, dk);
    gfx->fillTriangle(SX(400), HORIZON + 4, SX(372), HORIZON + 30, SX(430), HORIZON + 30, dk);
    for (int ember = 0; ember < 4; ember++)
      gfx->fillRect(SX(120) + ember * SX(70), HORIZON + 8 + (ember % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
    //#endregion
  } else if (biome == 4) {  // mountain: hazy back range, darker front range, snow caps, small conifers
    //#region Mountain colors
    uint16_t hazeTarget = night ? C565(0x5a, 0x66, 0x8a) : C565(0xc8, 0xd8, 0xe8);
    uint16_t backRowColor = lerp565(dk, hazeTarget, night ? 5 : 6, 16);
    uint16_t frontRowColor = dk;
    uint16_t frontSnowColor = night ? lerp565(C565(0xf4, 0xf8, 0xfc), C565(0x1e, 0x26, 0x46), 9, 16)
                                    : C565(0xf4, 0xf8, 0xfc);
    uint16_t backSnowColor = lerp565(frontSnowColor, backRowColor, 5, 16);
    uint16_t treeColor = lerp565(dk, C565(0x10, 0x30, 0x18), 8, 16);
    //#endregion

    //#region Peak table (466-space: apexX, peakHeight, baseHalfWidth)
    // the front-left peak stays at 155 so on the round board it clears the header text
    static const int16_t MOUNTAIN_PEAKS[6][3] = {
      { 60, 130, 100 }, { 200, 110, 100 }, { 300, 135, 110 }, { 415, 140, 100 },  // back row (hazy)
      { 110, 155, 120 }, { 395, 150, 105 },                                        // front row (dark)
    };
    const int BACK_ROW_PEAK_COUNT = 4;
    //#endregion

    //#region Peaks with snow caps (back row first, then front row)
    for (int i = 0; i < 6; i++) {
      bool isBackRow = i < BACK_ROW_PEAK_COUNT;
      uint16_t bodyColor = isBackRow ? backRowColor : frontRowColor;
      uint16_t snowColor = isBackRow ? backSnowColor : frontSnowColor;

      //#region Peak body
      int apexX = SX(MOUNTAIN_PEAKS[i][0]);
      int peakHeight = SY(MOUNTAIN_PEAKS[i][1]);
      int baseHalfWidth = SX(MOUNTAIN_PEAKS[i][2]);
      int apexY = HORIZON - peakHeight;
      gfx->fillTriangle(apexX, apexY, apexX - baseHalfWidth, HORIZON, apexX + baseHalfWidth, HORIZON, bodyColor);
      //#endregion

      //#region Snow cap on the same slopes (top 30 percent of the peak)
      int capDepth = peakHeight * 3 / 10;
      int capHalfWidth = baseHalfWidth * capDepth / peakHeight;
      int capBottomY = apexY + capDepth;
      gfx->fillTriangle(apexX, apexY, apexX - capHalfWidth, capBottomY, apexX + capHalfWidth, capBottomY, snowColor);
      //#endregion

      //#region Notch in the cap's lower edge so the snow line is jagged
      int notchHalfWidth = capHalfWidth / 2;
      int notchTipY = capBottomY - capDepth / 3;
      gfx->fillTriangle(apexX, notchTipY, apexX - notchHalfWidth, capBottomY, apexX + notchHalfWidth, capBottomY, bodyColor);
      //#endregion
    }
    //#endregion

    //#region Small conifers at the foot of the range (x466, baseOffsetY, isLarge)
    static const int16_t MOUNTAIN_CONIFERS[6][3] = {
      { 18, 2, 1 }, { 48, -2, 0 }, { 80, 3, 1 }, { 350, 1, 0 }, { 388, -3, 1 }, { 446, 2, 0 },
    };
    for (int i = 0; i < 6; i++) {
      int treeX = SX(MOUNTAIN_CONIFERS[i][0]);
      int treeBaseY = HORIZON + MOUNTAIN_CONIFERS[i][1];
      int treeHeight = MOUNTAIN_CONIFERS[i][2] ? 30 : 22;
      int treeHalfWidth = MOUNTAIN_CONIFERS[i][2] ? 8 : 6;
      //#region Lower tier (forest-tree proportions: apex at 46/60, upper tier base at 28/60)
      gfx->fillTriangle(treeX, treeBaseY - treeHeight * 46 / 60, treeX - treeHalfWidth, treeBaseY,
                        treeX + treeHalfWidth, treeBaseY, treeColor);
      //#endregion
      //#region Upper tier
      int upperTierBaseY = treeBaseY - treeHeight * 28 / 60;
      int upperHalfWidth = treeHalfWidth * 12 / 16;
      gfx->fillTriangle(treeX, treeBaseY - treeHeight, treeX - upperHalfWidth, upperTierBaseY,
                        treeX + upperHalfWidth, upperTierBaseY, treeColor);
      //#endregion
    }
    //#endregion
  } else if (biome == 5) {  // snow: background mounds, frozen lake, falling flakes (day only)
    //#region Snow colors (day palette, blended toward night blue after dark)
    const uint16_t nightBlue = C565(0x0c, 0x12, 0x24);
    uint16_t backMoundLitColor = C565(0xdc, 0xe6, 0xf0);
    uint16_t backMoundShadeColor = C565(0xb0, 0xbe, 0xd2);
    uint16_t frontMoundLitColor = C565(0xf4, 0xf8, 0xfc);
    uint16_t frontMoundShadeColor = C565(0xbc, 0xc8, 0xda);
    uint16_t lakeRimColor = C565(0x7c, 0xa2, 0xc0);
    uint16_t lakeIceColor = C565(0xb0, 0xd2, 0xe8);
    uint16_t lakeReflectionColor = C565(0xd0, 0xe6, 0xf4);
    uint16_t lakeGlintColor = UI_WHITE;
    if (night) {
      backMoundLitColor = lerp565(backMoundLitColor, nightBlue, 9, 16);
      backMoundShadeColor = lerp565(backMoundShadeColor, nightBlue, 9, 16);
      frontMoundLitColor = lerp565(frontMoundLitColor, nightBlue, 9, 16);
      frontMoundShadeColor = lerp565(frontMoundShadeColor, nightBlue, 9, 16);
      lakeRimColor = lerp565(lakeRimColor, nightBlue, 7, 16);
      lakeIceColor = lerp565(lakeIceColor, nightBlue, 7, 16);
      lakeReflectionColor = lerp565(lakeReflectionColor, nightBlue, 7, 16);
      lakeGlintColor = lerp565(lakeGlintColor, nightBlue, 8, 16);
    }
    const uint16_t driftCrestColor = lerp565(hill, frontMoundShadeColor, 8, 16);
    const uint16_t lakeCrackColor = lerp565(lakeRimColor, lakeIceColor, 6, 16);
    //#endregion

    //#region Mound table (466-space: leftX, width, rise; back row first, then front row)
    static const int16_t SNOW_MOUNDS[4][3] = {
      { -20, 150, 34 }, { 250, 170, 40 },  // back row (hazier)
      { 95, 110, 22 }, { 380, 110, 26 },   // front row (whiter)
    };
    const int BACK_ROW_MOUND_COUNT = 2;
    const int shadeWidth = SX(10);
    //#endregion

    //#region Background snow mounds (shade on the left, the sun is on the right)
    for (int i = 0; i < 4; i++) {
      bool isBackRow = i < BACK_ROW_MOUND_COUNT;
      int moundLeft = SX(SNOW_MOUNDS[i][0]);
      int moundWidth = SX(SNOW_MOUNDS[i][1]);
      int moundRadius = SY(SNOW_MOUNDS[i][2]);
      int moundTop = HORIZON - 14 - moundRadius;
      gfx->fillRoundRect(moundLeft, moundTop, moundWidth, 2 * moundRadius, moundRadius,
                         isBackRow ? backMoundShadeColor : frontMoundShadeColor);
      gfx->fillRoundRect(moundLeft + shadeWidth, moundTop, moundWidth - shadeWidth, 2 * moundRadius, moundRadius,
                         isBackRow ? backMoundLitColor : frontMoundLitColor);
    }
    //#endregion

    //#region Repaint the front hill over the mound bases so the mounds read as behind it
    // the hill is flat across the visible width from HORIZON-14 down, so this is pixel-identical
    gfx->fillRect(0, HORIZON - 14, LCD_WIDTH, SY(40) + 2, hill);
    gfx->drawFastHLine(0, HORIZON - 14, LCD_WIDTH, driftCrestColor);
    //#endregion

    //#region Frozen lake on the right (rim, ice, paler far half)
    const int lakeLeft = SX(318);
    const int lakeWidth = SX(124);
    const int lakeTop = HORIZON + SY(4);
    const int lakeHeight = SY(18);
    const int lakeInset = SX(4);
    const int iceHeight = lakeHeight - 2;
    gfx->fillRoundRect(lakeLeft, lakeTop, lakeWidth, lakeHeight, lakeHeight / 2, lakeRimColor);
    gfx->fillRoundRect(lakeLeft + lakeInset, lakeTop + 1, lakeWidth - 2 * lakeInset, iceHeight, iceHeight / 2,
                       lakeIceColor);
    gfx->fillRoundRect(lakeLeft + 3 * lakeInset, lakeTop + 1, lakeWidth - 6 * lakeInset, iceHeight / 2,
                       iceHeight / 4, lakeReflectionColor);
    //#endregion

    //#region Lake glints (white) and a stepped crack (darker ice)
    gfx->drawFastHLine(lakeLeft + SX(26), lakeTop + 2, SX(16), lakeGlintColor);
    gfx->drawFastHLine(lakeLeft + SX(80), lakeTop + lakeHeight - 3, SX(10), lakeGlintColor);
    gfx->drawFastHLine(lakeLeft + SX(50), lakeTop + SY(6), SX(8), lakeCrackColor);
    gfx->drawFastHLine(lakeLeft + SX(56), lakeTop + SY(9), SX(8), lakeCrackColor);
    gfx->drawFastHLine(lakeLeft + SX(62), lakeTop + SY(12), SX(8), lakeCrackColor);
    //#endregion

    //#region Falling snowflakes (day only)
    if (!night) {
      for (int flake = 0; flake < 10; flake++) {
        int flakeX = (flake * 53 + now / 40) % LCD_WIDTH;
        int flakeY = (flake * 90 + now / 18) % HORIZON;
        gfx->fillRect(flakeX, flakeY, 3, 3, UI_WHITE);
      }
    }
    //#endregion
  }
}

// first game: pick a starter among Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  ctext(T(S_CHOOSE_STARTER), 2, STARTER_TITLE_Y);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(STARTER_ROW_X, ry, STARTER_ROW_W, STARTER_ROW_H, STARTER_R, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(STARTER_ROW_X, ry, STARTER_ROW_W, STARTER_ROW_H, STARTER_R, de.accent);
    const uint8_t *th = thumbs.get(d);     // starter thumbnail (if the SD is ready)
    if (th) drawThumbCell(th, STARTER_TH_X, ry + STARTER_TH_DY, STARTER_TH_S, false, STARTER_TH_CELL);
    gfx->setTextColor(UI_INK);
    int nx = STARTER_ROW_X + STARTER_NAME_DX;
    int fs = fitSize(dexName(d), TS(3), STARTER_ROW_X + STARTER_ROW_W - nx - 6);
    setSize(fs);
    setCur(nx, ry + STARTER_NAME_DY);
    printT(dexName(d));
  }
  gfx->flush();
}

// ---------- text: size and cursor (ready for CJK fonts) ----------
// The classic font anchors the cursor at the TOP CORNER of the text, and the 74
// Y coordinates in the code are written that way. U8g2 fonts anchor it at the
// BASELINE (drawChar does curY = y - baseline*size), so switching fonts would
// shift all the text up. setCur() compensates for that difference in a single
// place, so Y keeps meaning "top" with any font.
//
// setSize() exists only because the library keeps textsize_x protected and
// setCur() needs to know the active scale to multiply the ascent.
//
// With the classic font gFontAscent is 0, so setCur(x,y) is exactly
// setCur(x,y): this does not move a pixel in the six current languages.
uint8_t gTextSize = 1;
int gFontAscent = 0;  // px from the top edge to the baseline, 0 = classic font

// --- CJK font tuning (the two values go together) ---
// The classic font is 8 px tall at scale 1. If the CJK one is taller, the scale
// has to be divided so the sizes match... but dividing THINS the stroke: at
// scale 1 unifont draws 1 px lines while Latin at scale 2 draws them 2 px wide,
// and Japanese looks faint even though it is the same size.
//
// That is why a small AND BOLD (_b_) base font is preferable, allowing the SAME
// scale as Latin: that way the stroke weight matches and no division is needed.
//
// A truly bold font (b10_b) was tried and does NOT work at this size: the
// strokes merge and the kana lose detail, illegible even to a native reader.
// It goes back to unifont, which is the most legible, and the thin stroke is
// compensated in printT() by redrawing 1 px offset (pseudo-bold).
// japanese3 and not japanese1: subsets 1 and 2 include kana and kanji but NO
// CJK punctuation, and Arduino_GFX draws nothing when a glyph is missing (nor
// advances the cursor), so the 25 ！ and ？ marks in the strings disappeared.
// It is the same unifont family: identical metrics header and a strict
// superset, checked glyph by glyph over the 226 codepoints the firmware
// uses. It costs 102 KB more flash and does not move a pixel.
// Korean needs a font with hangul, and CJK_FONT was a single #define.
// Instead of assuming one subset works for both scripts, the font is chosen
// per language. korean2 and not korean1: the Korean here uses 296 distinct
// syllables and all of them are in KS X 1001, the 2350-syllable set that is
// the measured size for korean2; korean1 has 478. The check looks at
// membership in KS X 1001, it does not walk the font's table, so it relies on
// korean2's set being exactly that; ASCII is assumed present.
// Not verified on the board: whether CJK_SIZE_DIV 2 suits korean2 as well as
// the Japanese one. The ascent, however, is measured at runtime in applyLangFont().
#define CJK_FONT_JA u8g2_font_unifont_t_japanese3
#define CJK_FONT_KO u8g2_font_unifont_t_korean2
#define CJK_SIZE_DIV 2

void setSize(uint8_t n) {
  // gTextSize stores the scale ACTUALLY applied, not the requested one: setCur()
  // multiplies the ascent by it and it has to match what is drawn.
  gTextSize = gCjkFont ? (n >= CJK_SIZE_DIV ? n / CJK_SIZE_DIV : 1) : n;
  gfx->setTextSize(gTextSize);
}

void setCur(int x, int y) {
  gfx->setCursor(x, y + gFontAscent * gTextSize);
}

// Sets the font for the active language. It is the ONLY place that touches it:
// the rest of the code draws the same for every language thanks to textW()/setCur().
//
// The ascent is measured instead of hard-coded: getTextBounds() returns y1 as
// the offset of the top edge relative to the cursor, negative with U8g2 fonts
// (which anchor at the baseline). That way setCur() can keep treating Y as
// "top" whatever the font, with no per-font magic constants.
void applyLangFont() {
  gCjkFont = LANG_IS_CJK(gLang);
  if (!gCjkFont) {
    gfx->setFont();            // classic 5x7, CP437: Y is already the top edge
    gfx->setUTF8Print(false);
    gFontAscent = 0;
    return;
  }
  gfx->setFont((gLang == LANG_KO) ? CJK_FONT_KO : CJK_FONT_JA);
  gfx->setUTF8Print(true);     // CJK strings are multibyte UTF-8
  int16_t x1, y1;
  uint16_t w, h;
  uint8_t antes = gTextSize;
  gfx->setTextSize(1);
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  gFontAscent = -y1;           // negative y1: go up from the baseline
  gfx->setTextSize(antes);
}

// ---------- text measurement (ready for CJK fonts) ----------
// Arduino_GFX's classic font advances 6 px per character at size 1, and the UI
// centered with a bare strlen(s)*6*n. That only works for one byte per
// character and fixed width: with a U8g2 font for Japanese/Korean/Chinese,
// strlen() does not count characters (UTF-8 is multibyte) and they are not all
// the same width. Centralizing it here is what will allow adding those
// languages without touching every drawing site.
//
// For the classic font it returns EXACTLY the same count as before, so this
// change does not move a pixel in the six current languages.
uint16_t textW(const char *s, uint8_t size) {
  if (!gCjkFont) return (uint16_t)strlen(s) * 6 * size;
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// cursor x to leave the text centered on CX
int centerX(const char *s, uint8_t size) { return CX - textW(s, size) / 2; }

// Size that fits in maxW. On the round board it changes nothing (returns n). On
// the rectangular one it steps the size down until it fits; it measures with the
// size ALREADY set (textW with a CJK font ignores the argument and uses the active font).
int fitSize(const char *s, int n, int maxW) {
#if BOARD_IS_ROUND
  (void)s; (void)maxW;
  return n;
#else
  if (maxW <= 0) maxW = LCD_WIDTH - 8;
  for (; n > 1; n--) {
    setSize(n);
    if (textW(s, n) <= maxW) break;
  }
  return n;
#endif
}

// Text centered on CX: TS() reduces the size on the rectangular board and
// fitSize() lowers it further if it does not fit. On the round one it is
// equivalent to setSize + setCur(centerX) + printT.
void ctext(const char *s, int n, int y, int maxW) {
  n = fitSize(s, TS(n), maxW);
  setSize(n);
  setCur(centerX(s, n), y);
  printT(s);
}

// Like ctext(), but centers the visible glyphs: the 5x7 font advances 6 px per
// character including a blank trailing column, so ctext() leaves Latin text
// half a column (size / 2 px) left of CX. CJK widths come from the real bounds.
void drawGlyphCenteredText(const char *text, int requestedSize, int y, int maxWidth) {
  int size = fitSize(text, TS(requestedSize), maxWidth);
  setSize(size);
  int trailingColumn = gCjkFont ? 0 : gTextSize;
  setCur(centerX(text, size) + trailingColumn / 2, y);
  printT(text);
}

// Prints honoring the active font. With CJK it redraws the text offset by
// 1 px: unifont at a reduced scale leaves 1 px strokes and looks faint, and a
// truly bold font at this size merges the strokes and ruins the kana.
// Redrawing thickens the stroke without distorting the glyph. Tested on the
// board by usakomint, who compared both options in Japanese.
// The nickname keyboard is alphabet-only (left that way on purpose), so there
// is never CJK here and nothing needs thickening.
void printT(char c) { gfx->print(c); }

void printT(const char *s) {
  if (gCjkFont) {
    int16_t x = gfx->getCursorX(), y = gfx->getCursorY();
    gfx->print(s);
    gfx->setCursor(x + 1, y);
  }
  gfx->print(s);
}

void render() {
  if (pet.awaitingStarter()) {  // first game: pick a starter (top priority)
    renderStarterSelect();
    return;
  }
  if (galleryOpen) {
    renderGallery();
    return;
  }
  if (gameOpen) {
    renderGame();
    return;
  }
  if (coinOpen) {
    renderCoinGame();
    return;
  }
  if (battleOpen) {
    renderBattle();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (kbOpen) {
    renderKeyboard();
    return;
  }
  if (settingsScreen != SETTINGS_CLOSED) {
    renderSettingsScreen();
    return;
  }
  if (cardOpen) {
    renderCard();
    return;
  }
  int h = sceneHour();
  // by the clock only: a sleeping creature keeps the current sky (the panel still dims, see updateBrightness)
  gNight = h < 6 || h >= 20;
  // drawScene covers the full 466x466: no prior fillScreen(BLACK), so an
  // overlapping DMA flush never captures half-drawn black (anti-flicker)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return;
  }

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    drawHamburgerButton();
    drawPokedexButton();
    drawHeaderClock();
    int s = 5, x = CX - 16 * s, y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      setSize(2);
      setCur(centerX(rar, 2), SY(316));
      printT(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    // bottom panel: lower third (y=312..466 of 466); SY() rescales it
    gfx->fillRect(0, SY(312), LCD_WIDTH, LCD_HEIGHT - SY(312), gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(reg, 2), SY(348));
    printT(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), "%s%s", pet.shiny ? "*" : "", base);  // the level has its own line now
    drawHeaderWithLevel(name, gNight ? UI_INK_NIGHT : d.accent);
    drawHamburgerButton();
    drawPokedexButton();
    drawHeaderClock();
    drawStreakBadge();
    drawPet();
    drawBath();
    drawPoops();
    // bottom panel: clean base for bars and buttons over the landscape
    gfx->fillRect(0, SY(312), LCD_WIDTH, LCD_HEIGHT - SY(312), gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    if (pet.wantEvolveButton()) drawEvolveButton();        // red CTA: evolve
    else if (pet.canRunawayNow()) drawRunawayButton();     // somber CTA: runaway (neglect)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // gold CTA: farewell
  }

  if (pet.sleeping) {
    gfx->setTextColor(inkColor());  // the sky can be daytime now: night ink would vanish on it
    setSize(TS(3));
    setCur(SX(320), SY(130));
    printT("Zz");
  }

  // food picker
  if (feedMenuUntil) {
    if (!timeLeft(feedMenuUntil)) {
      feedMenuUntil = 0;
    } else {
      gfx->fillRoundRect(FEED_X, FEED_Y, FEED_W, FEED_H, FEED_R, UI_WHITE);
      gfx->drawRoundRect(FEED_X, FEED_Y, FEED_W, FEED_H, FEED_R, inkColor());
      const int fix = FEED_X + (FEED_ITEM - 16 * FEED_ICON_S) / 2;
      const int fiy = FEED_Y + (FEED_H - 16 * FEED_ICON_S) / 2;
      drawMap(SPR_ICON_APPLE, 16, fix, fiy, FEED_ICON_S, false);    // apple
      drawMap(SPR_ICON_BERRY_B, 16, fix + FEED_ITEM, fiy, FEED_ICON_S, false);  // hamburger
      drawMap(SPR_ICON_BERRY_G, 16, fix + 2 * FEED_ITEM, fiy, FEED_ICON_S, false);  // pizza
      drawMap(SPR_ICON_CANDY, 16, fix + 3 * FEED_ITEM, fiy, FEED_ICON_S, false);
      drawMap(SPR_ICON_COFFEE, 16, fix + 4 * FEED_ITEM, fiy, FEED_ICON_S, false);
    }
  }

  #pragma region Play picker: ball (minigame), coin (coin chase) and crossed swords (wild battle)
  if (playMenuUntil) {
    if (!timeLeft(playMenuUntil)) {
      playMenuUntil = 0;
    } else {
      gfx->fillRoundRect(PLAY_MENU_X, PLAY_MENU_Y, PLAY_MENU_W, PLAY_MENU_H, PLAY_MENU_R, UI_WHITE);
      gfx->drawRoundRect(PLAY_MENU_X, PLAY_MENU_Y, PLAY_MENU_W, PLAY_MENU_H, PLAY_MENU_R, inkColor());
      const int playIconX = PLAY_MENU_X + (PLAY_MENU_ITEM - 16 * PLAY_MENU_ICON_S) / 2;
      const int playIconY = PLAY_MENU_Y + (PLAY_MENU_H - 16 * PLAY_MENU_ICON_S) / 2;
      drawMap(SPR_ICON_PLAY, 16, playIconX, playIconY, PLAY_MENU_ICON_S, false);
      drawMap(SPR_ICON_COIN, 16, playIconX + PLAY_MENU_ITEM, playIconY, PLAY_MENU_ICON_S, false);
      drawMap(SPR_ICON_SWORDS, 16, playIconX + 2 * PLAY_MENU_ITEM, playIconY, PLAY_MENU_ICON_S, false);
    }
  }
  #pragma endregion

  #pragma region Wild encounter prompt (closes on its own after WILD_PROMPT_MS)
  if (wildPromptUntil) {
    if (!timeLeft(wildPromptUntil)) wildPromptUntil = 0;
    else drawWildPrompt();
  }
  #pragma endregion

  // "release?" dialog (long press on the creature)
  if (confirmUntil) {
    if (!timeLeft(confirmUntil)) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(CONF_X, CONF_Y, CONF_W, CONF_H, CONF_R, UI_WHITE);
      gfx->drawRoundRect(CONF_X, CONF_Y, CONF_W, CONF_H, CONF_R, UI_INK);
      // 48 and not 28: the buffer was sized when every string was one byte
      // per character. With a UTF-8 row, "%s 놓아줄까요?" plus the name goes
      // over 28 and snprintf cuts in the middle of a 3-byte sequence, which
      // the font can no longer draw.
      char q[48];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
      gfx->setTextColor(UI_INK);
      ctext(q, 2, CONF_Y + CONF_QY, CONF_W - 16);
      const int cby = CONF_Y + CONF_BTN_REL_Y;
      const int yesX = CONF_X + CONF_BTN_MARGIN;
      const int noX = CONF_X + CONF_W - CONF_BTN_MARGIN - CONF_BTN_W;
      setSize(2);
      gfx->fillRoundRect(yesX, cby, CONF_BTN_W, CONF_BTN_H, 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      setCur(yesX + (CONF_BTN_W - textW(T(S_YES), 2)) / 2, cby + CONF_BTN_TY);
      printT(T(S_YES));
      gfx->fillRoundRect(noX, cby, CONF_BTN_W, CONF_BTN_H, 12, UI_BAR_BAD);
      setCur(noX + (CONF_BTN_W - textW(T(S_NO), 2)) / 2, cby + CONF_BTN_TY);
      printT(T(S_NO));
    }
  }

  // decision dialog (evolve/keep, say goodbye/stay together)
  if (choiceKind) {
    if (!timeLeft(choiceUntil)) choiceKind = 0;
    else drawChoiceDialog();
  }

  gfx->flush();
}

// ---------- minigame: keepy-uppy with the pokeball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameOpen = true;
  gameOver = false;
  gameAttackGain = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  hitTime = 0;
  ballLastHitAt = 0;
  gamePetX = CX;
  lastGameStep = millis();
  respawnBall();
}

void respawnBall() {
  ballX = SX(112) + random(SX(242));
  ballY = SY(82);
  float respawnSpeed = BALL_RESPAWN_SPEED_BASE + gameScore * BALL_RESPAWN_SPEED_PER_POINT;  // livelier as you progress
  if (respawnSpeed > BALL_RESPAWN_SPEED_MAX) respawnSpeed = BALL_RESPAWN_SPEED_MAX;
  ballVX = random(2) ? respawnSpeed : -respawnSpeed;
  ballVX += ((int)random(9) - 4) * BALL_RESPAWN_JITTER_STEP;
  ballVY = BALL_RESPAWN_VY;
}

// touch radius around the ball: shrinks as the score grows, floored for resistive touch
int ballHitRadius() {
  int hitRadius = gameScore >= 20 ? BALL_HIT_RADIUS_LATE : gameScore >= 8 ? BALL_HIT_RADIUS_MID : BALL_HIT_RADIUS_START;
  return hitRadius < BALL_HIT_RADIUS_MIN ? BALL_HIT_RADIUS_MIN : hitRadius;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOver) {
    if (isMinigameOkTapped(x, y)) {
      gameOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (isTouchInsideCircle(x, y, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R)) {  // X = quit with no reward
    gameOpen = false;
    return;
  }
  float dx = ballX - x, dy = ballY - y;
  int hitRadius = ballHitRadius();
  if (dx * dx + dy * dy < hitRadius * hitRadius) {  // the ball was hit!
    #pragma region Ignore hits during the cooldown or while the ball still rises
    uint32_t now = millis();
    if (now - ballLastHitAt < BALL_HIT_COOLDOWN_MS || ballVY < BALL_HIT_MIN_VY) return;
    #pragma endregion

    #pragma region Lift, drift away from the finger, chaos and a random reversal
    gameScore++;
    sfxPlay(SFX_PLAY);
    uint16_t liftScore = gameScore > BALL_LIFT_SCORE_CAP ? BALL_LIFT_SCORE_CAP : gameScore;
    float lift = BALL_LIFT_BASE + liftScore * BALL_LIFT_PER_POINT;
    ballVY = -lift;
    float drift = BALL_DRIFT_BASE + (gameScore >= BALL_DRIFT_STEP1_SCORE ? BALL_DRIFT_STEP1 : 0.0f) +
                  (gameScore >= BALL_DRIFT_STEP2_SCORE ? BALL_DRIFT_STEP2 : 0.0f);
    float chaos = ((int)random(17) - 8) * BALL_CHAOS_STEP;
    ballVX += dx * drift + chaos;
    if (random(100) < BALL_REVERSE_CHANCE_PCT) ballVX = -ballVX * (0.72f + random(45) * 0.01f);
    if (ballVX > BALL_MAX_VX) ballVX = BALL_MAX_VX;
    if (ballVX < -BALL_MAX_VX) ballVX = -BALL_MAX_VX;
    #pragma endregion

    hitX = ballX;
    hitY = ballY;
    hitTime = now;
    ballLastHitAt = now;
  }
}

// random sideways noise added on every wall bounce
float ballWallNoise() {
  return ((int)random(9) - 4) * BALL_WALL_NOISE_STEP;
}

void stepGame() {
  uint32_t now = millis();
  #pragma region Frame-time factor (normalised to the 115 ms frame of the fork)
  // without this the physics advanced "one step per frame" and the game ran
  // faster or slower depending on how long the previous frame took to draw,
  // which depends on the size of the loaded sprite
  float k = lastGameStep ? (now - lastGameStep) / BALL_FRAME_MS : 1.0f;
  if (k > BALL_FRAME_FACTOR_MAX) k = BALL_FRAME_FACTOR_MAX;  // abnormally late frame: do not take a huge jump
  lastGameStep = now;
  #pragma endregion

  #pragma region Gravity, wind sway and random kicks
  float gravity = BALL_GRAVITY_BASE + gameScore * BALL_GRAVITY_PER_POINT;  // falls faster each time
  if (gameScore >= BALL_GRAVITY_STEP1_SCORE) gravity += BALL_GRAVITY_STEP1;
  if (gameScore >= BALL_GRAVITY_STEP2_SCORE) gravity += BALL_GRAVITY_STEP2;
  if (gravity > BALL_GRAVITY_MAX) gravity = BALL_GRAVITY_MAX;
  ballVX += sinf((now + gameScore * BALL_WIND_SCORE_PHASE_MS) * BALL_WIND_RATE) * BALL_WIND_AMPLITUDE * k;
  if (random(1000) < (long)(BALL_KICK_CHANCE_PCT * 10.0f * k)) ballVX += ((int)random(7) - 3) * BALL_KICK_STEP;
  ballVY += gravity * k;
  ballX += ballVX * k;
  ballY += ballVY * k;
  #pragma endregion

#if BOARD_IS_ROUND
  #pragma region Bounce off the circular wall (physical round bezel)
  float dx = ballX - CX, dy = ballY - CY;
  float distance = sqrtf(dx * dx + dy * dy);
  if (distance > ARENA_WALL_R) {
    float normalX = dx / distance, normalY = dy / distance;
    float dot = ballVX * normalX + ballVY * normalY;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * normalX) * BALL_WALL_GAIN_X + ballWallNoise();
      ballVY = (ballVY - 2 * dot * normalY) * BALL_WALL_GAIN_Y;
      sfxPlay(SFX_BALL_BOUNCE);
    }
    ballX = CX + normalX * ARENA_WALL_R;
    ballY = CY + normalY * ARENA_WALL_R;
  }
  #pragma endregion
#else
  #pragma region Bounce off left/right (gain and noise) and the ceiling (0.88)
  if (ballX < ARENA_LEFT) {
    ballX = ARENA_LEFT;
    ballVX = fabsf(ballVX) * BALL_WALL_GAIN_X + ballWallNoise();
    sfxPlay(SFX_BALL_BOUNCE);
  }
  if (ballX > ARENA_RIGHT) {
    ballX = ARENA_RIGHT;
    ballVX = -fabsf(ballVX) * BALL_WALL_GAIN_X + ballWallNoise();
    sfxPlay(SFX_BALL_BOUNCE);
  }
  if (ballY < ARENA_TOP) {
    ballY = ARENA_TOP;
    ballVY = fabsf(ballVY) * BALL_WALL_GAIN_Y;
    sfxPlay(SFX_BALL_BOUNCE);
  }
  // the narrow arena bounces often: keep the 1.05 gain from building up past the hit cap
  if (ballVX > BALL_MAX_VX) ballVX = BALL_MAX_VX;
  if (ballVX < -BALL_MAX_VX) ballVX = -BALL_MAX_VX;
  #pragma endregion
#endif
  if (ballY > ARENA_GROUND_Y) {  // hit the ground
    if (++gameMisses >= 3) {
      gameNewHi = (gameScore > pet.gameHi);
      uint8_t attackBefore = pet.trAtk;
      pet.playResult(gameScore);  // trains ATK, updates the record and gives happiness
      gameAttackGain = pet.trAtk - attackBefore;
      sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOver = true;
    } else {
      respawnBall();
      sfxPlay(SFX_BALL_MISS);
    }
  }
  // the creature follows it from below
  float chase = (ballX - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase * k;
}

int scaledPixelEdge(int origin, int pixelIndex, uint16_t scalePercent);  // prototype (defined with the PMD drawing)
void drawPmdActResizedM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                        uint16_t sizePercent);  // prototype (defined with the PMD drawing)

// the creature in a minigame, anchored by the feet: PMD action (idle if missing)
// or the B/W SD sprite fallback; sizePercent resizes either one (100 = usual size)
void drawMinigamePet(uint8_t act, int centerX, int feetY, uint8_t maxScale, uint16_t sizePercent) {
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdActResizedM(pmd, act, centerX, feetY, millis(), true, false, maxScale, sizePercent);
  } else if (mon.loaded) {
    uint16_t scalePercent = ((mon.h * 2 > 130) ? 1 : 2) * sizePercent;
    int w = mon.w * scalePercent / 100, h = mon.h * scalePercent / 100;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = centerX - w / 2, py = feetY - h;
    int opaqueColumnMin = mon.w, opaqueColumnMax = -1, opaqueRowMin = mon.h, opaqueRowMax = -1;
    for (int r = 0; r < mon.h; r++) {
      int rowTop = scaledPixelEdge(py, r, scalePercent);
      int rowHeight = scaledPixelEdge(py, r + 1, scalePercent) - rowTop;
      if (rowHeight <= 0) continue;
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        int columnLeft = scaledPixelEdge(px, c, scalePercent);
        int columnWidth = scaledPixelEdge(px, c + 1, scalePercent) - columnLeft;
        if (columnWidth > 0) gfx->fillRect(columnLeft, rowTop, columnWidth, rowHeight, mon.pal[idx]);
        if (c < opaqueColumnMin) opaqueColumnMin = c;
        if (c > opaqueColumnMax) opaqueColumnMax = c;
        if (r < opaqueRowMin) opaqueRowMin = r;
        opaqueRowMax = r;
      }
    }
    #pragma region Remember the opaque pixel box on screen (coin chase pickup)
    lastSpriteDrawnBounds.valid = false;
    if (opaqueColumnMax >= 0) {
      lastSpriteDrawnBounds = { (int16_t)scaledPixelEdge(px, opaqueColumnMin, scalePercent),
                                (int16_t)scaledPixelEdge(py, opaqueRowMin, scalePercent),
                                (int16_t)(scaledPixelEdge(px, opaqueColumnMax + 1, scalePercent) - 1),
                                (int16_t)(scaledPixelEdge(py, opaqueRowMax + 1, scalePercent) - 1), true };
    }
    #pragma endregion
  }
}

void drawGameScene();  // prototype (defined further down)

#pragma region Minigame results screen (ball game and coin chase)
// Same look as the wild battle result: the creature on the habitat, a big
// colored title, two detail lines, a gold reward line and an OK button.
// Reuses the BATTLE_RESULT_* / BATTLE_OK_* layout so both screens stay aligned.
bool isMinigameOkTapped(int16_t x, int16_t y) {
  return isTouchInsideRect(x, y, BATTLE_OK_X, BATTLE_RESULT_BTN_Y - BATTLE_RESULT_HIT_PAD_Y, BATTLE_OK_W,
                           BATTLE_RESULT_BTN_H + 2 * BATTLE_RESULT_HIT_PAD_Y);
}

// one line centered on CX in the result column
void drawMinigameResultLine(const char *text, int preferredSize, int top) {
  drawTextCenteredInBox(text, preferredSize, CX - BATTLE_RESULT_TEXT_MAX_W / 2, top, BATTLE_RESULT_TEXT_MAX_W, 8 * preferredSize);
}

// title: the score (green for a new record or a cleared coin chase);
// lines: record (or NEW RECORD!), joy; reward: rewardFormat ("SPD +%u", "ATK +%u") when the stat went up
void drawMinigameResults(uint8_t score, bool newRecord, bool cleared, uint16_t record, uint8_t statGain, StrId rewardFormat) {
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t resultInk = night ? UI_INK_NIGHT : UI_INK;

  drawMinigamePet(PMD_IDLE, CX, BATTLE_GROUND_Y, BATTLE_SPRITE_MAX_S, 100);

  #pragma region Panel (rect board only, like the battle result)
  if (BATTLE_RESULT_PANEL_W > 0) {
    gfx->fillRoundRect(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                       BATTLE_RESULT_PANEL_R, UI_WHITE);
    gfx->drawRoundRect(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                       BATTLE_RESULT_PANEL_R, UI_INK);
    resultInk = UI_INK;
  }
  #pragma endregion

  #pragma region Title, record line, joy line, reward line
  char scoreText[22];
  snprintf(scoreText, sizeof(scoreText), T(S_SCORE_FMT), score);
  gfx->setTextColor(newRecord || cleared ? UI_BAR_OK : resultInk);
  drawMinigameResultLine(scoreText, BATTLE_RESULT_TITLE_SIZE, BATTLE_RESULT_TITLE_Y);

  if (newRecord) {
    gfx->setTextColor(UI_BAR_WARN);
    drawMinigameResultLine(T(S_NEW_RECORD), BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
  } else {
    char recordText[24];
    snprintf(recordText, sizeof(recordText), T(S_RECORD_FMT), record);
    gfx->setTextColor(resultInk);
    drawMinigameResultLine(recordText, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
  }
  gfx->setTextColor(resultInk);
  drawMinigameResultLine(score >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY), BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_DAMAGE_Y);

  if (statGain) {
    char rewardText[24];
    snprintf(rewardText, sizeof(rewardText), T(rewardFormat), statGain);
    gfx->setTextColor(UI_BAR_WARN);
    drawMinigameResultLine(rewardText, BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
  }
  #pragma endregion

  #pragma region OK button
  gfx->fillRoundRect(BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  drawTextCenteredInBox(T(S_OK), BATTLE_OK_TEXT_SIZE, BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H);
  #pragma endregion

  gfx->flush();
}
#pragma endregion

// ---------- minigame: coin chase (trains speed) ----------

#pragma region Coin chase: play area and walking speed
// keeps a body-center point inside the play area: the bezel circle on the round
// board (trimmed above for the timer and below for the score), a rectangle on the CYD
void clampToCoinPlayArea(float &bodyX, float &bodyY) {
#if BOARD_IS_ROUND
  float offsetX = bodyX - CX, offsetY = bodyY - CY;
  float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
  if (distance > COIN_PLAY_RADIUS) {
    bodyX = CX + offsetX * COIN_PLAY_RADIUS / distance;
    bodyY = CY + offsetY * COIN_PLAY_RADIUS / distance;
  }
#else
  if (bodyX < COIN_PLAY_LEFT) bodyX = COIN_PLAY_LEFT;
  if (bodyX > COIN_PLAY_RIGHT) bodyX = COIN_PLAY_RIGHT;
#endif
  if (bodyY < COIN_PLAY_TOP) bodyY = COIN_PLAY_TOP;
  if (bodyY > COIN_PLAY_BOTTOM) bodyY = COIN_PLAY_BOTTOM;
}

// px/s from the speed stat: higher SPE walks faster, clamped so nobody crawls or teleports
float coinPetSpeedPerSecond() {
  uint16_t speedStat = pet.speStat();
  if (speedStat > COIN_SPEED_STAT_CAP) speedStat = COIN_SPEED_STAT_CAP;
  return COIN_PET_SPEED_MIN + (COIN_PET_SPEED_MAX - COIN_PET_SPEED_MIN) * speedStat / COIN_SPEED_STAT_CAP;
}
#pragma endregion

void startCoinGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  coinOpen = true;
  coinOver = false;
  coinPoints = 0;
  coinSpeedGain = 0;
  coinNewHi = false;
  coinTimeLeftMs = COIN_START_TIME_MS;
  coinPetX = coinTargetX = CX;
  coinPetFeetY = coinTargetFeetY = COIN_PLAY_BOTTOM + COIN_PET_BODY_OFFSET;
  coinPetFacingLeft = false;
  lastSpriteDrawnBounds.valid = false;  // from another screen: no pickup until the creature is drawn here
  lastCoinStep = millis();
  respawnCoin();
}

// new coin at a random spot in the play area, not right next to the creature
void respawnCoin() {
  coinSpawnedAt = millis();
  float petBodyY = coinPetFeetY - COIN_PET_BODY_OFFSET;
  for (int attempt = 0; attempt < COIN_SPAWN_ATTEMPTS; attempt++) {
#if BOARD_IS_ROUND
    coinX = CX - COIN_PLAY_RADIUS + random(2 * COIN_PLAY_RADIUS + 1);
#else
    coinX = COIN_PLAY_LEFT + random(COIN_PLAY_RIGHT - COIN_PLAY_LEFT + 1);
#endif
    coinY = COIN_PLAY_TOP + random(COIN_PLAY_BOTTOM - COIN_PLAY_TOP + 1);
    clampToCoinPlayArea(coinX, coinY);
    float offsetX = coinX - coinPetX, offsetY = coinY - petBodyY;
    if (offsetX * offsetX + offsetY * offsetY >= (float)COIN_SPAWN_MIN_DISTANCE * COIN_SPAWN_MIN_DISTANCE) return;
  }
  // every attempt landed too close: the last one stays (the creature grabs it at once)
}

void coinGameTap(int16_t x, int16_t y) {
  if (coinOver) {
    if (isMinigameOkTapped(x, y)) {
      coinOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (isTouchInsideCircle(x, y, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R)) {  // X = quit with no reward
    coinOpen = false;
    return;
  }
  #pragma region The tapped spot becomes the body destination
  float targetBodyX = x, targetBodyY = y;
  clampToCoinPlayArea(targetBodyX, targetBodyY);
  coinTargetX = targetBodyX;
  coinTargetFeetY = targetBodyY + COIN_PET_BODY_OFFSET;
  #pragma endregion
}

// the coin circle overlaps the creature's opaque pixel box from the last drawn frame
bool isCoinTouchingSprite() {
  if (!lastSpriteDrawnBounds.valid) return false;
  #pragma region Closest point of the box to the coin center
  float closestX = coinX, closestY = coinY;
  if (closestX < lastSpriteDrawnBounds.left) closestX = lastSpriteDrawnBounds.left;
  if (closestX > lastSpriteDrawnBounds.right) closestX = lastSpriteDrawnBounds.right;
  if (closestY < lastSpriteDrawnBounds.top) closestY = lastSpriteDrawnBounds.top;
  if (closestY > lastSpriteDrawnBounds.bottom) closestY = lastSpriteDrawnBounds.bottom;
  #pragma endregion
  float offsetX = coinX - closestX, offsetY = coinY - closestY;
  return offsetX * offsetX + offsetY * offsetY <= (float)COIN_RADIUS * COIN_RADIUS;
}

// ends the round: reward (SPE training, joy, record) and the results screen
void finishCoinGame() {
  coinNewHi = (coinPoints > pet.coinHi);
  coinSpeedGain = pet.coinChaseResult(coinPoints);
  sfxPlay(coinNewHi && coinPoints > 0 ? SFX_MEDAL : SFX_LEVEL);
  coinOver = true;
}

void stepCoinGame() {
  uint32_t now = millis();
  #pragma region Elapsed real time (clamped for an abnormally late frame)
  uint32_t elapsedMs = now - lastCoinStep;
  if (elapsedMs > COIN_STEP_MAX_MS) elapsedMs = COIN_STEP_MAX_MS;
  lastCoinStep = now;
  #pragma endregion

  #pragma region Walk toward the tapped spot and stop there
  float offsetX = coinTargetX - coinPetX, offsetY = coinTargetFeetY - coinPetFeetY;
  float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
  float stepLength = coinPetSpeedPerSecond() * elapsedMs / 1000.0f;
  if (distance <= stepLength) {
    coinPetX = coinTargetX;
    coinPetFeetY = coinTargetFeetY;
  } else {
    coinPetX += offsetX * stepLength / distance;
    coinPetFeetY += offsetY * stepLength / distance;
  }
  if (offsetX < -1) coinPetFacingLeft = true;
  else if (offsetX > 1) coinPetFacingLeft = false;
  #pragma endregion

  #pragma region Coin pickup when the coin touches the creature: +1 point, +5 s (capped at 30 s), new coin
  if (isCoinTouchingSprite()) {
    coinPoints++;
    sfxPlay(SFX_PLAY);
    coinTimeLeftMs += COIN_BONUS_TIME_MS;
    if (coinTimeLeftMs > COIN_MAX_TIME_MS) coinTimeLeftMs = COIN_MAX_TIME_MS;
    if (coinPoints >= COIN_WIN_POINTS) {
      finishCoinGame();
      return;
    }
    respawnCoin();
  } else if (now - coinSpawnedAt >= COIN_LIFETIME_MS) {
    respawnCoin();  // missed: the coin moves somewhere else
  }
  #pragma endregion

  #pragma region Clock: the round ends at 0 s
  coinTimeLeftMs -= (int32_t)elapsedMs;
  if (coinTimeLeftMs <= 0) {
    coinTimeLeftMs = 0;
    finishCoinGame();
  }
  #pragma endregion
}

// gold coin with a darker rim and a shine: three circles, cheaper than a scaled bitmap
void drawCoin(int centerX, int centerY) {
  gfx->fillCircle(centerX, centerY, COIN_RADIUS, C565(0x7e, 0x55, 0x30));
  gfx->fillCircle(centerX, centerY, COIN_RADIUS - 2, C565(0xe0, 0xb8, 0x60));
  gfx->fillCircle(centerX, centerY, COIN_RADIUS - 4, C565(0xff, 0xd9, 0x5e));
  gfx->fillCircle(centerX - COIN_RADIUS / 3, centerY - COIN_RADIUS / 3, COIN_RADIUS / 4, UI_WHITE);
}

void renderCoinGame() {
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (coinOver) {
    drawMinigameResults(coinPoints, coinNewHi && coinPoints > 0, coinPoints >= COIN_WIN_POINTS, pet.coinHi, coinSpeedGain, S_SPD_GAIN_FMT);
    return;
  }

  drawGameScene();
  stepCoinGame();
  if (coinOver) return;  // the round just ended: the results screen draws next frame

  #pragma region Timer (top middle): whole seconds, rounded up
  char timerText[8];
  snprintf(timerText, sizeof(timerText), "%ld", (long)((coinTimeLeftMs + 999) / 1000));
  gfx->setTextColor(coinTimeLeftMs <= 5000 ? UI_BAR_BAD : ink);
  ctext(timerText, COIN_TIMER_TEXT_SIZE, COIN_TIMER_Y);
  #pragma endregion

  drawCoin((int)coinX, (int)coinY);

  #pragma region The creature: walks while it has somewhere to go
  bool walking = coinPetX != coinTargetX || coinPetFeetY != coinTargetFeetY;
  uint8_t act = !walking ? PMD_IDLE : coinPetFacingLeft ? PMD_WALKL : PMD_WALKR;
  drawMinigamePet(act, (int)coinPetX, (int)coinPetFeetY, COIN_PET_MAX_SCALE, COIN_PET_SIZE_PERCENT);
  #pragma endregion

  #pragma region Score (bottom, centered on CX)
  char scoreLine[24];
  snprintf(scoreLine, sizeof(scoreLine), T(S_SCORE_FMT), coinPoints);
  gfx->setTextColor(ink);
  ctext(scoreLine, MINIGAME_SCORE_TEXT_SIZE, MINIGAME_SCORE_Y, MINIGAME_SCORE_MAX_W);
  #pragma endregion

  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);  // quit (see coinGameTap)
  gfx->flush();
}

// ---------- training bag (trains strength) ----------

// the bag can't start while the creature is an egg, asleep or in a ceremony
bool canStartSack() {
  return !pet.isEgg() && !pet.sleeping && !pet.ceremony;
}

void startSack() {
  if (!canStartSack()) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (!timeLeft(sackUntil)) return;  // time is already up
  sackHits++;
  sackShake = 16;  // shakes the bag
}

void drawGameScene();  // prototype (defined further down)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // habitat background
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // result screen
  if (sackOverUntil) {
    if (!timeLeft(sackOverUntil)) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    ctext(b, 4, SY(150));
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    ctext(g, 3, SY(210));
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      ctext(T(S_NEW_RECORD), 2, SY(256));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      ctext(r, 2, SY(256));
    }
    gfx->flush();
    return;
  }

  // the 10 s are over: apply training
  if (!timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // active mashing
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = SK(86);
  gfx->fillRect(CX - SKM(6) / 2, SK(56), SKM(6), top - SK(56), ink);          // hook/rope
  gfx->fillRect(sx - SKM(8) / 2, top - SK(30), SKM(8), SK(34), ink);          // chain
  gfx->fillRoundRect(sx - SK(42), top, SK(84), SK(150), SK(26), C565(0xb5, 0x3a, 0x3a));  // bag
  gfx->fillRoundRect(sx - SK(42), top, SK(84), SK(22), SK(18), C565(0x7e, 0x28, 0x28));   // cap
  gfx->drawRoundRect(sx - SK(42), top, SK(84), SK(150), SK(26), ink);
  gfx->fillRect(sx - SK(42), top + SK(70), SK(84), SKM(4), C565(0x7e, 0x28, 0x28));        // seam

  // hit counter
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  ctext(buf, 6, SY(268));
  ctext(T(S_HIT_FAST), 2, SY(322));

  // time bar
  uint32_t left = sackUntil - now;
  int bw = SX(280), fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, SY(350), bw, SKM(16), SKM(5), UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, SY(350), fw, SKM(16), SKM(5), UI_BAR_OK);

  gfx->flush();
}

// minigame background: the creature's habitat (sky by time of day + biome ground)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = SY(376);  // minigame horizon (lower than HORIZON: leaves room for the ball)
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, LCD_WIDTH, 8, lerp565(top, bot, y, hor));
  if (night)
    drawNightStars(millis(), top, bot, hor);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, LCD_WIDTH, LCD_HEIGHT - hor, soil);
}

void renderGame() {
  // no fillScreen(BLACK): drawGameScene covers the full 466x466. If the
  // previous flush's DMA is still reading the buffer, it sees valid content
  // (not half-painted black), which was the flicker at 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOver) {
    drawMinigameResults(gameScore, gameNewHi && gameScore > 0, false, pet.gameHi, gameAttackGain, S_ATK_GAIN_FMT);
    return;
  }

  drawGameScene();
  stepGame();

  #pragma region Record and lives (top)
  // 24 and not 12: in Japanese "きろく %u" is 10 bytes before the number and the
  // record was cut to its first digit ("きろく 2" with 219). Text buffer sizes
  // are watched by TestBuffersDeTexto in test/test_tools.py.
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  gfx->setTextColor(ink);
  drawGlyphCenteredText(rec, 2, BALL_RECORD_Y);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(LIFE_X0 + i * LIFE_DX, LIFE_Y, LIFE_R, UI_BAR_BAD);
    else gfx->drawCircle(LIFE_X0 + i * LIFE_DX, LIFE_Y, LIFE_R, UI_TRACK);
  }
  #pragma endregion

  uint8_t act = (ballX > gamePetX + 4) ? PMD_WALKR : (ballX < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
  drawMinigamePet(act, (int)gamePetX, GAME_PET_Y, 3, 100);

  #pragma region Score (bottom, centered on CX, like the coin chase; the creature stands just above it)
  char scoreLine[24];
  snprintf(scoreLine, sizeof(scoreLine), T(S_SCORE_FMT), gameScore);
  gfx->setTextColor(ink);
  drawGlyphCenteredText(scoreLine, MINIGAME_SCORE_TEXT_SIZE, MINIGAME_SCORE_Y, MINIGAME_SCORE_MAX_W);
  #pragma endregion

  // impact ring that expands and fades (soft hit feedback)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // the pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - BALL_OFF, (int)ballY - BALL_OFF, BALL_S, false);

  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);  // quit (see gameTap)
  gfx->flush();
}

// ---------- wild battles: random encounter prompt and manual turn-based battle ----------
// BATTLE_*, WILD_PROMPT_*: boards/layout.h (one table per board, shared by drawing and battleTap)

static_assert(TYPE_NAME_COUNT == TYPE_COUNT, "type name table out of sync");

#pragma region Wild battle: stats, scheduling and the random encounter
// the pet's battle stats (level clamped to the battle range, types from the dex)
BattleStats petBattleStats() {
  BattleStats stats = {};
  stats.level = battleLevelFor(pet.level());
  stats.atk = pet.atkStat();
  stats.def = pet.defStat();
  stats.spe = pet.speStat();
  stats.hp = 0;  // derived from level and defense by the battle engine
  if (!pet.isEgg() && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    stats.type1 = DEX_TBL[pet.speciesId].type1;
    stats.type2 = DEX_TBL[pet.speciesId].type2;
  }
  return stats;
}

void scheduleNextWild(uint32_t now) {
  nextWildEligible = now + WILD_COOLDOWN_MS;
}

// fixed 1 in WILD_SHINY_ODDS
bool rollWildShiny() {
  return random(WILD_SHINY_ODDS) == 0;
}

// the prompt only shows over the plain, idle main screen with the user looking at it
bool mainScreenReadyForWild() {
  if (screenOff || dimStage > 0 || wasPressed) return false;
  if (!isMainScreenShowing()) return false;  // every screen, menu and dialog (including this prompt)
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return false;
  if (pet.evolving() || pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return false;
  return true;
}

// opens the "a wild one appeared" prompt for wildDex (level and shiny rolled here)
void openWildPrompt(int16_t wildDex, uint32_t now) {
  if (wildDex < 1 || wildDex > DEX_COUNT) return;
  wildPromptDex = wildDex;
  wildPromptLevel = wildLevelFor(battleLevelFor(pet.level()), (uint8_t)random(100));
  wildPromptShiny = rollWildShiny();
  wildPromptUntil = now + WILD_PROMPT_MS;
  scheduleNextWild(now);
  sfxPlay(SFX_WILD_APPEAR);
}

// once the cooldown is over, one roll per minute; the chance depends on the day phase
void maybeOfferWildEncounter(uint32_t now) {
  if (nextWildEligible == 0) {
    scheduleNextWild(now);
    return;
  }
  if (wildPromptUntil) return;
  if (now - lastWildCheck < WILD_CHECK_MS) return;
  lastWildCheck = now;
  if (timeLeft(nextWildEligible)) return;
  if (!mainScreenReadyForWild()) return;

  #pragma region Encounter chance: lower at night, except for ghost and poison species
  uint8_t dayPhase = dayPhaseFromHour(sceneHour());
  int16_t candidateDex = pickWildSpecies((uint8_t)random(100), dayPhase);
  uint8_t chancePct = (dayPhase == 3) ? 4 : (dayPhase == 0 ? 7 : 8);
  if (dayPhase == 3 && candidateDex >= 1 && candidateDex <= DEX_COUNT) {
    const DexEntry &candidate = DEX_TBL[candidateDex];
    if (candidate.type1 == TYPE_GHOST || candidate.type2 == TYPE_GHOST ||
        candidate.type1 == TYPE_POISON || candidate.type2 == TYPE_POISON) chancePct = 8;
  }
  if ((uint8_t)random(100) >= chancePct) return;
  #pragma endregion

  openWildPrompt(candidateDex, now);
}
#pragma endregion

#pragma region Wild battle: open, close, turns and result
void closeBattle() {
  battleOpen = false;
  battleResolved = false;
  battleAttackMenuOpen = false;
  battleCatchOffered = false;
  battleCatchTried = false;
  battleCatchDone = false;
  battleCatchSuccess = false;
  battleRespectCatch = false;
  battleCatchChance = 0;
  galleryPmd.unload();  // the wild sprite borrowed the gallery's PmdMon
}

// forcedDex 0 = random wild species; forcedLevel 0 = random level around the pet's;
// forcedShiny -1 = roll, 0 / 1 = forced
void startBattleWith(int16_t forcedDex, uint8_t forcedLevel, int8_t forcedShiny, bool fromWildEncounter) {
  if (!canStartWildBattle(pet.isEgg(), pet.sleeping, pet.ceremony)) return;
  // galleryPmd is shared with the gallery; full screens are left alone (serial commands)
  if (galleryOpen || gameOpen || coinOpen || sackOpen || kbOpen || settingsScreen != SETTINGS_CLOSED) return;

  #pragma region Close the prompt and any main screen menu or dialog
  wildPromptUntil = 0;
  playMenuUntil = 0;
  feedMenuUntil = 0;
  confirmUntil = 0;
  choiceKind = 0;
  cardOpen = false;
  scheduleNextWild(millis());
  #pragma endregion

  #pragma region Pick the wild species, level and shiny
  uint8_t petBattleLevel = battleLevelFor(pet.level());
  if (forcedDex >= 1 && forcedDex <= DEX_COUNT) {
    battleDex = forcedDex;
    battleLevel = forcedLevel ? forcedLevel : wildLevelFor(petBattleLevel, (uint8_t)random(100));
  } else {
    battleDex = pickWildSpecies((uint8_t)random(100), dayPhaseFromHour(sceneHour()));
    battleLevel = wildLevelFor(petBattleLevel, (uint8_t)random(100));
  }
  if (battleDex < 1 || battleDex > DEX_COUNT) return;
  battleShiny = forcedShiny < 0 ? rollWildShiny() : forcedShiny != 0;
  #pragma endregion

  #pragma region Reset the battle state
  battlePlayer = petBattleStats();
  battleEnemy = wildBattleStats(battleDex, battleLevel);
  battleEnemy.hp = 0;
  battleRun = beginBattleRuntime(battlePlayer, battleEnemy);
  battleTurn = {};
  battleReward = {};
  battlePlayerMoveText[0] = 0;
  battleWildMoveText[0] = 0;
  battleAttackMenuOpen = false;
  battleLowHpWarned = false;
  battleCatchOffered = false;
  battleCatchTried = false;
  battleCatchDone = false;
  battleCatchSuccess = false;
  battleRespectCatch = false;
  battleCatchChance = 0;
  battleResolved = false;
  battleFromWildEncounter = fromWildEncounter;
  battleOpen = true;
  #pragma endregion

  galleryPmd.unload();
  galleryPmd.load(battleDex, battleShiny);  // streaming on the CYD: headers only, one frame per draw
  sfxPlay(SFX_TAP);
}

void startBattle() {
  startBattleWith(0, 0, -1);
}

// applies the win (reward, catch offer) or the loss (respect catch after a close loss)
void finishBattle() {
  if (battleResolved) return;
  battleResolved = true;
  battleAttackMenuOpen = false;
  if (battleTurn.playerWon) {
    bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
    battleReward = pet.applyBattleWin(battleDex, closeWin);
    battleCatchOffered = true;
    battleRespectCatch = false;
    battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                               !battleFromWildEncounter);
    sfxPlay(SFX_BATTLE_WIN);
  } else {
    battleReward = {};
    pet.applyBattleLoss();
    bool closeLoss = battleRun.enemyHp > 0 && battleRun.enemyHp * 100UL <= battleRun.enemyMaxHp * 30UL;
    battleCatchChance = closeLoss ? pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level,
                                                                  !battleFromWildEncounter) : 0;
    battleCatchOffered = battleCatchChance > 0;
    battleRespectCatch = battleCatchOffered;
    sfxPlay(SFX_BATTLE_LOSS);
  }
}

// one turn: runs the engine, writes the message line and plays the turn sounds
void performBattleAction(BattleAction action) {
  if (battleResolved) return;
  battleAttackMenuOpen = false;
  battleTurn = stepBattle(battleRun, action, (uint8_t)random(100));

  #pragma region Our move (under our sprite): rest, dodge, or the damage our attack did
  if (battleTurn.restFailed) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_NO_REST));
  } else if (battleTurn.playerRested) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), T(S_RESTED_FMT), battleTurn.playerHeal);
  } else if (action == BATTLE_DODGE) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_DODGED));
  } else if (battleTurn.playerDamage > 0) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), T(S_HIT_FMT), battleTurn.playerDamage);
  } else {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_MISSED));  // the wild one dodged it
  }
  #pragma endregion

  #pragma region Move of the wild one (under its sprite): dodge, or the damage its attack did
  if (battleTurn.restFailed) {
    battleWildMoveText[0] = 0;  // no turn was played
  } else if (battleTurn.enemyDodged && battleTurn.enemyDamage == 0) {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), "%s", T(S_DODGED));
  } else if (battleTurn.enemyDamage > 0) {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), T(S_HIT_FMT), battleTurn.enemyDamage);
  } else if (battleRun.enemyHp == 0) {
    battleWildMoveText[0] = 0;  // fainted before acting
  } else {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), "%s", T(S_MISSED));  // we dodged it
  }
  #pragma endregion

  if (battleTurn.battleEnded) {
    finishBattle();
    return;
  }

  #pragma region Turn sounds (queued): our move, then the hit taken, then the low HP warning
  if (battleTurn.restFailed) sfxPlay(SFX_DENY);
  else if (battleTurn.playerRested) sfxPlay(SFX_HEART);
  else if (battleTurn.playerDamage > 0) sfxPlay(SFX_BATTLE_HIT);
  else sfxPlay(SFX_TAP);
  if (battleTurn.enemyDamage > 0) sfxPlay(SFX_BATTLE_HURT);
  if (!battleLowHpWarned && battleRun.playerHp > 0 && battleRun.playerHp <= battleRun.playerMaxHp * 3 / 10) {
    battleLowHpWarned = true;
    sfxPlay(SFX_DENY);
  }
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle: touch
// a drawn box grown by padX / padY on each side (the battle's generous hit areas)
bool isTouchInsidePaddedRect(int16_t x, int16_t y, int left, int top, int width, int height, int padX, int padY) {
  return isTouchInsideRect(x, y, left - padX, top - padY, width + 2 * padX, height + 2 * padY);
}

void battleTap(int16_t x, int16_t y) {
  #pragma region Result view: Catch / Leave, then OK
  if (battleResolved) {
    if (battleCatchOffered && !battleCatchDone) {
      if (isTouchInsidePaddedRect(x, y, BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
        bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
        battleCatchTried = true;
        battleCatchDone = true;
        if (battleRespectCatch) {
          battleCatchSuccess = pet.tryRespectCatchWild(battleDex, battleLevel, battlePlayer.level,
                                                       (uint8_t)random(100), battleShiny, !battleFromWildEncounter);
          battleCatchChance = pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level,
                                                            !battleFromWildEncounter);
        } else {
          battleCatchSuccess = pet.tryCatchWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                                (uint8_t)random(100), battleShiny, !battleFromWildEncounter);
          battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                                     !battleFromWildEncounter);
        }
        sfxPlay(battleCatchSuccess ? SFX_CATCH_OK : SFX_CATCH_FAIL);
        galleryDirty = true;  // the grid shows the new catch
        return;
      }
      if (isTouchInsidePaddedRect(x, y, BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
        battleCatchDone = true;
        battleCatchTried = false;
        sfxPlay(SFX_TAP);
      }
      return;
    }
    if (isTouchInsidePaddedRect(x, y, BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
      closeBattle();
      sfxPlay(SFX_TAP);
    }
    return;
  }
  #pragma endregion

  #pragma region Quick / Heavy overlay: any other tap closes it
  if (battleAttackMenuOpen) {
    if (isTouchInsidePaddedRect(x, y, BATTLE_QUICK_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H,
                                BATTLE_ATTACK_MENU_HIT_PAD_X, BATTLE_ATTACK_MENU_HIT_PAD_Y)) {
      performBattleAction(BATTLE_ATTACK_QUICK);
      return;
    }
    if (isTouchInsidePaddedRect(x, y, BATTLE_HEAVY_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H,
                                BATTLE_ATTACK_MENU_HIT_PAD_X, BATTLE_ATTACK_MENU_HIT_PAD_Y)) {
      performBattleAction(BATTLE_ATTACK_HEAVY);
      return;
    }
    battleAttackMenuOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  #pragma endregion

  #pragma region Run, then the three action buttons
  if (isTouchInsideRect(x, y, BATTLE_RUN_HIT_X, BATTLE_RUN_HIT_Y, BATTLE_RUN_HIT_W, BATTLE_RUN_HIT_H)) {
    closeBattle();
    sfxPlay(SFX_TAP);
    return;
  }
  if (isTouchInsidePaddedRect(x, y, BATTLE_ATTACK_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H,
                              BATTLE_ACTION_HIT_PAD_X, BATTLE_ACTION_HIT_PAD_Y)) {
    battleAttackMenuOpen = true;
    sfxPlay(SFX_TAP);
  } else if (isTouchInsidePaddedRect(x, y, BATTLE_DODGE_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H,
                                     BATTLE_ACTION_HIT_PAD_X, BATTLE_ACTION_HIT_PAD_Y)) {
    performBattleAction(BATTLE_DODGE);
  } else if (isTouchInsidePaddedRect(x, y, BATTLE_REST_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H,
                                     BATTLE_ACTION_HIT_PAD_X, BATTLE_ACTION_HIT_PAD_Y)) {
    performBattleAction(BATTLE_REST);
  }
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle: drawing helpers
void drawBattleHpBar(int x, int y, uint16_t currentHp, uint16_t maxHp, uint16_t color) {
  if (maxHp == 0) maxHp = 1;
  int fillWidth = (int)((uint32_t)currentHp * BATTLE_HP_W / maxHp);
  if (fillWidth > BATTLE_HP_W) fillWidth = BATTLE_HP_W;
  gfx->fillRoundRect(x, y, BATTLE_HP_W, BATTLE_HP_H, BATTLE_HP_R, UI_TRACK);
  if (fillWidth > 2) gfx->fillRoundRect(x, y, fillWidth, BATTLE_HP_H, BATTLE_HP_R, color);
}

// "+N ATK/DEF/SPD" line for the win reward ("" when there is none)
void battleRewardText(char *buffer, size_t bufferSize) {
  if (battleReward.amount == 0) {
    buffer[0] = 0;
    return;
  }
  StrId rewardFormat = S_SPD_GAIN_FMT;
  if (battleReward.stat == BATTLE_REWARD_ATK) rewardFormat = S_ATK_GAIN_FMT;
  else if (battleReward.stat == BATTLE_REWARD_DEF) rewardFormat = S_DEF_GAIN_FMT;
  snprintf(buffer, bufferSize, T(rewardFormat), battleReward.amount);
}

// label centered in a button box (shrinks to fit; text color set by the caller)
void drawBattleButtonLabel(int left, int top, int width, int height, const char *label, int preferredSize) {
  drawTextCenteredInBox(label, preferredSize, left, top, width, height);
}

// one line of text centered on CX in the result column
void drawBattleResultLine(const char *text, int preferredSize, int top) {
  drawTextCenteredInBox(text, preferredSize, CX - BATTLE_RESULT_TEXT_MAX_W / 2, top, BATTLE_RESULT_TEXT_MAX_W, 8 * preferredSize);
}

// chip color per battle type (the fork's palette; grey for TYPE_NONE / unknown)
uint16_t battleTypeColor(uint8_t type) {
  switch (type) {
    case TYPE_FIRE: return 0xEA87;
    case TYPE_WATER: return 0x4C98;
    case TYPE_ELECTRIC: return 0xBCA1;
    case TYPE_GRASS: return 0x3C49;
    case TYPE_ICE: return 0x5D99;
    case TYPE_FIGHTING: return 0xA2A5;
    case TYPE_POISON: return 0x8A73;
    case TYPE_GROUND: return 0xB447;
    case TYPE_FLYING: return 0x8D7F;
    case TYPE_PSYCHIC: return 0xD28F;
    case TYPE_BUG: return 0x7CC4;
    case TYPE_ROCK: return 0x9407;
    case TYPE_GHOST: return 0x6B33;
    case TYPE_DRAGON: return 0x5A5F;
    case TYPE_DARK: return 0x5ACB;
    case TYPE_STEEL: return 0xA534;
    case TYPE_FAIRY: return 0xF3B7;
    default: return 0x8C4D;
  }
}

#pragma region Type chips (battle screen and Pokedex detail share the drawing, each with its own size)
struct TypeChipStyle {
  uint8_t textSize;
  int16_t height, padding, radius, gap;
};
static const TypeChipStyle BATTLE_TYPE_CHIP_STYLE = { 1, BATTLE_CHIP_H, BATTLE_CHIP_PAD, BATTLE_CHIP_R, BATTLE_CHIP_GAP };
static const TypeChipStyle GALLERY_TYPE_CHIP_STYLE = { GALLERY_TYPE_CHIP_TEXT_SIZE, GALLERY_TYPE_CHIP_H,
                                                       GALLERY_TYPE_CHIP_PAD, GALLERY_TYPE_CHIP_R, GALLERY_TYPE_CHIP_GAP };

int typeChipWidth(uint8_t type, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE) {
  setSize(style.textSize);  // textW() with a CJK font measures with the active size
  return (int)textW(typeName(type), style.textSize) + 2 * style.padding;
}

// width of one or two chips side by side
int typeChipsTotalWidth(const DexEntry &entry, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE) {
  int firstWidth = typeChipWidth(entry.type1, style);
  if (entry.type2 == TYPE_NONE) return firstWidth;
  return firstWidth + style.gap + typeChipWidth(entry.type2, style);
}

void drawTypeChip(int x, int y, uint8_t type, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE) {
  if (type == TYPE_NONE) return;
  int chipWidth = typeChipWidth(type, style);
  gfx->fillRoundRect(x, y, chipWidth, style.height, style.radius, lerp565(battleTypeColor(type), UI_WHITE, 5, 8));
  gfx->drawRoundRect(x, y, chipWidth, style.height, style.radius, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(style.textSize);
  // glyph height actually drawn: 8 px per scale step in the 5x7 font, 16 px in the CJK fonts
  int textHeight = (gCjkFont ? 16 : 8) * gTextSize;
  setCur(x + style.padding, y + (style.height - textHeight) / 2);
  printT(typeName(type));
}

// one or two chips starting at x (or ending at x when alignRight); returns the left edge
int drawTypeChips(int x, int y, const DexEntry &entry, bool alignRight, const TypeChipStyle &style = BATTLE_TYPE_CHIP_STYLE) {
  int firstWidth = typeChipWidth(entry.type1, style);
  int totalWidth = typeChipsTotalWidth(entry, style);
  int chipsLeft = alignRight ? x - totalWidth : x;
  drawTypeChip(chipsLeft, y, entry.type1, style);
  if (entry.type2 != TYPE_NONE) drawTypeChip(chipsLeft + firstWidth + style.gap, y, entry.type2, style);
  return chipsLeft;
}
#pragma endregion

// the same pokeball as the minigame: this species is already caught
void drawCaughtBattleMarker(int x, int y) {
  drawMap(SPR_ICON_PLAY, 16, x, y, BATTLE_CAUGHT_S, false);
}
#pragma endregion

#pragma region Wild encounter prompt drawing
void drawWildPrompt() {
  gfx->fillRoundRect(WILD_PROMPT_X, WILD_PROMPT_Y, WILD_PROMPT_W, WILD_PROMPT_H, WILD_PROMPT_R, UI_WHITE);
  gfx->drawRoundRect(WILD_PROMPT_X, WILD_PROMPT_Y, WILD_PROMPT_W, WILD_PROMPT_H, WILD_PROMPT_R, UI_INK);
  const int textLeft = WILD_PROMPT_X + 8, textWidth = WILD_PROMPT_W - 16;

  #pragma region Question and the wild species line
  gfx->setTextColor(UI_INK);
  drawTextCenteredInBox(T(S_WILD_Q), WILD_PROMPT_TITLE_SIZE, textLeft, WILD_PROMPT_TITLE_Y, textWidth, 8 * WILD_PROMPT_TITLE_SIZE);
  char wildLine[48];
  snprintf(wildLine, sizeof(wildLine), "%s%s Lv.%u", wildPromptShiny ? "*" : "", dexName(wildPromptDex), wildPromptLevel);
  drawTextCenteredInBox(wildLine, WILD_PROMPT_NAME_SIZE, textLeft, WILD_PROMPT_NAME_Y, textWidth, 8 * WILD_PROMPT_NAME_SIZE);
  #pragma endregion

  #pragma region FIGHT / LATER buttons
  gfx->fillRoundRect(WILD_PROMPT_BTN_X, WILD_PROMPT_FIGHT_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, WILD_PROMPT_BTN_R, UI_BAR_BAD);
  gfx->fillRoundRect(WILD_PROMPT_BTN_X, WILD_PROMPT_LATER_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, WILD_PROMPT_BTN_R, UI_TRACK);
  gfx->setTextColor(UI_WHITE);
  drawBattleButtonLabel(WILD_PROMPT_BTN_X, WILD_PROMPT_FIGHT_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, T(S_FIGHT), WILD_PROMPT_BTN_TEXT_SIZE);
  gfx->setTextColor(UI_INK);
  drawBattleButtonLabel(WILD_PROMPT_BTN_X, WILD_PROMPT_LATER_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, T(S_LATER), WILD_PROMPT_BTN_TEXT_SIZE);
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle screen
void renderBattle() {
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  const DexEntry &petEntry = DEX_TBL[pet.speciesId];
  const DexEntry &wildEntry = DEX_TBL[battleDex];

  #pragma region Title and the run button (top right, battle in progress only)
  gfx->setTextColor(ink);
  drawTextCenteredInBox(T(battleFromWildEncounter ? S_WILD_BATTLE : S_BATTLE), BATTLE_TITLE_SIZE,
                        CX - BATTLE_TITLE_MAX_W / 2, BATTLE_TITLE_Y, BATTLE_TITLE_MAX_W, 8 * BATTLE_TITLE_SIZE);
  if (!battleResolved) {
    gfx->fillRoundRect(BATTLE_RUN_X, BATTLE_RUN_Y, BATTLE_RUN_W, BATTLE_RUN_H, BATTLE_RUN_R, UI_TRACK);
    gfx->setTextColor(UI_INK);
    drawBattleButtonLabel(BATTLE_RUN_X, BATTLE_RUN_Y, BATTLE_RUN_W, BATTLE_RUN_H, T(S_RUN_BATTLE), BATTLE_BTN_TEXT_SIZE);
  }
  #pragma endregion

  #pragma region Info columns: name and level, HP bar, type chips (pet left, wild right)
  // 40: the longest dex name in UTF-8 or an 11-byte nickname, plus " Lv.100"
  char petLabel[40], wildLabel[40];
  snprintf(petLabel, sizeof(petLabel), "%s Lv.%u", pet.nick[0] ? pet.nick : dexName(pet.speciesId), battlePlayer.level);
  snprintf(wildLabel, sizeof(wildLabel), "%s%s Lv.%u", battleShiny ? "*" : "", dexName(battleDex), battleLevel);
  gfx->setTextColor(ink);
  int nameSize = shrinkTextSizeToWidth(petLabel, BATTLE_NAME_SIZE, BATTLE_NAME_MAX_W);
  setCur(BATTLE_PET_NAME_X, BATTLE_NAME_Y + 4 * (BATTLE_NAME_SIZE - nameSize));
  printT(petLabel);
  nameSize = shrinkTextSizeToWidth(wildLabel, BATTLE_NAME_SIZE, BATTLE_NAME_MAX_W);
  setCur(BATTLE_WILD_NAME_RIGHT - (int)textW(wildLabel, nameSize), BATTLE_NAME_Y + 4 * (BATTLE_NAME_SIZE - nameSize));
  printT(wildLabel);

  drawBattleHpBar(BATTLE_PET_HP_X, BATTLE_HP_Y, battleRun.playerHp, battleRun.playerMaxHp, UI_BAR_OK);
  drawBattleHpBar(BATTLE_WILD_HP_X, BATTLE_HP_Y, battleRun.enemyHp, battleRun.enemyMaxHp, UI_BAR_BAD);
  drawTypeChips(BATTLE_PET_CHIP_X, BATTLE_CHIP_Y, petEntry, false);
  int wildChipsLeft = drawTypeChips(BATTLE_WILD_CHIP_RIGHT, BATTLE_CHIP_Y, wildEntry, true);
  // left of the wild type chips: next to the name it would leave the round display
  if (pet.isCaught(battleDex)) drawCaughtBattleMarker(wildChipsLeft - BATTLE_CAUGHT_DX, BATTLE_CAUGHT_Y);
  #pragma endregion

  #pragma region Sprites (idle animation; thumbnail when there is no PMD sprite)
  if (pmd.loaded) {
    drawPmdAct(PMD_IDLE, BATTLE_PET_CX, BATTLE_GROUND_Y, millis(), true, false, BATTLE_SPRITE_MAX_S);
  } else {
    const uint8_t *petThumb = thumbs.get(pet.speciesId);
    if (petThumb) drawThumbCell(petThumb, BATTLE_PET_THUMB_X, BATTLE_THUMB_Y, BATTLE_THUMB_S, false, BATTLE_THUMB_CELL);
  }
  if (galleryPmd.loaded) {
    drawPmdActM(galleryPmd, PMD_IDLE, BATTLE_WILD_CX, BATTLE_GROUND_Y, millis(), true, false, BATTLE_SPRITE_MAX_S);
  } else {
    const uint8_t *wildThumb = thumbs.get(battleDex);
    if (wildThumb) drawThumbCell(wildThumb, BATTLE_WILD_THUMB_X, BATTLE_THUMB_Y, BATTLE_THUMB_S, false, BATTLE_THUMB_CELL);
  }
  #pragma endregion

  if (battleResolved) {
    #pragma region Result: panel (rect board), WIN / LOSS, rounds and damage, info line
    uint16_t resultInk = ink;
    if (BATTLE_RESULT_PANEL_W > 0) {
      gfx->fillRoundRect(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                         BATTLE_RESULT_PANEL_R, UI_WHITE);
      gfx->drawRoundRect(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                         BATTLE_RESULT_PANEL_R, UI_INK);
      resultInk = UI_INK;
    }
    gfx->setTextColor(battleTurn.playerWon ? UI_BAR_OK : UI_BAR_BAD);
    drawBattleResultLine(battleTurn.playerWon ? T(S_WIN) : T(S_LOSS), BATTLE_RESULT_TITLE_SIZE, BATTLE_RESULT_TITLE_Y);
    char roundsLine[32];
    char damageLine[48];
    snprintf(roundsLine, sizeof(roundsLine), T(S_ROUNDS_FMT), battleRun.round);
    snprintf(damageLine, sizeof(damageLine), T(S_DAMAGE_FMT), battleRun.playerDamageTotal, battleRun.enemyDamageTotal);
    gfx->setTextColor(resultInk);
    drawBattleResultLine(roundsLine, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
    drawBattleResultLine(damageLine, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_DAMAGE_Y);
    if (battleCatchDone && battleCatchTried) {
      gfx->setTextColor(battleCatchSuccess ? UI_BAR_OK : UI_BAR_BAD);
      drawBattleResultLine(battleCatchSuccess ? T(S_CAUGHT_OK) : T(S_ESCAPED), BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
    } else if (battleTurn.playerWon) {
      char rewardLine[48];
      battleRewardText(rewardLine, sizeof(rewardLine));
      if (rewardLine[0]) {
        gfx->setTextColor(UI_BAR_WARN);
        drawBattleResultLine(rewardLine, BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
      }
    } else if (battleRespectCatch && battleCatchOffered && !battleCatchDone) {
      gfx->setTextColor(UI_BAR_WARN);
      drawBattleResultLine(T(S_CLOSE_CHANCE), BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
    }
    #pragma endregion

    #pragma region Result buttons: Catch / Leave while a catch is offered, then OK
    gfx->setTextColor(UI_BG_DAY);
    if (battleCatchOffered && !battleCatchDone) {
      gfx->fillRoundRect(BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK);
      gfx->fillRoundRect(BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_TRACK);
      drawBattleButtonLabel(BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, T(S_CATCH_WILD), BATTLE_BTN_TEXT_SIZE);
      gfx->setTextColor(UI_INK);
      drawBattleButtonLabel(BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, T(S_LEAVE_WILD), BATTLE_BTN_TEXT_SIZE);
    } else {
      gfx->fillRoundRect(BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK);
      drawBattleButtonLabel(BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, T(S_OK), BATTLE_OK_TEXT_SIZE);
    }
    #pragma endregion
  } else {
    #pragma region Moves of the last turn, each centered under its sprite (damage to us in red)
    if (battlePlayerMoveText[0]) {
      gfx->setTextColor(ink);
      drawTextCenteredInBox(battlePlayerMoveText, BATTLE_MOVE_SIZE, BATTLE_PET_CX - BATTLE_MOVE_W / 2, BATTLE_MOVE_Y,
                            BATTLE_MOVE_W, 8 * BATTLE_MOVE_SIZE);
    }
    if (battleWildMoveText[0]) {
      gfx->setTextColor(battleTurn.enemyDamage > 0 ? UI_BAR_BAD : ink);
      drawTextCenteredInBox(battleWildMoveText, BATTLE_MOVE_SIZE, BATTLE_WILD_CX - BATTLE_MOVE_W / 2, BATTLE_MOVE_Y,
                            BATTLE_MOVE_W, 8 * BATTLE_MOVE_SIZE);
    }
    #pragma endregion

    #pragma region Quick / Heavy overlay (after Attack)
    if (battleAttackMenuOpen) {
      gfx->fillRoundRect(BATTLE_QUICK_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H, BATTLE_ATTACK_MENU_R, UI_BAR_BAD);
      gfx->fillRoundRect(BATTLE_HEAVY_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H, BATTLE_ATTACK_MENU_R, UI_BAR_WARN);
      gfx->setTextColor(UI_BG_DAY);
      drawBattleButtonLabel(BATTLE_QUICK_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H, T(S_QUICK_ATTACK), BATTLE_BTN_TEXT_SIZE);
      drawBattleButtonLabel(BATTLE_HEAVY_X, BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W, BATTLE_ATTACK_MENU_H, T(S_HEAVY_ATTACK), BATTLE_BTN_TEXT_SIZE);
    }
    #pragma endregion

    #pragma region Attack / Dodge / Rest n
    gfx->fillRoundRect(BATTLE_ATTACK_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, BATTLE_ACTION_R, UI_BAR_BAD);
    gfx->fillRoundRect(BATTLE_DODGE_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, BATTLE_ACTION_R, 0x4C98);
    gfx->fillRoundRect(BATTLE_REST_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, BATTLE_ACTION_R, UI_BAR_OK);
    gfx->setTextColor(UI_BG_DAY);
    drawBattleButtonLabel(BATTLE_ATTACK_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, T(S_ATTACK), BATTLE_BTN_TEXT_SIZE);
    drawBattleButtonLabel(BATTLE_DODGE_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, T(S_DODGE), BATTLE_BTN_TEXT_SIZE);
    // 40: a CJK "rest" label plus " %u"
    char restLabel[40];
    snprintf(restLabel, sizeof(restLabel), "%s %u", T(S_REST), battleRun.restUsesLeft);
    drawBattleButtonLabel(BATTLE_REST_X, BATTLE_ACTION_Y, BATTLE_ACTION_W, BATTLE_ACTION_H, restLabel, BATTLE_BTN_TEXT_SIZE);
    #pragma endregion
  }

  gfx->flush();
}
#pragma endregion

// ---------- creature info card (vertical swipe) ----------

// x where the info card bars start. It was fixed at 150, which was plenty for
// 3-character Latin labels but not for the wider Japanese ones: こうげき ran
// into the bar. It is computed from the longest label so the four bars stay
// aligned in any language, with the original value as a floor (so nothing
// changes in the Latin-script languages).
// Requires text size 2 to already be set, because textW() needs it.
static int statBarX() {
  const StrId ids[] = { S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_VIN };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  int x = SX(96) + ancho + 12;   // 12 px of space between label and bar
  return x < SX(150) ? SX(150) : x;
}

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(SX(96), y);
  printT(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  setCur(SX(330), y);
  printT(num);
  int bx = statBarX();
  int bw = SX(310) - bx;   // the bar always ends at SX(310), leaving space before the number
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
}

// ---------- on-screen time setting (settings menu > Time) ----------
// The user sets their LOCAL time by eye; the firmware uses it as-is, so there
// is no time zone to manage. Preserves the day (does not break streak/age).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  settingsScreen = SETTINGS_TIME;
}

// OK: sets the time and goes back to the settings menu
void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  settingsScreen = SETTINGS_MENU;
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, CLK_BTN, CLK_BTN, CLK_BTN_R, UI_WHITE);
  gfx->drawRoundRect(x, y, CLK_BTN, CLK_BTN, CLK_BTN_R, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(TS(4));
  setCur(x + CLK_GLYPH_DX, y + CLK_GLYPH_DY);
  printT(l);
}

#pragma region Settings shared helpers
// SETTINGS_*, CLK_*, CLOSE_BTN_*: boards/layout.h (one table per board, shared by
// drawing and touch)

// Largest size <= preferredSize at which text fits maxWidth, on BOTH boards
// (fitSize() only shrinks on the rect board, and these screens show translated
// labels on the round one too). Leaves that size applied, like fitSize().
int shrinkTextSizeToWidth(const char *text, int preferredSize, int maxWidth) {
  int textSize = preferredSize;
  for (; textSize > 1; textSize--) {
    setSize(textSize);
    if (textW(text, textSize) <= maxWidth) break;
  }
  setSize(textSize);
  return textSize;
}

// Prints text centered on both axes inside a box, shrunk to the box width minus
// SETTINGS_TEXT_SIDE_PAD per side. Height is taken as 8 px per size step: exact
// for the classic font, close enough for the CJK one (16 px glyphs at half scale).
void drawTextCenteredInBox(const char *text, int preferredSize, int boxLeft, int boxTop, int boxWidth, int boxHeight) {
  int textSize = shrinkTextSizeToWidth(text, preferredSize, boxWidth - 2 * SETTINGS_TEXT_SIDE_PAD);
  setCur(boxLeft + (boxWidth - (int)textW(text, textSize)) / 2, boxTop + (boxHeight - 8 * textSize) / 2);
  printT(text);
}

// Line of text centered on CX in the title-wide box starting at top (titles and section labels)
void drawSettingsLabel(const char *text, int top) {
  drawTextCenteredInBox(text, TS(3), SETTINGS_TITLE_X, top, SETTINGS_TITLE_MAX_W, 8 * TS(3));
}

// Clears the screen and draws the title shared by every settings screen
void beginSettingsScreen(const char *title) {
  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  drawSettingsLabel(title, SETTINGS_TITLE_Y);
}

// Round X at the top right: the only way out of every settings screen
void drawSettingsCloseButton() {
  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);
}

bool isTouchOnSettingsCloseButton(int16_t touchX, int16_t touchY) {
  return isTouchInsideCircle(touchX, touchY, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R);
}
#pragma endregion

#pragma region Settings menu screen
// Title, four large rows (Screen Off, Audio / Video, Time, Language) and the firmware version
#define SETTINGS_MENU_ROW_COUNT 4
static const StrId SETTINGS_MENU_ROW_LABELS[SETTINGS_MENU_ROW_COUNT] = { S_MENU_SCREEN_OFF, S_MENU_AV, S_MENU_TIME, S_MENU_LANG };

void openSettingsMenu() { settingsScreen = SETTINGS_MENU; }

int settingsMenuRowTop(int rowIndex) {
  return SETTINGS_MENU_ROW_Y + rowIndex * (SETTINGS_MENU_ROW_H + SETTINGS_MENU_ROW_GAP);
}

void renderSettingsMenu() {
  beginSettingsScreen(T(S_MENU_TITLE));

  #pragma region Rows: label centered, arrow at the right (equal margin on the left keeps it centered)
  const int labelMargin = SETTINGS_MENU_ARROW_INSET + SETTINGS_MENU_ARROW_SIZE / 2;
  for (int rowIndex = 0; rowIndex < SETTINGS_MENU_ROW_COUNT; rowIndex++) {
    int rowTop = settingsMenuRowTop(rowIndex);
    gfx->fillRoundRect(SETTINGS_MENU_ROW_X, rowTop, SETTINGS_MENU_ROW_W, SETTINGS_MENU_ROW_H, SETTINGS_MENU_ROW_R, UI_WHITE);
    gfx->drawRoundRect(SETTINGS_MENU_ROW_X, rowTop, SETTINGS_MENU_ROW_W, SETTINGS_MENU_ROW_H, SETTINGS_MENU_ROW_R, UI_INK);
    gfx->setTextColor(UI_INK);
    drawTextCenteredInBox(T(SETTINGS_MENU_ROW_LABELS[rowIndex]), TS(3), SETTINGS_MENU_ROW_X + labelMargin, rowTop,
                          SETTINGS_MENU_ROW_W - 2 * labelMargin, SETTINGS_MENU_ROW_H);
    drawPageArrow(gfx, SETTINGS_MENU_ROW_X + SETTINGS_MENU_ROW_W - SETTINGS_MENU_ARROW_INSET, rowTop + SETTINGS_MENU_ROW_H / 2,
                  SETTINGS_MENU_ARROW_SIZE, 1, UI_INK);
  }
  #pragma endregion

  #pragma region Firmware version (discreet, at the very bottom)
  char versionText[20];
  snprintf(versionText, sizeof(versionText), "TamaPoke v%s", FW_VERSION);
  gfx->setTextColor(UI_LABEL);
  setSize(1);
  setCur(centerX(versionText, 1), SETTINGS_MENU_VERSION_Y);
  printT(versionText);
  #pragma endregion

  drawSettingsCloseButton();
  gfx->flush();
}

void handleSettingsMenuTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnSettingsCloseButton(touchX, touchY)) {  // back to the main screen
    settingsScreen = SETTINGS_CLOSED;
    return;
  }
  for (int rowIndex = 0; rowIndex < SETTINGS_MENU_ROW_COUNT; rowIndex++) {
    if (!isTouchInsideRect(touchX, touchY, SETTINGS_MENU_ROW_X, settingsMenuRowTop(rowIndex), SETTINGS_MENU_ROW_W, SETTINGS_MENU_ROW_H)) continue;
    if (rowIndex == 0) {  // Screen Off: back to the main screen with the panel dark (see turnScreenOff)
      settingsScreen = SETTINGS_CLOSED;
      turnScreenOff();
    }
    else if (rowIndex == 1) settingsScreen = SETTINGS_AUDIO_VIDEO;
    else if (rowIndex == 2) openClock();  // loads the current time into the editor
    else settingsScreen = SETTINGS_LANGUAGE;
    return;
  }
}
#pragma endregion

#pragma region Audio / video settings screen
// Sound on/off pill and a brightness bar with -/+ buttons (same squares as the
// time screen). Each step applies on the next loop (updateBrightness); the level
// is written to NVS only when the screen closes, since every NVS write freezes
// both cores for ~1s.
void loadBrightnessLevelFromNvs() {
  Preferences preferences;
  preferences.begin("tamapoke", true);  // read-only
  settingsBrightnessLevel = preferences.getUChar("bright", SETTINGS_BRIGHTNESS_DEFAULT);
  preferences.end();
  if (settingsBrightnessLevel > SETTINGS_BRIGHTNESS_MAX) settingsBrightnessLevel = SETTINGS_BRIGHTNESS_DEFAULT;
  settingsBrightnessSavedLevel = settingsBrightnessLevel;
}

void saveBrightnessLevelToNvs() {
  if (settingsBrightnessLevel == settingsBrightnessSavedLevel) return;
  Preferences preferences;
  preferences.begin("tamapoke", false);
  preferences.putUChar("bright", settingsBrightnessLevel);
  preferences.end();
  settingsBrightnessSavedLevel = settingsBrightnessLevel;
}

void renderAudioVideoSettings() {
  beginSettingsScreen(T(S_MENU_AV));

  #pragma region Sound: label and on/off pill
  drawSettingsLabel(T(S_SOUND), SETTINGS_SOUND_LABEL_Y);
  bool soundOn = audioEnabled();
  gfx->fillRoundRect(SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y, SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H, SETTINGS_PILL_R,
                     soundOn ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y, SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H, SETTINGS_PILL_R, UI_INK);
  gfx->setTextColor(soundOn ? UI_BG_DAY : UI_INK);
  drawTextCenteredInBox(soundOn ? T(S_ON) : T(S_OFF), TS(3), SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y,
                        SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H);
  #pragma endregion

  #pragma region Brightness: label, -/+ buttons and the level bar between them
  gfx->setTextColor(UI_INK);
  drawSettingsLabel(T(S_BRIGHTNESS), SETTINGS_BRIGHTNESS_LABEL_Y);
  drawClockBtn(SETTINGS_BRIGHTNESS_MINUS_X, SETTINGS_BRIGHTNESS_ROW_Y, "-");
  drawClockBtn(SETTINGS_BRIGHTNESS_PLUS_X, SETTINGS_BRIGHTNESS_ROW_Y, "+");
  const int barTop = SETTINGS_BRIGHTNESS_ROW_Y + (CLK_BTN - SETTINGS_BRIGHTNESS_BAR_H) / 2;
  gfx->fillRoundRect(SETTINGS_BRIGHTNESS_BAR_X, barTop, SETTINGS_BRIGHTNESS_BAR_W, SETTINGS_BRIGHTNESS_BAR_H,
                     SETTINGS_BRIGHTNESS_BAR_H / 2, UI_TRACK);
  int fillWidth = (SETTINGS_BRIGHTNESS_BAR_W - 4) * settingsBrightnessLevel / SETTINGS_BRIGHTNESS_MAX;
  if (fillWidth > 0)
    gfx->fillRoundRect(SETTINGS_BRIGHTNESS_BAR_X + 2, barTop + 2, fillWidth, SETTINGS_BRIGHTNESS_BAR_H - 4,
                       (SETTINGS_BRIGHTNESS_BAR_H - 4) / 2, UI_BAR_OK);
  #pragma endregion

  drawSettingsCloseButton();
  gfx->flush();
}

void handleAudioVideoSettingsTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnSettingsCloseButton(touchX, touchY)) {  // back to the menu
    saveBrightnessLevelToNvs();
    settingsScreen = SETTINGS_MENU;
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y, SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H)) {
    audioSetEnabled(!audioEnabled());
    if (audioEnabled()) sfxPlay(SFX_TAP);  // confirms when turning on
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_BRIGHTNESS_MINUS_X, SETTINGS_BRIGHTNESS_ROW_Y, CLK_BTN, CLK_BTN)) {
    if (settingsBrightnessLevel > 0) settingsBrightnessLevel--;
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_BRIGHTNESS_PLUS_X, SETTINGS_BRIGHTNESS_ROW_Y, CLK_BTN, CLK_BTN)) {
    if (settingsBrightnessLevel < SETTINGS_BRIGHTNESS_MAX) settingsBrightnessLevel++;
  }
}
#pragma endregion

#pragma region Time settings screen: 12h/24h format
// The choice only changes how this screen shows and steps the hour: clockH is
// always 0..23, so applyClock() is the same in both modes.
void loadClockFormatFromNvs() {
  Preferences preferences;
  preferences.begin("tamapoke", true);  // read-only
  clockUses24Hour = preferences.getBool("clk24h", true);
  preferences.end();
}

void saveClockFormatToNvs() {
  Preferences preferences;
  preferences.begin("tamapoke", false);
  preferences.putBool("clk24h", clockUses24Hour);
  preferences.end();
}

// clockH as a 12-hour dial shows it: 0 -> 12, 13 -> 1
int clockHourOn12HourDial() {
  int hourInHalfDay = clockH % 12;
  return hourInHalfDay == 0 ? 12 : hourInHalfDay;
}

// hour -/+ (hourStep -1 or +1). In 12h mode it cycles 12, 1..11 inside the
// current half of the day: AM/PM only changes with its own toggle.
void stepClockHour(int hourStep) {
  if (clockUses24Hour) {
    clockH = (clockH + 24 + hourStep) % 24;
    return;
  }
  int halfDayOffset = clockH >= 12 ? 12 : 0;
  clockH = (clockH % 12 + 12 + hourStep) % 12 + halfDayOffset;
}

// Two-segment switch on the CLK_MODE row (12h|24h, AM|PM): the selected segment
// is filled. Disabled = track color with nothing selected (AM/PM in 24h mode).
// One button showing the current option; each tap switches to the other one.
// Disabled = grayed out (AM/PM in 24h mode), kept in place so the layout does not jump.
void drawClockToggleButton(int buttonLeft, const char *currentLabel, bool enabled) {
  gfx->fillRoundRect(buttonLeft, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H, CLK_MODE_R, UI_WHITE);
  gfx->drawRoundRect(buttonLeft, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H, CLK_MODE_R, enabled ? UI_INK : UI_TRACK);
  gfx->setTextColor(enabled ? UI_INK : UI_TRACK);
  drawTextCenteredInBox(currentLabel, TS(3), buttonLeft, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H);
}

// HOUR / MIN label centered under its -/+ button pair
void drawClockPairLabel(const char *label, int pairLeft) {
  int pairCenterX = pairLeft + CLK_PAIR_W / 2;
  setSize(2);
  setCur(pairCenterX - (int)textW(label, 2) / 2, CLK_LBL_Y);
  printT(label);
}
#pragma endregion

#pragma region Time settings screen
void renderClock() {
  beginSettingsScreen(T(S_SET_TIME));

  gfx->setTextColor(UI_INK);
  char timeText[8];
  if (clockUses24Hour) snprintf(timeText, sizeof(timeText), "%02d:%02d", clockH, clockM);
  else snprintf(timeText, sizeof(timeText), "%2d:%02d", clockHourOn12HourDial(), clockM);  // same 5 chars: stays centered
  setSize(TS(7));
  setCur(CX - CLK_DIG_HALF, CLK_DIG_Y);
  printT(timeText);

  drawClockBtn(CLK_HM_X, CLK_BTN_Y, "-");  // hour -
  drawClockBtn(CLK_HP_X, CLK_BTN_Y, "+");  // hour +
  drawClockBtn(CLK_MM_X, CLK_BTN_Y, "-");  // min -
  drawClockBtn(CLK_MP_X, CLK_BTN_Y, "+");  // min +
  gfx->setTextColor(UI_LABEL);
  drawClockPairLabel(T(S_HOUR), CLK_HM_X);
  drawClockPairLabel(T(S_MIN), CLK_MM_X);

  #pragma region 12h/24h and AM/PM toggle buttons (language-neutral literals)
  drawClockToggleButton(CLK_FORMAT_X, clockUses24Hour ? "24h" : "12h", true);
  drawClockToggleButton(CLK_AMPM_X, clockH >= 12 ? "PM" : "AM", !clockUses24Hour);
  #pragma endregion

  gfx->fillRoundRect(CLK_OK_X, CLK_OK_Y, CLK_OK_W, CLK_OK_H, CLK_OK_R, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(TS(3));
  setCur(CX - CLK_OK_TX, CLK_OK_TY);
  printT("OK");

  drawSettingsCloseButton();
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (isTouchOnSettingsCloseButton(x, y)) {  // back to the menu WITHOUT applying
    settingsScreen = SETTINGS_MENU;
    return;
  }
  if (y >= CLK_BTN_Y && y <= CLK_BTN_Y + CLK_BTN) {  // row of +/- buttons
    if (x >= CLK_HM_X && x < CLK_HM_X + CLK_BTN) stepClockHour(-1);
    else if (x >= CLK_HP_X && x < CLK_HP_X + CLK_BTN) stepClockHour(1);
    else if (x >= CLK_MM_X && x < CLK_MM_X + CLK_BTN) clockM = (clockM + 59) % 60;
    else if (x >= CLK_MP_X && x < CLK_MP_X + CLK_BTN) clockM = (clockM + 1) % 60;
    return;
  }
  #pragma region 12h/24h toggle (flips and persists) and AM/PM toggle (12h only)
  if (isTouchInsideRect(x, y, CLK_FORMAT_X, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H)) {
    clockUses24Hour = !clockUses24Hour;
    saveClockFormatToNvs();
    return;
  }
  if (!clockUses24Hour && isTouchInsideRect(x, y, CLK_AMPM_X, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H)) {
    clockH = (clockH + 12) % 24;  // same hour on the dial, other half of the day
    return;
  }
  #pragma endregion
  if (y >= CLK_OK_Y && y <= CLK_OK_Y + CLK_OK_H && x >= CLK_OK_X && x <= CLK_OK_X + CLK_OK_W) { applyClock(); return; }
}
#pragma endregion

#pragma region Language settings screen
// One tappable cell per language, 2 columns (row-major, in Lang order). Names
// are ASCII-only so they render under the CP437 font AND the CJK U8g2 fonts.
static const char *const LANGUAGE_NATIVE_NAMES[LANG_COUNT] = {
  "Espanol", "English", "Francais", "Deutsch", "Italiano", "Portugues", "Nihongo", "Hangugeo"
};

int languageCellLeft(int languageIndex) { return SETTINGS_LANG_CELL_X0 + (languageIndex % 2) * SETTINGS_LANG_CELL_COL_STEP; }
int languageCellTop(int languageIndex) { return SETTINGS_LANG_CELL_Y0 + (languageIndex / 2) * SETTINGS_LANG_CELL_ROW_STEP; }

void renderLanguageSettings() {
  beginSettingsScreen(T(S_MENU_LANG));

  #pragma region One text size for every cell: the largest at which all names fit
  int sharedTextSize = TS(3);
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    sharedTextSize = shrinkTextSizeToWidth(LANGUAGE_NATIVE_NAMES[languageIndex], sharedTextSize,
                                           SETTINGS_LANG_CELL_W - 2 * SETTINGS_TEXT_SIDE_PAD);
  }
  #pragma endregion

  #pragma region Cells (the active language is filled)
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    bool isActiveLanguage = (languageIndex == gLang);
    int cellLeft = languageCellLeft(languageIndex), cellTop = languageCellTop(languageIndex);
    gfx->fillRoundRect(cellLeft, cellTop, SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H, SETTINGS_LANG_CELL_R,
                       isActiveLanguage ? UI_BAR_OK : UI_WHITE);
    gfx->drawRoundRect(cellLeft, cellTop, SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H, SETTINGS_LANG_CELL_R, UI_INK);
    gfx->setTextColor(isActiveLanguage ? UI_BG_DAY : UI_INK);
    drawTextCenteredInBox(LANGUAGE_NATIVE_NAMES[languageIndex], sharedTextSize, cellLeft, cellTop,
                          SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H);
  }
  #pragma endregion

  drawSettingsCloseButton();
  gfx->flush();
}

void handleLanguageSettingsTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnSettingsCloseButton(touchX, touchY)) {  // back to the menu
    settingsScreen = SETTINGS_MENU;
    return;
  }
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    if (!isTouchInsideRect(touchX, touchY, languageCellLeft(languageIndex), languageCellTop(languageIndex),
                           SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H)) continue;
    setLang((Lang)languageIndex);
    applyLangFont();  // the font changes with the language
    sfxPlay(SFX_TAP);
    settingsScreen = SETTINGS_MENU;
    return;
  }
}
#pragma endregion

#pragma region Settings dispatch (render and tap for whichever settings screen is open)
void renderSettingsScreen() {
  switch (settingsScreen) {
    case SETTINGS_AUDIO_VIDEO: renderAudioVideoSettings(); break;
    case SETTINGS_TIME: renderClock(); break;
    case SETTINGS_LANGUAGE: renderLanguageSettings(); break;
    default: renderSettingsMenu(); break;
  }
}

void handleSettingsTap(int16_t touchX, int16_t touchY) {
  switch (settingsScreen) {
    case SETTINGS_AUDIO_VIDEO: handleAudioVideoSettingsTap(touchX, touchY); break;
    case SETTINGS_TIME: clockTap(touchX, touchY); break;
    case SETTINGS_LANGUAGE: handleLanguageSettingsTap(touchX, touchY); break;
    default: handleSettingsMenuTap(touchX, touchY); break;
  }
}
#pragma endregion

// flame + streak number at the top right (under the clock and the battery),
// right-justified on STREAK_RIGHT: 22 px for the flame and its gap, then the number
void drawStreakBadge() {
  if (pet.streak < 1) return;
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  int x = STREAK_RIGHT - 22 - (int)textW(s, 2), y = STREAK_Y;
  #pragma region Flame: one teardrop silhouette (circle + tip triangle) with a smaller inner flame
  gfx->fillCircle(x + 8, y + 12, 6, UI_BAR_BAD);
  gfx->fillTriangle(x + 9, y, x + 2, y + 11, x + 14, y + 11, UI_BAR_BAD);
  gfx->fillCircle(x + 8, y + 13, 3, UI_BAR_WARN);
  gfx->fillTriangle(x + 8, y + 7, x + 5, y + 13, x + 11, y + 13, UI_BAR_WARN);
  #pragma endregion
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x + 22, y + 2);
  printT(s);
}

// temporary banner: new medal or streak milestone
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  // 32 and not 20: "%u にちれんぞく！" already exceeds 20 with a single digit, and
  // snprintf cut in the middle of a 3-byte character.
  char buf[32];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1) return;
  gfx->fillRoundRect(CEL_X, CEL_Y, CEL_W, CEL_H, CEL_R, UI_BAR_WARN);
  gfx->drawRoundRect(CEL_X, CEL_Y, CEL_W, CEL_H, CEL_R, UI_INK);
  gfx->setTextColor(UI_INK);
  ctext(l1, 3, CEL_L1Y, CEL_W - 16);
  ctext(l2, 2, CEL_L2Y, CEL_W - 16);
}

// medals on the info card: labeled badge, colored if earned
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  setSize(2);
  setCur(x + (100 - textW(medalLabel(i), 2)) / 2, y + 5);
  printT(medalLabel(i));
}

// pencil next to the card name: as tall as the text line, never below the minimum
int cardPencilSizeForTextSize(int textSize) {
  return 8 * textSize < CARD_PENCIL_MIN_SIZE ? CARD_PENCIL_MIN_SIZE : 8 * textSize;
}

//#region Info card page 0: portrait and stat row helpers (CARD_PROFILE_* in boards/layout.h)
// Height of one text line at the scale setSize() last applied: 8 px per step
// with the classic font, 16 px per step with the CJK unifont (whose scale is
// already the requested size halved, so sizes 2 and up match the classic font).
int cardTextLineHeight() {
  return (gCjkFont ? 16 : 8) * gTextSize;
}

// Smallest box (canvas pixel columns/rows) holding every visible pixel of all the
// idle frames. PMD canvases carry empty margins (and the creature is rarely
// centered inside them), so centering the canvas does not center the creature.
// Measured once per species and cached; reading frames is cheap with a blob and
// one SD read per frame when streaming.
void measureIdleVisibleBounds(int &minColumn, int &maxColumn, int &minRow, int &maxRow) {
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
void drawCardProfilePortrait() {
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

// Creature status text (statusMsg()) centered under the portrait, in one line if
// it fits the column, else split at a space into two, else shrunk. Tries the
// preferred size first and steps down, so a wrapped line is preferred to tiny text.
void drawCardProfileStatusMessage() {
  const int columnWidth = CARD_PROFILE_PORTRAIT_W;
  const int columnCenterX = CARD_PROFILE_PORTRAIT_X + CARD_PROFILE_PORTRAIT_W / 2;
  char workingText[48];
  snprintf(workingText, sizeof(workingText), "%s", statusMsg());
  gfx->setTextColor(UI_INK);

  for (int textSize = CARD_PROFILE_STATUS_SIZE; textSize >= 1; textSize--) {
    setSize(textSize);
    //#region Single line
    if (textW(workingText, textSize) <= columnWidth) {
      setCur(columnCenterX - textW(workingText, textSize) / 2, CARD_PROFILE_STATUS_Y);
      printT(workingText);
      return;
    }
    //#endregion

    //#region Two lines, split at the space that balances them best
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
      workingText[bestSplitIndex] = '\0';
      const char *secondLine = workingText + bestSplitIndex + 1;
      setCur(columnCenterX - textW(workingText, textSize) / 2, CARD_PROFILE_STATUS_Y);
      printT(workingText);
      setCur(columnCenterX - textW(secondLine, textSize) / 2, CARD_PROFILE_STATUS_Y + cardTextLineHeight() + 2);
      printT(secondLine);
      return;
    }
    //#endregion
  }
  //#region Nothing fit even at size 1: draw the single line anyway, centered on the column
  setCur(columnCenterX - textW(workingText, 1) / 2, CARD_PROFILE_STATUS_Y);
  printT(workingText);
  //#endregion
}

// Label of a stat row, left-justified in the stats column and shrunk to its
// width on both boards. Returns the top of the value line under it.
int drawCardProfileRowLabel(int rowTop, const char *label) {
  gfx->setTextColor(UI_LABEL);
  shrinkTextSizeToWidth(label, CARD_PROFILE_LABEL_SIZE, CARD_PROFILE_STATS_W);
  setCur(CARD_PROFILE_STATS_LEFT, rowTop);
  printT(label);
  return rowTop + cardTextLineHeight() + CARD_PROFILE_VALUE_GAP;
}

// Stat row whose value is text, right-justified on the line under the label
void drawCardProfileTextRow(int rowTop, const char *label, const char *value) {
  int valueTop = drawCardProfileRowLabel(rowTop, label);
  gfx->setTextColor(UI_INK);
  int valueSize = shrinkTextSizeToWidth(value, CARD_PROFILE_VALUE_SIZE, CARD_PROFILE_STATS_W);
  setCur(CARD_PROFILE_STATS_RIGHT - textW(value, valueSize), valueTop);
  printT(value);
}

// Stat row whose value is a bar and its number: the number right-justified, the
// bar filling the rest of the line. The number's slot is measured on maxValue,
// so the bar keeps the same length whatever the value.
void drawCardProfileBarRow(int rowTop, const char *label, uint16_t value, uint16_t maxValue, uint16_t barColor) {
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
  //#region Bar track and fill, vertically centered on the value line
  int barWidth = CARD_PROFILE_STATS_W - numberSlotWidth - CARD_PROFILE_BAR_GAP;
  int barTop = valueTop + (lineHeight - CARD_PROFILE_BAR_H) / 2;
  int fillWidth = (int)value * barWidth / maxValue;
  if (fillWidth > barWidth) fillWidth = barWidth;
  gfx->fillRoundRect(CARD_PROFILE_STATS_LEFT, barTop, barWidth, CARD_PROFILE_BAR_H, 3, UI_TRACK);
  if (fillWidth > 2) gfx->fillRoundRect(CARD_PROFILE_STATS_LEFT, barTop, fillWidth, CARD_PROFILE_BAR_H, 3, barColor);
  //#endregion
}
//#endregion

// page 0: profile (name, portrait on the left, stat rows on the right: level,
// streak (best), bond, favorite food, age)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[32];
  // no level here: it has its own stat row below
  snprintf(head, sizeof(head), "%s%s", pet.shiny ? "*" : "", nm);
  gfx->setTextColor(d.accent);
  // auto-shrink: long names do not fit at the large size. The width is MEASURED
  // with textW() (strlen() counts bytes). On the round one: size 3 when the name
  // plus the pencil fit in 198 px, otherwise 2; on the rectangular one the large size is TS(3).
  const int big = TS(3);
  setSize(big);
  int hts = (textW(head, big) + CARD_PENCIL_GAP + cardPencilSizeForTextSize(big) <= CARD_HEAD_MAXW) ? big : big - 1;
  setSize(hts);
  //#region Name and rename pencil, centered together as one group
  int headWidth = textW(head, hts);
  int pencilSize = cardPencilSizeForTextSize(hts);
  int groupLeft = CX - (headWidth + CARD_PENCIL_GAP + pencilSize) / 2;
  int headTop = hts == big ? SY(34) : SY(40);
  setCur(groupLeft, headTop);
  printT(head);
  cardPencilLeft = groupLeft + headWidth + CARD_PENCIL_GAP;
  cardPencilTop = headTop + (8 * hts - pencilSize) / 2;  // centered on the text line
  cardPencilSize = pencilSize;
  drawPencilIcon(gfx, cardPencilLeft, cardPencilTop, cardPencilSize, UI_INK);
  //#endregion
  if (pet.nick[0]) {  // real species under the nickname
    const char *sp = dexName(pet.speciesId);
    char par[32];
    snprintf(par, sizeof(par), "(%s)", sp);
    gfx->setTextColor(UI_TRACK);
    ctext(par, 2, SY(64));   // measured, not counted in bytes
  }

  //#region Animated portrait (left column)
  drawCardProfilePortrait();
  //#endregion

  //#region Status message under the portrait
  drawCardProfileStatusMessage();
  //#endregion

  //#region Stat rows (right column): level, streak (best), bond, favorite food, age
  int rowTop = CARD_PROFILE_ROW_Y0;
  char levelText[8];
  snprintf(levelText, sizeof(levelText), "%u", pet.level());
  drawCardProfileTextRow(rowTop, T(S_CARD_LEVEL), levelText);
  rowTop += CARD_PROFILE_ROW_STEP;

  char streakText[16];  // "65535 (65535)" at most
  snprintf(streakText, sizeof(streakText), "%u (%u)", pet.streak, pet.bestStreak);
  drawCardProfileTextRow(rowTop, T(S_CARD_STREAK_BEST), streakText);
  rowTop += CARD_PROFILE_ROW_STEP;

  drawCardProfileBarRow(rowTop, T(S_CARD_BOND), pet.bond, 100, C565(0xd4, 0x52, 0x7e));
  rowTop += CARD_PROFILE_ROW_STEP;

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  drawCardProfileTextRow(rowTop, T(S_CARD_FAV_FOOD), berry);
  rowTop += CARD_PROFILE_ROW_STEP;

  char ageText[24];
  snprintf(ageText, sizeof(ageText), T(S_CARD_AGE_FMT), (unsigned long)(pet.ageMinutes / 1440));
  drawCardProfileTextRow(rowTop, T(S_CARD_AGE), ageText);
  //#endregion
}

// page 1: battle (4 bars + train button)
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  ctext(T(S_BATTLE), 3, SY(48));

  drawCardStat(SY(118), T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(SY(160), T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(SY(202), T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(SY(244), T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  // button: strength training bag (greyed out while it can't start, e.g. asleep)
  gfx->fillRoundRect(TRAIN_X, TRAIN_Y, TRAIN_W, TRAIN_H, 12, canStartSack() ? UI_BAR_BAD : UI_TRACK);
  gfx->setTextColor(UI_BG_DAY);
  ctext(T(S_TRAIN_STR), 2, TRAIN_Y + TRAIN_TY, TRAIN_W - 8);
}

// page 2: medals with descriptive labels
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[24];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  ctext(head, 3, SY(48));

  for (int i = 0; i < MED_COUNT; i++) {
    int x = MED_X0 + (i % 2) * MED_COLSTEP, y = MED_Y0 + (i / 2) * MED_STEP;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, MED_W, MED_H, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // earned mark
      gfx->fillCircle(x + MED_MARK_CX, y + MED_H / 2, MED_MARK_R, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(x + MED_MARK_CX - 6, y + MED_H / 2 - 9);
      printT("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    int fs = fitSize(medalDesc(i), TS(2), MED_W - MED_TXT_X - 6);
    setSize(fs);
    setCur(x + MED_TXT_X, y + (MED_H - 8 * fs) / 2);
    printT(medalDesc(i));
  }
}

// page 3: progress (level, evolution, care mistakes) - surfaces mechanics that
// used to be invisible (how long until leveling up/evolving and why)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  // large title, shrunk to fit the circle / screen width (long translations)
  int titleSize = shrinkTextSizeToWidth(T(S_PROGRESS), TS(5), CARD_PROGRESS_TITLE_MAXW);
  setCur(centerX(T(S_PROGRESS), titleSize), CARD_PROGRESS_TITLE_Y);
  printT(T(S_PROGRESS));

  // level under the title
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  ctext(lv, 3, CARD_PROGRESS_LEVEL_Y);

  // progress bar to the next level (1 level = 60 min of play)
  uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
  int bx = SX(93), bw = SX(280), by = CARD_PROGRESS_BAR_Y, bh = BAR_H_CARD;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = (bw - 4) * into / MINUTES_PER_LEVEL;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  char nx[32];
  snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - into, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  ctext(nx, 2, by + CARD_PROGRESS_NEXT_GAP, LCD_WIDTH - 2 * CARD_SIDE_MARGIN);  // clear of the page arrows

  // evolution status (these lines sit level with the page arrows)
  gfx->setTextColor(UI_LABEL);
  ctext(T(S_EVO_LABEL), 2, CARD_PROGRESS_EVO_LABEL_Y, LCD_WIDTH - 2 * CARD_SIDE_MARGIN);
  char evoBuf[32];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  ctext(evo, 2, CARD_PROGRESS_EVO_VALUE_Y, LCD_WIDTH - 2 * CARD_SIDE_MARGIN);

  // care mistakes (delay evolution)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  ctext(ms, 2, CARD_PROGRESS_MISTAKES_Y);
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMedals();
  else renderCardProgress();

  //#region Close button (top-right)
  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);
  //#endregion

  //#region Page arrows on the side edges (none past the first or the last page)
  if (cardPage > 0) drawPageArrow(gfx, CARD_ARROW_LEFT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, -1, UI_INK);
  if (cardPage < CARD_PAGE_COUNT - 1) drawPageArrow(gfx, CARD_ARROW_RIGHT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, 1, UI_INK);
  //#endregion

  //#region Page indicator dots at the bottom, centered on CX
  for (int i = 0; i < CARD_PAGE_COUNT; i++) {
    int dotCenterX = CX + (2 * i - (CARD_PAGE_COUNT - 1)) * CARD_DOT_STEP / 2;
    if (i == cardPage) gfx->fillCircle(dotCenterX, CARD_DOTS_Y, 5, UI_INK);
    else gfx->drawCircle(dotCenterX, CARD_DOTS_Y, 4, UI_INK);
  }
  //#endregion
  gfx->flush();
}

// taps on the info card: close button, rename pencil (page 0), page arrows and
// the TRAIN STRENGTH button (page 1). Anywhere else does nothing.
void handleInfoCardTap(int16_t x, int16_t y) {
  //#region Close button, drawn circle (wins over the pencil's padded box)
  if (isTouchInsideCircle(x, y, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R)) {
    cardOpen = false;
    return;
  }
  //#endregion

  //#region Rename pencil next to the name (page 0)
  if (cardPage == 0 && cardPencilSize > 0 &&
      isTouchInsideRect(x, y, cardPencilLeft - CARD_PENCIL_HIT_PAD, cardPencilTop - CARD_PENCIL_HIT_PAD,
                        cardPencilSize + 2 * CARD_PENCIL_HIT_PAD, cardPencilSize + 2 * CARD_PENCIL_HIT_PAD)) {
    openKeyboard();
    return;
  }
  //#endregion

  //#region Close button, generous finger radius
  if (isTouchInsideCircle(x, y, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R)) {
    cardOpen = false;
    return;
  }
  //#endregion

  //#region Page arrows (a hidden arrow ignores taps)
  const int16_t arrowHitTop = CARD_ARROW_CY - CARD_ARROW_HIT_H / 2;
  if (cardPage > 0 && isTouchInsideRect(x, y, 0, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    cardPage--;
    return;
  }
  if (cardPage < CARD_PAGE_COUNT - 1 &&
      isTouchInsideRect(x, y, LCD_WIDTH - CARD_ARROW_HIT_W, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    cardPage++;
    return;
  }
  //#endregion

  //#region TRAIN STRENGTH button (page 1)
  // the card only closes when the bag can actually open (not while asleep)
  if (cardPage == 1 && y >= TRAIN_Y && y <= TRAIN_Y + TRAIN_H && x >= TRAIN_X && x <= TRAIN_X + TRAIN_W &&
      canStartSack()) {
    cardOpen = false;
    startSack();
  }
  //#endregion
}

// ---------- rename keyboard ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
// KB_X/Y/W/H and the rest: boards/layout.h

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  ctext(T(S_NAME), 2, KB_TITLE_Y);
  // current buffer
  gfx->fillRoundRect(KB_BOX_X, KB_BOX_Y, KB_BOX_W, KB_BOX_H, KB_BOX_R, UI_WHITE);
  gfx->drawRoundRect(KB_BOX_X, KB_BOX_Y, KB_BOX_W, KB_BOX_H, KB_BOX_R, UI_INK);
  setSize(TS(3));
  setCur(KB_BOX_TX, KB_BOX_TY);
  printT(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    setSize(2);
    if (i < 28) {
      setCur(x + KB_W / 2 - KB_CH_DX, y + KB_H / 2 - KB_CH_DY);
      printT(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      setCur(x + KB_W / 2 - KB_OK_DX, y + KB_H / 2 - KB_CH_DY);
      printT(lab);
    }
  }
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  // filter BEFORE dividing: with x/y below KB_X/KB_Y the subtraction is negative
  // and integer division truncates toward zero (not toward -inf), so col/row
  // would come out 0 instead of negative and the filter below would not catch it
  if (x < KB_X || y < KB_Y) return;
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col >= KB_COLS || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  if (i == 28) {  // delete
    if (nameLen) nameBuf[--nameLen] = 0;
  } else if (i == 29) {  // OK
    pet.rename(nameBuf);
    kbOpen = false;
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
  }
}

// ---------- pokedex gallery ----------

// The 4x4 grid takes 393x404 px on 466x466. GAL_CELL uses the SMALLER of the
// X/Y scale factors so the whole grid always fits on screen.
// The round board's grid is smaller (GALLERY_CELL in boards/layout.h) so it clears
// the close button; the thumbnails are drawn at GALLERY_THUMB_SCALE_NUM/DEN.
#define GAL_CELL GALLERY_CELL
#define GAL_X ((LCD_WIDTH - 4 * GAL_CELL) / 2)
#define GAL_Y GALLERY_GRID_Y

void drawGalleryNavigation(bool showLeftArrow, bool showRightArrow);  // defined after renderGallery

// draws a thumbnail centered in its cell; sil=true paints it in ink
void drawThumbCell(const uint8_t *b, int x, int y, int s, bool sil, int cell) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
#if !BOARD_IS_ROUND
  while (s > 1 && (w * s > cell || h * s > cell)) s--;  // make it fit in the cell
#endif
  int ox = x + (cell - w * s) / 2;
  int oy = y + (cell - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

// Thumbnail centered in a grid cell at the rational scale
// GALLERY_THUMB_SCALE_NUM/DEN (pixel boundaries rounded, so no gaps between pixels)
void drawGalleryGridThumb(const uint8_t *b, int cellX, int cellY, bool sil) {
  uint8_t thumbWidth = b[0], thumbHeight = b[1], paletteCount = b[2];
  const uint8_t *palette = b + 3;
  const uint8_t *pixels = palette + paletteCount * 2;
  int scaledWidth = thumbWidth * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
  int scaledHeight = thumbHeight * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
  int originX = cellX + (GAL_CELL - scaledWidth) / 2;
  int originY = cellY + (GAL_CELL - scaledHeight) / 2;
  for (int row = 0; row < thumbHeight; row++) {
    int top = row * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
    int bottom = (row + 1) * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
    for (int column = 0; column < thumbWidth; column++) {
      uint8_t paletteIndex = pixels[row * thumbWidth + column];
      if (paletteIndex == 0xFF) continue;
      int left = column * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
      int right = (column + 1) * GALLERY_THUMB_SCALE_NUM / GALLERY_THUMB_SCALE_DEN;
      uint16_t color = sil ? INK_K : (uint16_t)(palette[paletteIndex * 2] | (palette[paletteIndex * 2 + 1] << 8));
      gfx->fillRect(originX + left, originY + top, right - left, bottom - top, color);
    }
  }
}

void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  drawThumbCell(b, x, y, s, sil, GAL_CELL);
}

void renderGallery() {
  if (galleryDetail) {  // detail view: always redrawn (animated)
    gfx->fillScreen(RGB565_BLACK);
    fillScreenBg(UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    // known = raised or caught in a wild battle: color, name and animation
    bool reg = pet.isRegistered(galleryDetail) || pet.isCaught(galleryDetail);
    char head[32];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? dexName(galleryDetail) : "???");
    gfx->setTextColor(reg ? d.accent : UI_INK);
    // auto-shrinks long names (they do not fit at size 3), measuring instead of
    // counting bytes; 13 characters at size 3 are the 234 px from before.
    setSize(3);
    int gts = (textW(head, 3) <= GALLERY_DETAIL_HEADER_MAXW) ? 3 : 2;
    setSize(gts);
    setCur(centerX(head, gts), gts == 3 ? SY(56) : SY(60));
    printT(head);
    #pragma region Type chips (shown for every species, known or not)
    int typesWidth = typeChipsTotalWidth(d, GALLERY_TYPE_CHIP_STYLE);
    drawTypeChips(CX - typesWidth / 2, GALLERY_DETAIL_TYPES_Y, d, false, GALLERY_TYPE_CHIP_STYLE);
    #pragma endregion
    if (galleryPmd.loaded) {
      // animated and in color if registered; static silhouette if not ("?" style)
      drawPmdActM(galleryPmd, PMD_IDLE, CX, SY(300), reg ? millis() : 0, true, !reg, 6);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL / 2, SY(135), 4, !reg);
    }
    #pragma region Caught mark (species caught in a wild battle)
    if (pet.isCaught(galleryDetail)) {
      gfx->setTextColor(UI_BAR_OK);
      ctext(T(S_CAUGHT_MARK), 2, SY(392), LCD_WIDTH - 2 * CARD_ARROW_HIT_W);
    }
    #pragma endregion
    drawGalleryNavigation(galleryDetail > 1, galleryDetail < 151);
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // the grid is static
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  fillScreenBg(UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), SY(36));
  printT(head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > 151) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        bool raised = pet.isRegistered(dex);
        bool caught = pet.isCaught(dex);
        drawGalleryGridThumb(t, x, y, !raised && !caught);  // known = raised or caught
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          setSize(2);
          setCur(x + GAL_CELL * 62 / 80, y + GAL_CELL * 4 / 80);  // offsets tuned for an 80 px cell
          printT("*");
        }
        #pragma region Caught marker C: caught in a wild battle but never raised (bottom left of the cell)
        if (caught && !raised) {
          gfx->setTextColor(UI_BAR_OK);
          setSize(BOARD_IS_ROUND ? 2 : 1);
          setCur(x + GAL_CELL * 4 / 80, y + GAL_CELL * 58 / 80);
          printT("C");
        }
        #pragma endregion
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_TRACK);
        setSize(2);
        setCur(x + GAL_CELL * 24 / 80, y + GAL_CELL * 32 / 80);
        printT(num);
      }
    }
  }
  // page dots: centered (the original's fixed x=170 only fit at 466 px)
  int dotsX0 = CX - (9 * SX(14)) / 2;
  for (int i = 0; i < 10; i++) {
    if (i == galleryPage) gfx->fillCircle(dotsX0 + i * SX(14), SY(436), 4, UI_INK);
    else gfx->drawCircle(dotsX0 + i * SX(14), SY(436), 3, UI_INK);
  }
  drawGalleryNavigation(galleryPage > 0, galleryPage < 9);
  gfx->flush();
}

#pragma region Gallery navigation buttons (close X and page/species arrows)
// Shows the species `step` positions away (+1 next, -1 previous) in the detail
// view, if there is one: any dex 1..151 can open detail (same rule as a grid tap).
void galleryShowNeighborSpecies(int step) {
  int16_t neighborDex = galleryDetail + step;
  if (neighborDex < 1 || neighborDex > 151) return;
  galleryDetail = neighborDex;
  galleryPage = (neighborDex - 1) / 16;  // the grid shows this species' page when going back
  galleryPmd.load(neighborDex, pet.isShinyRegistered(neighborDex));
}

// Opens the gallery on its first page (Pokedex button on the main screen)
void openGallery() {
  galleryOpen = true;
  galleryPage = 0;
  galleryDetail = 0;
  galleryDirty = true;
}

// Closes the gallery and returns to the main screen it is opened from
void closeGalleryToMainScreen() {
  galleryOpen = false;
  galleryDetail = 0;
  galleryPmd.unload();
}

// Left/right arrows (same style and hit boxes as the info card) plus the round X.
// Arrows are hidden when there is no further page/species in that direction.
void drawGalleryNavigation(bool showLeftArrow, bool showRightArrow) {
  drawCloseButton(gfx, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_R, UI_INK, UI_BG_DAY);
  if (showLeftArrow) drawPageArrow(gfx, CARD_ARROW_LEFT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, -1, UI_INK);
  if (showRightArrow) drawPageArrow(gfx, CARD_ARROW_RIGHT_CX, CARD_ARROW_CY, CARD_ARROW_SIZE, 1, UI_INK);
}

// Returns true when the tap hit the X (closeHit = true) or a visible arrow
// (step = -1 left / +1 right).
bool galleryNavigationHit(int16_t x, int16_t y, bool leftArrowVisible, bool rightArrowVisible, int &step, bool &closeHit) {
  step = 0;
  closeHit = isTouchInsideCircle(x, y, CLOSE_BTN_CX, CLOSE_BTN_CY, CLOSE_BTN_HIT_R);
  if (closeHit) return true;
  const int16_t arrowHitTop = CARD_ARROW_CY - CARD_ARROW_HIT_H / 2;
  if (leftArrowVisible && isTouchInsideRect(x, y, 0, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    step = -1;
    return true;
  }
  if (rightArrowVisible && isTouchInsideRect(x, y, LCD_WIDTH - CARD_ARROW_HIT_W, arrowHitTop, CARD_ARROW_HIT_W, CARD_ARROW_HIT_H)) {
    step = 1;
    return true;
  }
  return false;
}
#pragma endregion

void galleryTap(int16_t x, int16_t y) {
  int navigationStep;
  bool closeHit;
  if (galleryDetail) {
    // Only the X returns to the grid; the arrows switch species; other taps do nothing
    if (!galleryNavigationHit(x, y, galleryDetail > 1, galleryDetail < 151, navigationStep, closeHit)) return;
    if (!closeHit) {
      galleryShowNeighborSpecies(navigationStep);
      return;
    }
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (galleryNavigationHit(x, y, galleryPage > 0, galleryPage < 9, navigationStep, closeHit)) {
    if (closeHit) {
      closeGalleryToMainScreen();
    } else {
      galleryPage += navigationStep;
      galleryDirty = true;
    }
    return;
  }
  // same reason as in the keyboard: filter before dividing
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c > 3 || r > 3) return;
  int16_t dex = galleryPage * 16 + r * 4 + c + 1;
  if (dex > 151) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
}

void drawBattery() {
  int pc = batPercent();
  if (pc < 0) return;  // no battery connected
  int x = BAT_X, y = BAT_Y, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // terminal
  if (charging) {
    // charging bolt (zigzag) instead of the level bar
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

#pragma region Header
// Name centered on CX at the top; HDR_NAME_MAX_W leaves room for the hamburger,
// the streak badge, the clock and the battery.
void drawHeaderName(const char *name, uint16_t nameColor) {
  gfx->setTextColor(nameColor);
  ctext(name, 3, HDR_NAME_Y, HDR_NAME_MAX_W);
  //#region Remember where the name landed (tapping it opens the info card)
  // ctext() left the size it actually used set; textW() measures with it
  headerNameWidth = textW(name, gTextSize);
  headerNameLeft = CX - headerNameWidth / 2;
  //#endregion
}

// Egg and ceremony views: name plus a message line under it
void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  drawHeaderName(name, nameColor);
  headerLevelRowWidth = 0;  // no level row here: its tap box is off
  gfx->setTextColor(inkColor());
  ctext(msg, 2, HDR_MSG_Y);
}

// Main pet view: name plus "Lv.N" and a progress bar toward the next level
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
  // the row keeps its 16 px line (size 2 height); the smaller text is centered on it
  setCur(rowLeft, HDR_MSG_Y + (16 - 8 * HDR_LVL_TEXT_SIZE) / 2);
  printT(levelText);
  int barLeft = rowLeft + levelTextWidth + HDR_LVL_GAP;
  int barTop = HDR_MSG_Y + (16 - HDR_LVL_BAR_H) / 2;  // 16 = text height at size 2
  uint32_t minutesIntoLevel = pet.ageMinutes % MINUTES_PER_LEVEL;
  int fillWidth = (HDR_LVL_BAR_W - 4) * (int)minutesIntoLevel / MINUTES_PER_LEVEL;
  gfx->fillRoundRect(barLeft, barTop, HDR_LVL_BAR_W, HDR_LVL_BAR_H, 4, UI_TRACK);
  if (fillWidth > 0) gfx->fillRoundRect(barLeft + 2, barTop + 2, fillWidth, HDR_LVL_BAR_H - 4, 2, UI_BAR_OK);
  #pragma endregion
}

// Menu button at the top left (main pet view and egg view)
void drawHamburgerButton() {
  drawHamburgerIcon(gfx, HAMBURGER_X, HAMBURGER_Y, HAMBURGER_W, HAMBURGER_H, inkColor());
}

// Pokedex button under the hamburger (main pet view and egg view): opens the gallery
void drawPokedexButton() {
  drawMap(SPR_ICON_POKEDEX, 16, POKEDEX_BTN_X, POKEDEX_BTN_Y, POKEDEX_BTN_S, false);
}

// Current local time at the top right, in the format chosen on the time screen.
// Source: pet.lastSeenEpoch, the same cache sceneHour() reads (0 = no clock yet).
void drawHeaderClock() {
  uint32_t epochSeconds = pet.lastSeenEpoch;
  if (!epochSeconds) return;
  int hour = (int)((epochSeconds / 3600) % 24);
  int minute = (int)((epochSeconds / 60) % 60);
  char clockText[8];
  const char *meridiemText = "";  // "AM"/"PM" in 12h mode, one size smaller so the clock stays narrow
  if (clockUses24Hour) {
    snprintf(clockText, sizeof(clockText), "%02d:%02d", hour, minute);
  } else {
    int hourOn12HourDial = hour % 12 == 0 ? 12 : hour % 12;
    snprintf(clockText, sizeof(clockText), "%d:%02d", hourOn12HourDial, minute);
    meridiemText = hour >= 12 ? "PM" : "AM";
  }
  const int meridiemSize = HDR_TIME_SIZE - 1;
  int meridiemWidth = meridiemText[0] ? HDR_TIME_MERIDIEM_GAP + (int)textW(meridiemText, meridiemSize) : 0;
  int clockLeft = HDR_TIME_RIGHT - meridiemWidth - (int)textW(clockText, HDR_TIME_SIZE);
  gfx->setTextColor(inkColor());
  setSize(HDR_TIME_SIZE);
  setCur(clockLeft, HDR_TIME_Y);
  printT(clockText);
  if (meridiemText[0]) {  // bottom-aligned with the digits (8 px per text size step)
    setSize(meridiemSize);
    setCur(HDR_TIME_RIGHT - (int)textW(meridiemText, meridiemSize), HDR_TIME_Y + 8 * (HDR_TIME_SIZE - meridiemSize));
    printT(meridiemText);
  }
}
#pragma endregion

// ceremony animation (10s): farewell = a bow with hearts and it walks away;
// runaway = it gets scared and runs off. Replaces the idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // fallback if there is no PMD sprite
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 over the 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // sad ending: bluish gloom + rain
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % LCD_WIDTH;
      int ry = (i * 91 + now / 2) % (LCD_HEIGHT + 4);
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // head down, trembling
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // walks away slowly and fades out
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // flickers toward the silhouette
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silhouette: it blurs as it leaves
    // tear falling from the creature
    if (t < 0.55f) {
      int ty = y - SY(150) + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // epic farewell: pulsing golden halo + rising sparks and hearts
  int gcy = PET_GROUND - SY(96);
  for (int k = 0; k < 4; k++) {
    int r = SY(60) + k * SY(34) + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % LCD_WIDTH;
    int py = SY(410) - (int)((now / 8 + i * 70) % SY(360));   // rise and reappear at the bottom
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // little heart
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // bow / farewell pose
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // walks off to the right
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // big heart following the creature
    drawMap(SPR_HEART, 32, x + SX(50), y - SY(190), PET_HEART_SCALE, false);
}

// decision dialog (2 stacked buttons): evolve/keep or say goodbye/stay together
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolution
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else {                // farewell
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  const int b1y = CH_Y + CH_B1_REL_Y, b2y = CH_Y + CH_B2_REL_Y;
  gfx->fillRoundRect(CH_X, CH_Y, CH_W, CH_H, CH_R, UI_WHITE);
  gfx->drawRoundRect(CH_X, CH_Y, CH_W, CH_H, CH_R, UI_INK);
  gfx->setTextColor(UI_INK);
  ctext(q, 2, CH_Y + CH_QY, CH_W - 16);
  gfx->fillRoundRect(CH_BX, b1y, CH_BW, CH_BH, 12, c1);     // action button
  gfx->setTextColor(t1);
  ctext(o1, 2, b1y + CH_BTY, CH_BW - 12);
  gfx->fillRoundRect(CH_BX, b2y, CH_BW, CH_BH, 12, c2);     // keep/stay together button
  gfx->setTextColor(t2);
  ctext(o2, 2, b2y + CH_BTY, CH_BW - 12);
}

// big red CTA button to evolve (pulses to draw attention)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // beats: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, EVO_R, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, EVO_R, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, EVO_R - 2, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  const char *t = T(S_EVO_TAP);
  int fs = fitSize(t, TS(3), EVO_BTN_W - 16);
  setSize(fs);
  setCur(centerX(t, fs), y + h / 2 - (fs == 3 ? 11 : 4 * fs));
  printT(t);
}

// gold farewell CTA button: "<name> wants to tell you something..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, FAR_R, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, FAR_R, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  int fs = fitSize(buf, TS(2), FAR_BTN_W - 16);
  setSize(fs);
  setCur(centerX(buf, fs), y + h / 2 - 4 * fs);
  printT(buf);
}

// somber runaway-from-neglect CTA button: "<name> feels abandoned..."
// (sad ending: dark blue-gray, slow and muted pulse)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, FAR_R, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, FAR_R, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  int fs = fitSize(buf, TS(2), FAR_BTN_W - 16);
  setSize(fs);
  setCur(centerX(buf, fs), y + h / 2 - 4 * fs);
  printT(buf);
}

// epic evolution animation: radial halo + spinning rays + accelerating sprite
// flashing + sparks shooting out + final flash
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // radial halo that grows and pulses
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // spinning rays from the creature's center
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // flashing between the PREVIOUS and the NEW form (silhouettes), accelerating;
  // at the end (t>0.9) it stays on the new one for the reveal flash
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // sparks shooting out
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // final flash before revealing the new form
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // no SD and no flash sprite: clear notice that sprites are missing
    gfx->setTextColor(inkColor());
    setSize(6);
    setCur(CX - 18, PET_CY - 80);
    printT("?");
    setSize(2);
    const char *l1 = T(S_NO_SPRITES);
    setCur(centerX(l1, 2), PET_CY - 4);
    printT(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    setCur(centerX(l2, 2), PET_CY + 20);
    printT(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // evolution animation: alternates the silhouette of the previous and the new form
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // little hop

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // overlaid expressions using the species' anchors
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- bath scene ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (!timeLeft(bathUntil)) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // happy pose when clean
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // foam: bubbles swaying and slowly rising
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // the bubbles pop: sparkles
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- PMD pet: behavior ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  // guard: never more iterations than frames, in case a corrupt .bin has ms[]=0
  // despite the sanitizing on load (defense in depth)
  for (uint8_t guard = 0; guard < a.frames && t >= a.ms[i]; guard++) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// draws an action anchored at its base (center-x, ground) and returns its scale
// draws an action of a specific PmdMon (m); drawPmdAct uses the global pmd
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActResizedM(m, actId, cx, groundY, t, loop, sil, maxS, 100);
}

// same as drawPmdActM, then resized by sizePercent (100 = the usual whole-number zoom)
void drawPmdActResizedM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                        uint16_t sizePercent) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? SY(170) / m.acts[PMD_IDLE].h : 5;
  const uint8_t minS = BOARD_IS_ROUND ? 2 : 1;  // at 320x240 a x2 zoom is already half the screen
  if (sBase < minS) sBase = minS;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > minS && a.h * s > SY(250)) s--;  // actions with a large frame (attack)
  // anchor by the feet (a.base), not by the canvas height: that way actions
  // with different padding (Hurt, Eat...) all sit at the same ground height
  uint16_t scalePercent = (uint16_t)s * sizePercent;
  int x0 = cx - a.w * scalePercent / 200, y0 = groundY - (a.base ? a.base : a.h) * scalePercent / 100;
  drawPmdFrameAtScalePercent(m, actId, x0, y0, t, loop, sil, scalePercent);
}

//#region Draw the current frame of an action, canvas top-left at (canvasLeft, canvasTop), at a fixed scale
void drawPmdFrameAtScale(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop, bool sil, uint8_t scale) {
  drawPmdFrameAtScalePercent(m, actId, canvasLeft, canvasTop, t, loop, sil, (uint16_t)scale * 100);
}

// scalePercent 250 = x2.5: source pixel c covers screen x from scaledPixelEdge(c) up to scaledPixelEdge(c + 1),
// so a fractional zoom still tiles with no gaps
int scaledPixelEdge(int origin, int pixelIndex, uint16_t scalePercent) {
  return origin + pixelIndex * scalePercent / 100;
}

void drawPmdFrameAtScalePercent(PmdMon &m, uint8_t actId, int canvasLeft, int canvasTop, uint32_t t, bool loop, bool sil,
                                uint16_t scalePercent) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = m.frame(actId, fi);  // blob: direct pointer; streaming: reads 1 frame from the SD
  lastSpriteDrawnBounds.valid = false;
  if (!fr) return;
  int opaqueColumnMin = a.w, opaqueColumnMax = -1, opaqueRowMin = a.h, opaqueRowMax = -1;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    int rowTop = scaledPixelEdge(canvasTop, r, scalePercent);
    int rowHeight = scaledPixelEdge(canvasTop, r + 1, scalePercent) - rowTop;
    if (rowHeight <= 0) continue;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      int columnLeft = scaledPixelEdge(canvasLeft, c, scalePercent);
      int columnWidth = scaledPixelEdge(canvasLeft, c + 1, scalePercent) - columnLeft;
      if (columnWidth > 0) gfx->fillRect(columnLeft, rowTop, columnWidth, rowHeight, sil ? INK_K : m.pal[idx]);
      if (c < opaqueColumnMin) opaqueColumnMin = c;
      if (c > opaqueColumnMax) opaqueColumnMax = c;
      if (r < opaqueRowMin) opaqueRowMin = r;
      opaqueRowMax = r;
    }
  }
  #pragma region Remember the opaque pixel box on screen (coin chase pickup)
  if (opaqueColumnMax >= 0) {
    lastSpriteDrawnBounds = { (int16_t)scaledPixelEdge(canvasLeft, opaqueColumnMin, scalePercent),
                              (int16_t)scaledPixelEdge(canvasTop, opaqueRowMin, scalePercent),
                              (int16_t)(scaledPixelEdge(canvasLeft, opaqueColumnMax + 1, scalePercent) - 1),
                              (int16_t)(scaledPixelEdge(canvasTop, opaqueRowMax + 1, scalePercent) - 1), true };
  }
  #pragma endregion
}
//#endregion
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// picks the creature's next whim when it is happy
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // walk
    beh.targetX = SX(150) + random(SX(176));  // SX(): unscaled, the CYD walked half off its 320 px screen
    beh.until = now + 15000;
  } else if (r < 60) {
    // random gesture among the available ones
    // (Hop excluded: jumps too high; Sit excluded: faces backwards)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // face forward
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // evolution finished: free the previous form

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // happy: the scheduler decides (idle / walk / gesture)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  // SX/SY: unscaled, the CYD drew it at y -34 and only its tip showed at the top edge
  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + SX(50), PET_GROUND - SY(190), PET_HEART_SCALE, false);
}

// animated sprite from the SD: integer per-pixel zoom, frames at their own pace
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // little hop
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emotes instead of expressions (imported sprites have no anchors)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// closed eye: erases the 3x4 eye and draws the eyelid
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// erases the base smile and draws an open mouth (eating) or a frown (sad)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, POOP_X0 + i * POOP_DX, POOP_Y, POOP_S, false);
  }
}

void drawBars() {
  drawBar(NEEDS_LEFT_X, NEEDS_ROW1_Y, T(S_BAR_FOOD), pet.fullness);
  drawBar(NEEDS_RIGHT_X, NEEDS_ROW1_Y, T(S_BAR_JOY), pet.joy);
  drawBar(NEEDS_LEFT_X, NEEDS_ROW2_Y, T(S_BAR_ENE), pet.energy);
  drawBar(NEEDS_RIGHT_X, NEEDS_ROW2_Y, T(S_BAR_HYG), pet.hygiene);
}

// Gap between the label and its bar in the needs row. It was fixed at 48 px,
// exactly what 4 Latin letters take at scale 2, with no margin. Japanese
// labels are wider (ごきげん is 4 kana, ~64 px) and ran into the bar. It is
// derived from the longest label, with 48 as a floor so nothing changes in
// the Latin-script languages.
// Requires text size 2 to already be set, because textW() needs it.
static int barLabelGap() {
  const StrId ids[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  // below the original threshold it stays EXACTLY 48, so nothing moves in the
  // Latin-script languages; above it, space is added so the label can breathe
  return ancho <= 48 ? 48 : ancho + 8;
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x, y);
  printT(label);
  int gap = barLabelGap();
  // the two columns start at NEEDS_LEFT_X / NEEDS_RIGHT_X (boards/layout.h): the
  // first one's bar has to end NEEDS_COLUMN_SPACING before the second one's
  // label, and both are drawn the same width
  int bw = (NEEDS_RIGHT_X - NEEDS_LEFT_X) - gap - NEEDS_COLUMN_SPACING;
  if (bw > NEEDS_BAR_W_MAX) bw = NEEDS_BAR_W_MAX;
  if (bw < NEEDS_BAR_W_MIN) bw = NEEDS_BAR_W_MIN;  // floor: keep the bar from disappearing
  const int bh = NEEDS_BAR_H;
  int bx = x + gap;
  int by = y + (16 - bh) / 2;  // 16 = text height at size 2: the bar is centered on its label
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, by, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 3, fill);
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // while asleep only LIGHT works
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16 + buttons[i].iconOffsetX, buttons[i].cy - 16 + buttons[i].iconOffsetY, 2, false);
  }
}

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
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// draws a scaled n x n pixel map; silhouette=true paints it in ink
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}

// Definitions of the game-wide state declared in app_state.h, with the initial
// values the sketch has always started with.

#pragma region Includes
#include "app_state.h"
#include "species.h"  // SPR_ICON_* for the main screen buttons
#pragma endregion

#pragma region Static screen redraw
bool screenDirty = true;  // the first frame of any screen is always drawn
uint32_t lastStaticScreenDrawMs = 0;

bool consumeStaticScreenRedraw(uint32_t nowMs) {
  if (!screenDirty && nowMs - lastStaticScreenDrawMs < STATIC_SCREEN_REFRESH_MS) return false;
  screenDirty = false;
  lastStaticScreenDrawMs = nowMs;
  return true;
}
#pragma endregion

#pragma region Text and night mode
// always false until applyLangFont() picks a CJK font
bool gCjkFont = false;
uint8_t gTextSize = 1;
int gFontAscent = 0;
bool gNight = false;

bool isNightHour(int hour) { return hour < 6 || hour >= 20; }
#pragma endregion

#pragma region Creature and its sprites
Pet pet;
SdMon mon;
PmdMon pmd;
PmdMon evoPmd;
PmdMon galleryPmd;
int16_t monFor = -2;
bool monShinyFor = false;
PetBehavior beh;
SpriteDrawnBounds lastSpriteDrawnBounds = {};
#pragma endregion

#pragma region Pokedex gallery
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;
int16_t galleryDetail = 0;
GalleryFilter galleryFilter = GALLERY_FILTER_ALL;
int16_t galleryFilteredDex[DEX_COUNT];
int16_t galleryFilteredCount = 0;
bool galleryFilterDirty = true;
#pragma endregion

#pragma region Screens and overlays
bool screenOff = false;
bool cardOpen = false;
bool kbOpen = false;
uint8_t cardPage = 0;
uint32_t feedMenuUntil = 0;
uint32_t playMenuUntil = 0;
#pragma endregion

#pragma region Keyboard state
KeyboardPurpose keyboardPurpose = KEYBOARD_FOR_NAME;
char keyboardText[WIFI_PASSWORD_MAX_LENGTH + 1] = "";
uint8_t keyboardTextLength = 0;
uint8_t keyboardTextMaxLength = 0;
bool keyboardShiftOn = false;
bool keyboardSymbolsOn = false;
#pragma endregion

#pragma region Hit boxes recorded while drawing
int16_t headerNameLeft = 0, headerNameWidth = 0;
int16_t headerLevelRowLeft = 0, headerLevelRowWidth = 0;
int16_t cardPencilLeft = 0, cardPencilTop = 0;
int16_t cardPencilSize = 0;
#pragma endregion

#pragma region Settings screens state
SettingsScreen settingsScreen = SETTINGS_CLOSED;
uint8_t wifiListPage = 0;
char wifiPasswordTargetSsid[WIFI_SSID_MAX_LENGTH + 1] = "";
int clockH = 12, clockM = 0;
bool clockUses24Hour = true;
uint8_t settingsBrightnessLevel = SETTINGS_BRIGHTNESS_DEFAULT;
uint8_t settingsBrightnessSavedLevel = SETTINGS_BRIGHTNESS_DEFAULT;
#pragma endregion

#pragma region Bath scene
uint32_t bathUntil = 0;
bool bathPending = false;
#pragma endregion

#pragma region Ball game state
bool gameOpen = false;
bool gameOver = false;
uint8_t gameAttackGain = 0;
float ballX, ballY, ballVX, ballVY, gamePetX;
uint32_t lastGameStep = 0;
uint8_t gameScore, gameMisses;
float hitX, hitY;
uint32_t hitTime = 0;
uint32_t ballLastHitAt = 0;
bool gameNewHi = false;
#pragma endregion

#pragma region Coin chase minigame state
bool coinOpen = false;
bool coinOver = false;
float coinX, coinY;
uint32_t coinSpawnedAt = 0;
float coinPetX, coinPetFeetY;
float coinTargetX, coinTargetFeetY;
bool coinPetFacingLeft = false;
int32_t coinTimeLeftMs = 0;
uint32_t lastCoinStep = 0;
uint8_t coinPoints = 0;
uint8_t coinSpeedGain = 0;
bool coinNewHi = false;
#pragma endregion

#pragma region Training bag state
bool sackOpen = false;
uint32_t sackUntil = 0;
bool sackOver = false;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;
#pragma endregion

#pragma region Wild battle state
bool battleOpen = false;
bool battleResolved = false;
int16_t battleDex = 0;
uint8_t battleLevel = 1;
bool battleShiny = false;
BattleStats battlePlayer = {};
BattleStats battleEnemy = {};
BattleRuntime battleRun = {};
BattleTurnResult battleTurn = {};
BattleReward battleReward = {};
bool battleFromWildEncounter = false;
// battlePlayerMoveText / battleWildMoveText are defined in battle_ui.cpp, next to their snprintf
bool battleAttackMenuOpen = false;
bool battleCatchOffered = false;
bool battleCatchTried = false;
bool battleCatchDone = false;
bool battleCatchSuccess = false;
bool battleRespectCatch = false;
uint8_t battleCatchChance = 0;
bool battleLowHpWarned = false;
#pragma endregion

#pragma region Wild encounter prompt state
uint32_t wildPromptUntil = 0;
uint32_t nextWildEligible = 0;
uint32_t lastWildCheck = 0;
int16_t wildPromptDex = 0;
uint8_t wildPromptLevel = 1;
bool wildPromptShiny = false;
#pragma endregion

#pragma region Main screen action buttons
// SPR_ICON_CONTROLLER (16x16) fills columns 0..15 and rows 3..12: its visual
// center matches the cell center, so it needs no optical offset.
#if BOARD_IS_ROUND
Btn buttons[MAIN_BUTTON_COUNT] = {
  { 140, 390, SPR_ICON_FOOD },        // eat
  { 202, 404, SPR_ICON_CONTROLLER },  // play (opens the play menu)
  { 264, 404, SPR_ICON_LIGHT },       // light
  { 326, 390, SPR_ICON_CLEAN },       // bath
};
#else
Btn buttons[MAIN_BUTTON_COUNT] = {
  { (int16_t)(LCD_WIDTH * 1 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_FOOD },
  { (int16_t)(LCD_WIDTH * 3 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_CONTROLLER },
  { (int16_t)(LCD_WIDTH * 5 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_LIGHT },
  { (int16_t)(LCD_WIDTH * 7 / 8), (int16_t)(LCD_HEIGHT - 22), SPR_ICON_CLEAN },
};
#endif
#pragma endregion

#pragma region Picker item counts match the layout tables
static_assert(FEED_PICK_COUNT == FEED_ITEM_COUNT, "FeedPickerItem must list every food picker item");
static_assert(PLAY_PICK_COUNT == PLAY_MENU_ITEM_COUNT, "PlayPickerItem must list every play picker item");
#pragma endregion

#pragma region Starter choice
const int16_t STARTER_DEX[STARTER_COUNT] = { 1, 4, 7 };
#pragma endregion

#pragma region Touch gesture and dialog state
bool wasPressed = false;
uint32_t lastRender = 0;
uint32_t lastInteract = 0;
uint8_t dimStage = 0;
bool swallowGesture = false;
uint32_t holdStart = 0;
uint32_t confirmUntil = 0;
uint8_t choiceKind = CHOICE_NONE;
uint32_t choiceUntil = 0;
int16_t tX0, tY0, tXl, tYl;
uint32_t tStart = 0;
bool holdFired = false;
#pragma endregion

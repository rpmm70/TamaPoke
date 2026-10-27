// render() dispatcher, the main screen with its overlays and the starter choice
// (see main_screen.h).

#pragma region Includes
#include "main_screen.h"
#include "app_state.h"         // pet, screen flags, gNight, inkColor, STARTER_DEX, markScreenDirty, ...
#include "bath.h"              // drawBath
#include "battle_ui.h"         // renderBattle, drawWildPrompt
#include "board_display.h"     // gfx, clearScreenToDayBackground
#include "boards/layout.h"     // CX, SY(), BOTTOM_PANEL_Y, STARTER_*, FEED_*, PLAY_MENU_*, CONF_*
#include "ceremony_fx.h"       // drawCeremony, drawChoiceDialog, drawEvolveButton, drawFarewellButton, drawRunawayButton
#include "colors.h"            // lerp565, WATER_BLUE_COLOR, UI_* palette, SPRITE_OUTLINE_COLOR
#include "countof.h"
#include "dex.h"               // DEX_TBL, dexName, R_RARE, R_LEGENDARY
#include "gallery.h"           // renderGallery
#include "hud.h"               // drawHeader, drawHeaderWithLevel, drawHamburgerButton, drawPokedexButton, ...
#include "i18n.h"              // T()
#include "info_card.h"         // renderCard
#include "keyboard.h"          // renderKeyboard
#include "minigame_bag.h"      // renderSack
#include "minigame_ball.h"     // renderGame
#include "minigame_coin.h"     // renderCoinGame
#include "pet_render.h"        // drawPet, drawPetHeart, drawSleepBubble
#include "scene.h"             // sceneHour, drawScene
#include "settings_screens.h"  // renderSettingsScreen
#include "species.h"           // SPR_EGG, SPR_ICON_*, SPRITE_H
#include "sprite_draw.h"       // drawMap, drawThumbCell
#include "ui_text.h"           // ctext, setSize, setCur, fitSize, printT
#include "ui_widgets.h"        // drawBorderedBox, drawLabeledButton, drawTextCenteredOnCx
#if BOARD_HAS_WIFI
#include "wifi_manager.h"      // wifiIsScanning, wifiConnectionState, wifiScanResultCount, wifiIsEnabled
#endif
#pragma endregion

#pragma region Layout constants
// the flash icons of the food and play pickers are 16x16 pixel maps
static const int PICKER_ICON_PIXELS = 16;
// egg drawn at a fixed zoom (also on the CYD)
static const int EGG_SPRITE_SCALE = 5;
// rare / legendary egg label and the "POKEDEX n/151" line on the bottom panel
#define EGG_RARITY_LABEL_Y SY(316)
#define EGG_POKEDEX_COUNT_Y SY(348)
// starter name: right padding inside its row
static const int STARTER_NAME_RIGHT_PAD = 6;
// Yes / No buttons of the release dialog
static const int CONFIRM_BUTTON_RADIUS = 12;
#pragma endregion

#pragma region Egg cracks
// pixels of the egg sprite painted in the outline color as it cracks
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
#pragma endregion

#pragma region Picker icons
// in FeedPickerItem / PlayPickerItem order
static const char *const *const FEED_PICKER_ICONS[] = {
  SPR_ICON_APPLE, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G, SPR_ICON_CANDY, SPR_ICON_COFFEE,
};
static const char *const *const PLAY_PICKER_ICONS[] = {
  SPR_ICON_PLAY, SPR_ICON_COIN, SPR_ICON_GLOVE, SPR_ICON_SWORDS,
};
static_assert(countof(FEED_PICKER_ICONS) == FEED_PICK_COUNT && FEED_PICK_COUNT == FEED_ITEM_COUNT,
              "one food picker icon per item");
static_assert(countof(PLAY_PICKER_ICONS) == PLAY_PICK_COUNT && PLAY_PICK_COUNT == PLAY_MENU_ITEM_COUNT,
              "one play picker icon per item");
#pragma endregion

#pragma region Static screen redraw
// what is open, as one number: a change marks the screen dirty so the first
// frame of a static screen (and of a new page or sub-screen) is always drawn
enum OpenScreen : uint8_t {
  OPEN_SCREEN_STARTER_SELECT = 1,
  OPEN_SCREEN_GALLERY,
  OPEN_SCREEN_BALL_GAME,
  OPEN_SCREEN_COIN_CHASE,
  OPEN_SCREEN_BATTLE,
  OPEN_SCREEN_TRAINING_BAG,
  OPEN_SCREEN_KEYBOARD,
  OPEN_SCREEN_SETTINGS,
  OPEN_SCREEN_INFO_CARD,
  OPEN_SCREEN_MAIN
};

static uint32_t currentOpenScreenKey() {
  if (pet.awaitingStarter() && settingsScreen == SETTINGS_CLOSED) return OPEN_SCREEN_STARTER_SELECT;
  if (galleryOpen) return OPEN_SCREEN_GALLERY;
  if (gameOpen) return OPEN_SCREEN_BALL_GAME;
  if (coinOpen) return OPEN_SCREEN_COIN_CHASE;
  if (battleOpen) return ((uint32_t)OPEN_SCREEN_BATTLE << 24) | ((uint32_t)(uint16_t)battleDex << 8) | (battleResolved ? 1 : 0);
  if (sackOpen) return OPEN_SCREEN_TRAINING_BAG;
  if (kbOpen) return ((uint32_t)OPEN_SCREEN_KEYBOARD << 24) | keyboardPurpose;
  if (settingsScreen != SETTINGS_CLOSED) return ((uint32_t)OPEN_SCREEN_SETTINGS << 24) | settingsScreen;
  if (cardOpen) return ((uint32_t)OPEN_SCREEN_INFO_CARD << 24) | cardPage;
  return OPEN_SCREEN_MAIN;
}

// a battle is static only when nothing on it moves: both creatures are
// thumbnails (no PMD sprite) and it is daytime (night stars twinkle)
static bool battleScreenIsAnimated() {
  return pmd.loaded || galleryPmd.loaded;  // the battle arena itself never animates
}

#if BOARD_HAS_WIFI
// what the WiFi screen shows on its own (not from a tap), as one number
static uint32_t wifiDisplayStateKey() {
  return ((uint32_t)wifiIsEnabled() << 24) | ((uint32_t)wifiIsScanning() << 16) |
         ((uint32_t)wifiConnectionState() << 8) | wifiScanResultCount();
}
#endif

// the WiFi screen keeps redrawing while a scan or a connection is in progress
static bool settingsScreenIsAnimated() {
#if BOARD_HAS_WIFI
  if (settingsScreen != SETTINGS_WIFI) return false;
  #pragma region Repaint as soon as the WiFi state changes
  static uint32_t previousWifiDisplayStateKey = 0;
  uint32_t wifiStateKey = wifiDisplayStateKey();
  if (wifiStateKey != previousWifiDisplayStateKey) {
    previousWifiDisplayStateKey = wifiStateKey;
    markScreenDirty();
  }
  #pragma endregion
  return wifiIsScanning() || wifiConnectionState() == WIFI_CONNECTION_CONNECTING;
#else
  return false;
#endif
}
#pragma endregion

#pragma region Internal helpers
// clean base for the bars and buttons (and the egg texts) over the landscape:
// lower third of the screen (y=312..466 of 466)
static void drawBottomPanel() {
  gfx->fillRect(0, BOTTOM_PANEL_Y, LCD_WIDTH, LCD_HEIGHT - BOTTOM_PANEL_Y, gNight ? UI_BG_NIGHT : UI_BG_DAY);
}

// egg crack pixels (sprite coordinates) at the egg sprite scale
static void drawCracks(const uint8_t (*crackPixels)[2], size_t crackPixelCount, int spriteLeft, int spriteTop, int scale) {
  for (size_t i = 0; i < crackPixelCount; i++)
    gfx->fillRect(spriteLeft + crackPixels[i][0] * scale, spriteTop + crackPixels[i][1] * scale, scale, scale,
                  SPRITE_OUTLINE_COLOR);
}

// white strip with an ink outline and iconCount 16x16 icons, one per itemWidth slot
static void drawIconStrip(int stripLeft, int stripTop, int stripWidth, int stripHeight, int radius, int itemWidth,
                          int iconScale, const char *const *const *icons, int iconCount) {
  drawBorderedBox(stripLeft, stripTop, stripWidth, stripHeight, radius, UI_WHITE, inkColor());
  const int iconLeft = stripLeft + (itemWidth - PICKER_ICON_PIXELS * iconScale) / 2;
  const int iconTop = stripTop + (stripHeight - PICKER_ICON_PIXELS * iconScale) / 2;
  for (int i = 0; i < iconCount; i++)
    drawMap(icons[i], PICKER_ICON_PIXELS, iconLeft + i * itemWidth, iconTop, iconScale, false);
}

// first game: pick a starter among Bulbasaur / Charmander / Squirtle
static void renderStarterSelect() {
  clearScreenToDayBackground();
  gfx->setTextColor(UI_INK);
  ctext(T(S_CHOOSE_STARTER), 2, STARTER_TITLE_Y);
  for (int i = 0; i < (int)countof(STARTER_DEX); i++) {
    int16_t starterDex = STARTER_DEX[i];
    const DexEntry &starterEntry = DEX_TBL[starterDex];
    int rowTop = starterRowTop(i);
    drawBorderedBox(STARTER_ROW_X, rowTop, STARTER_ROW_W, STARTER_ROW_H, STARTER_R,
                    lerp565(starterEntry.accent, UI_WHITE, 6, 8), starterEntry.accent);
    const uint8_t *starterThumb = thumbs.get(starterDex);  // starter thumbnail (if the SD is ready)
    if (starterThumb)
      drawThumbCell(starterThumb, STARTER_TH_X, rowTop + STARTER_TH_DY, STARTER_TH_S, false, STARTER_TH_CELL);
    gfx->setTextColor(UI_INK);
    int nameLeft = STARTER_ROW_X + STARTER_NAME_DX;
    int nameSize = fitSize(dexName(starterDex), TS(3), STARTER_ROW_X + STARTER_ROW_W - nameLeft - STARTER_NAME_RIGHT_PAD);
    setSize(nameSize);
    setCur(nameLeft, rowTop + STARTER_NAME_DY);
    printT(dexName(starterDex));
  }
  gfx->flush();
}

// egg view: header, egg sprite with its cracks, rarity hint and Pokedex count
static void renderEggView() {
  drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
  drawHamburgerButton();
  drawPokedexButton();
  drawHeaderClock();
  const int scale = EGG_SPRITE_SCALE;
  const int eggLeft = CX - (SPRITE_H / 2) * scale, eggTop = PET_CY - (SPRITE_H / 2) * scale;
  drawMap(SPR_EGG, SPRITE_H, eggLeft, eggTop, scale, false);
  if (pet.eggCracks() >= 1) drawCracks(CRACK1, countof(CRACK1), eggLeft, eggTop, scale);
  if (pet.eggCracks() >= 2) drawCracks(CRACK2, countof(CRACK2), eggLeft, eggTop, scale);
  if (pet.eggRarity() >= R_RARE) {
    const char *rarityLabel = (pet.eggRarity() == R_LEGENDARY) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
    gfx->setTextColor(pet.eggRarity() == R_LEGENDARY ? UI_BAR_WARN : WATER_BLUE_COLOR);
    drawTextCenteredOnCx(rarityLabel, 2, EGG_RARITY_LABEL_Y);
  }
  char pokedexCountText[24];
  snprintf(pokedexCountText, sizeof(pokedexCountText), T(S_POKEDEX_FMT), pet.registeredCount());
  drawBottomPanel();
  gfx->setTextColor(inkColor());
  drawTextCenteredOnCx(pokedexCountText, 2, EGG_POKEDEX_COUNT_Y);
}

// creature view: header with level, the creature and its effects, needs and buttons
static void renderCreatureView() {
  const DexEntry &speciesEntry = DEX_TBL[pet.speciesId];
  char headerName[28];
  const char *baseName = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(headerName, sizeof(headerName), "%s%s", pet.shiny ? "*" : "", baseName);  // the level has its own line now
  drawHeaderWithLevel(headerName, gNight ? UI_INK_NIGHT : speciesEntry.accent);
  drawHamburgerButton();
  drawPokedexButton();
  drawHeaderClock();
  drawStreakBadge();
  drawPet();
  // right after drawPet: both are anchored on the sprite box it just drew
  if (pet.showHeart()) drawPetHeart();
  if (pet.sleeping) drawSleepBubble();
  drawBath();
  drawPoops();
  drawBottomPanel();
  drawBars();
  drawButtons();
  drawCelebration();
  if (pet.wantEvolveButton()) drawEvolveButton();           // red CTA: evolve
  else if (pet.canRunawayNow()) drawRunawayButton();        // somber CTA: runaway (neglect)
  else if (pet.wantFarewellButton()) drawFarewellButton();  // gold CTA: farewell
}

// "release?" dialog (long press on the creature)
static void drawReleaseDialog() {
  drawBorderedBox(CONF_X, CONF_Y, CONF_W, CONF_H, CONF_R, UI_WHITE, UI_INK);
  // 48 and not 28: the buffer was sized when every string was one byte
  // per character. With a UTF-8 row, the Korean question plus the name goes
  // over 28 and snprintf cuts in the middle of a 3-byte sequence, which
  // the font can no longer draw.
  char releaseQuestion[48];
  snprintf(releaseQuestion, sizeof(releaseQuestion), T(S_RELEASE_FMT), dexName(pet.speciesId));
  gfx->setTextColor(UI_INK);
  ctext(releaseQuestion, 2, CONF_Y + CONF_QY, CONF_W - 16);
  const int buttonTop = CONF_Y + CONF_BTN_REL_Y;
  const int yesLeft = CONF_X + CONF_BTN_MARGIN;
  const int noLeft = CONF_X + CONF_W - CONF_BTN_MARGIN - CONF_BTN_W;
  drawLabeledButton(yesLeft, buttonTop, CONF_BTN_W, CONF_BTN_H, CONFIRM_BUTTON_RADIUS, UI_BAR_OK, UI_WHITE, T(S_YES), 2);
  drawLabeledButton(noLeft, buttonTop, CONF_BTN_W, CONF_BTN_H, CONFIRM_BUTTON_RADIUS, UI_BAR_BAD, UI_WHITE, T(S_NO), 2);
}
#pragma endregion

#pragma region Public API
int starterRowTop(int starterIndex) {
  return STARTER_ROW_Y + starterIndex * (STARTER_ROW_H + STARTER_ROW_GAP);
}

void render() {
  const uint32_t nowMs = millis();

  #pragma region A different screen opened: its first frame is always drawn
  static uint32_t previousOpenScreenKey = 0;
  uint32_t openScreenKey = currentOpenScreenKey();
  if (openScreenKey != previousOpenScreenKey) {
    previousOpenScreenKey = openScreenKey;
    markScreenDirty();
  }
  #pragma endregion

  #pragma region Full screens (static ones redraw only when dirty or on the safety refresh)
  if (pet.awaitingStarter() && settingsScreen == SETTINGS_CLOSED) {  // first game: pick a starter once the time is set
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
    if (!battleScreenIsAnimated() && !consumeStaticScreenRedraw(nowMs)) return;
    renderBattle();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (kbOpen) {
    if (!consumeStaticScreenRedraw(nowMs)) return;
    renderKeyboard();
    return;
  }
  if (settingsScreen != SETTINGS_CLOSED) {
    if (!settingsScreenIsAnimated() && !consumeStaticScreenRedraw(nowMs)) return;
    renderSettingsScreen();
    return;
  }
  if (cardOpen) {
    // page 0 has the animated portrait; the other pages are static
    if (cardPage != 0 && !consumeStaticScreenRedraw(nowMs)) return;
    renderCard();
    return;
  }
  #pragma endregion

  #pragma region Landscape
  // by the clock only: a sleeping creature keeps the current sky (the panel still dims, see updateBrightness)
  gNight = isNightHour(sceneHour());
  // drawScene covers the full screen: no prior fillScreen(BLACK), so an
  // overlapping DMA flush never captures half-drawn black (anti-flicker).
  // The main and egg views cover the ground below BOTTOM_PANEL_Y with their
  // panel; the ceremony shows the full ground.
  drawScene(pet.isEgg() ? BIOME_MEADOW : DEX_TBL[pet.speciesId].biome, millis(), gNight,
            pet.ceremony ? LCD_HEIGHT : BOTTOM_PANEL_Y);
  #pragma endregion

  #pragma region Ceremony (farewell, runaway, release)
  if (pet.ceremony) {
    const DexEntry &speciesEntry = DEX_TBL[pet.speciesId];
    const char *ceremonyMessage = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                                  : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                                  : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), speciesEntry.accent, ceremonyMessage);
    drawCeremony();
    gfx->flush();
    return;
  }
  #pragma endregion

  if (pet.isEgg()) renderEggView();
  else renderCreatureView();

  #pragma region Food picker
  if (feedMenuUntil) {
    if (!timeLeft(feedMenuUntil)) {
      feedMenuUntil = 0;
    } else {
      drawIconStrip(FEED_X, FEED_Y, FEED_W, FEED_H, FEED_R, FEED_ITEM, FEED_ICON_S,
                    FEED_PICKER_ICONS, (int)countof(FEED_PICKER_ICONS));
    }
  }
  #pragma endregion

  #pragma region Play picker: ball (minigame), coin (coin chase), boxing glove (training bag) and crossed swords (wild battle)
  if (playMenuUntil) {
    if (!timeLeft(playMenuUntil)) {
      playMenuUntil = 0;
    } else {
      drawIconStrip(PLAY_MENU_X, PLAY_MENU_Y, PLAY_MENU_W, PLAY_MENU_H, PLAY_MENU_R, PLAY_MENU_ITEM, PLAY_MENU_ICON_S,
                    PLAY_PICKER_ICONS, (int)countof(PLAY_PICKER_ICONS));
    }
  }
  #pragma endregion

  #pragma region Wild encounter prompt (closes on its own after WILD_PROMPT_MS)
  if (wildPromptUntil) {
    if (!timeLeft(wildPromptUntil)) wildPromptUntil = 0;
    else drawWildPrompt();
  }
  #pragma endregion

  #pragma region Release dialog
  if (confirmUntil) {
    if (!timeLeft(confirmUntil)) confirmUntil = 0;
    else drawReleaseDialog();
  }
  #pragma endregion

  #pragma region Decision dialog (evolve or keep, say goodbye or stay together)
  if (choiceKind) {
    if (!timeLeft(choiceUntil)) choiceKind = CHOICE_NONE;
    else drawChoiceDialog();
  }
  #pragma endregion

  gfx->flush();
}
#pragma endregion

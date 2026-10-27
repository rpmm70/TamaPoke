// Touch input: gesture detection and routing to the open screen (see touch_input.h).

#pragma region Includes
#include "touch_input.h"
#include "app_state.h"         // pet, beh, gesture state, open screen flags, buttons, STARTER_DEX
#include "audio.h"             // sfxPlay
#include "bath.h"              // startBath
#include "battle_ui.h"         // battleTap, startBattle, startBattleWith
#include "boards/layout.h"     // SX()/SY(), STARTER_*, CH_*, CONF_*, FEED_*, PLAY_MENU_*, EVO_BTN_*, FAR_BTN_*, ...
#include "countof.h"
#include "gallery.h"           // galleryTap, rebuildGalleryFilteredList, galleryPageCount, openGallery, closeGalleryToMainScreen
#include "info_card.h"         // handleInfoCardTap
#include "keyboard.h"          // keyboardTap
#include "main_screen.h"       // starterRowTop
#include "minigame_bag.h"      // sackTap, startSack
#include "minigame_ball.h"     // gameTap, startGame
#include "minigame_coin.h"     // coinGameTap, startCoinGame
#include "minigame_common.h"   // isMinigameOkTapped
#include "settings_screens.h"  // handleSettingsTap, openSettingsMenu
#include "touch_hal.h"         // touchHalHasEvent, touchHalGetPoint
#include "ui_widgets.h"        // isTouchInsideRect, isTouchInsideCircle, isTouchOnScreenCloseButton
#pragma endregion

#pragma region Gesture thresholds and timeouts
// 50 Hz is plenty for a finger
static const uint32_t TOUCH_POLL_INTERVAL_MS = 20;
// long press without moving on the creature opens the release dialog
static const uint32_t LONG_PRESS_RELEASE_MS = 3000;
static const int LONG_PRESS_MAX_DRIFT_PX = 30;  // not scaled per board
static const uint32_t RELEASE_DIALOG_TIMEOUT_MS = 10000;
// swipe: mostly horizontal and quick
#define SWIPE_MIN_DX SX(80)
#define SWIPE_MAX_DY SY(70)
static const uint32_t SWIPE_MAX_MS = 800;
// tap: short and nearly still
static const uint32_t TAP_MAX_MS = 1500;
static const int TAP_MAX_MOVE_PX = 40;  // not scaled per board
// evolve / farewell decision dialog and the food / play pickers close on their own
static const uint32_t CHOICE_DIALOG_TIMEOUT_MS = 12000;
static const uint32_t PICKER_TIMEOUT_MS = 6000;
// the level row under the header name: hit box height (the size-1 level text is
// shorter; the generous box is kept from the original)
static const int HEADER_LEVEL_ROW_HIT_H = 16;
#pragma endregion

#pragma region Pet zone
// The box follows the creature as it walks (beh.x), so it can be caressed
// anywhere it wanders; SX(123) is the half-width the fixed box used to have.
#define PET_ZONE_HALF_WIDTH SX(123)
#define PET_ZONE_TOP SY(95)
#define PET_ZONE_BOTTOM SY(310)
#pragma endregion

#pragma region Internal helpers
static void onSwipe(int direction);
static void onTap(int16_t x, int16_t y);

static bool inPetZone(int16_t x, int16_t y) {
  return abs(x - (int)beh.x) < PET_ZONE_HALF_WIDTH && y > PET_ZONE_TOP && y < PET_ZONE_BOTTOM;
}

// item under x in a strip of itemCount icons itemWidth px wide starting at stripLeft
// (the exact right edge gives one past the last: clamped to the last item)
static int iconStripItemAt(int16_t x, int stripLeft, int itemWidth, int itemCount) {
  int item = (x - stripLeft) / itemWidth;
  if (item > itemCount - 1) item = itemCount - 1;
  return item;
}

// swipe: direction +1 = to the right. Only acts inside the info card and the gallery.
static void onSwipe(int direction) {
  if (pet.awaitingStarter()) return;  // blocked during the starter choice
  if (gameOpen || coinOpen || battleOpen || kbOpen || settingsScreen != SETTINGS_CLOSED) return;
  if (cardOpen) {  // inside the card: switch between the pages
    int requestedPage = (int)cardPage + (direction > 0 ? -1 : 1);  // left advances
    cardPage = requestedPage < 0 ? 0 : (requestedPage > CARD_PAGE_COUNT - 1 ? CARD_PAGE_COUNT - 1 : requestedPage);
    return;
  }
  if (!galleryOpen) return;  // main screen: no swipe action
  if (galleryDetail) return;  // in detail: only the X goes back to the grid
  int nextPage = galleryPage - direction;  // swiping left advances a page
  if (nextPage < 0) {                      // going back from the first = exit
    closeGalleryToMainScreen();
    return;
  }
  rebuildGalleryFilteredList();
  if (nextPage > galleryPageCount() - 1) nextPage = galleryPageCount() - 1;
  if (nextPage != galleryPage) {
    galleryPage = nextPage;
    galleryDirty = true;
  }
}

static void onTap(int16_t x, int16_t y) {
  #pragma region First game: pick a starter (after the first-boot time screen closes)
  if (pet.awaitingStarter() && settingsScreen == SETTINGS_CLOSED) {
    for (int i = 0; i < (int)countof(STARTER_DEX); i++) {
      if (isTouchInsideRect(x, y, STARTER_ROW_X, starterRowTop(i), STARTER_ROW_W + 1, STARTER_ROW_H + 1)) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  #pragma endregion

  #pragma region Full screens own their taps
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
  #pragma endregion

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

  #pragma region Decision dialog: action button (top) / keep (bottom)
  if (choiceKind) {
    bool actionButtonHit = isTouchInsideRect(x, y, CH_BX, CH_Y + CH_B1_REL_Y, CH_BW + 1, CH_BH + 1);
    bool keepButtonHit = isTouchInsideRect(x, y, CH_BX, CH_Y + CH_B2_REL_Y, CH_BW + 1, CH_BH + 1);  // keep / stay together
    if (choiceKind == CHOICE_EVOLVE) {
      if (actionButtonHit) { int16_t previousSpeciesId = pet.speciesId; pet.evolve(); evoPmd.load(previousSpeciesId, pet.shiny); }
      else if (keepButtonHit) pet.declineEvolve();
    } else if (choiceKind == CHOICE_FAREWELL) {
      if (actionButtonHit) pet.startFarewell();
      else if (keepButtonHit) pet.declineFarewell();
    }
    choiceKind = CHOICE_NONE;
    return;
  }
  #pragma endregion

  #pragma region Release dialog: YES releases, any other tap closes it
  if (confirmUntil) {
    if (timeLeft(confirmUntil) &&
        isTouchInsideRect(x, y, CONF_X + CONF_BTN_MARGIN, CONF_Y + CONF_BTN_REL_Y, CONF_BTN_W + 1, CONF_BTN_H + 1)) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  #pragma endregion

  #pragma region Food picker
  if (feedMenuUntil) {
    if (timeLeft(feedMenuUntil) && isTouchInsideRect(x, y, FEED_X, FEED_Y, FEED_W + 1, FEED_H + 1)) {
      int item = iconStripItemAt(x, FEED_X, FEED_ITEM, FEED_ITEM_COUNT);
      if (item == FEED_PICK_COFFEE) pet.feedCoffee();
      else if (item == FEED_PICK_CANDY) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
    return;
  }
  #pragma endregion

  #pragma region Play picker: ball game, coin chase, training bag, wild battle; any other tap closes it
  if (playMenuUntil) {
    bool insideStrip = isTouchInsideRect(x, y, PLAY_MENU_X, PLAY_MENU_Y, PLAY_MENU_W, PLAY_MENU_H);
    bool pickerStillOpen = timeLeft(playMenuUntil) > 0;
    playMenuUntil = 0;
    if (pickerStillOpen && insideStrip) {
      int item = iconStripItemAt(x, PLAY_MENU_X, PLAY_MENU_ITEM, PLAY_MENU_ITEM_COUNT);
      sfxPlay(SFX_TAP);
      if (item == PLAY_PICK_BALL_GAME) startGame();
      else if (item == PLAY_PICK_COIN_CHASE) startCoinGame();
      else if (item == PLAY_PICK_TRAINING_BAG) startSack();
      else startBattle();
    }
    return;
  }
  #pragma endregion

  #pragma region Tapping the Pokedex button (under the hamburger) opens the gallery
  // Checked before the hamburger: their hit circles overlap a little, and a tap
  // in that strip is closer to the Pokedex icon.
  if (isTouchInsideCircle(x, y, POKEDEX_BTN_X + 8 * POKEDEX_BTN_S, POKEDEX_BTN_Y + 8 * POKEDEX_BTN_S, POKEDEX_BTN_HIT_R)) {
    sfxPlay(SFX_TAP);
    openGallery();
    return;
  }
  #pragma endregion

  #pragma region Tapping the hamburger button (top left) opens the settings menu
  // Reached only with no dialog open: starter choice, ceremony, choice, release
  // and food picker all returned above. Checked before the egg tap and the name box.
  if (isTouchInsideCircle(x, y, HAMBURGER_X + HAMBURGER_W / 2, HAMBURGER_Y + HAMBURGER_H / 2, HAMBURGER_HIT_R)) {
    sfxPlay(SFX_TAP);
    openSettingsMenu();
    return;
  }
  #pragma endregion

  #pragma region Egg tap
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  #pragma endregion

  #pragma region Tapping the name of the creature (header) opens the info card
  // Same blockers the old swipe-up had: starter choice, ceremony, egg, dialogs
  // and overlays all returned above. The box spans from a little above the name
  // down to the level line, which stays above the pet zone and every button.
  bool onHeaderName = headerNameWidth > 0 &&
      isTouchInsideRect(x, y, headerNameLeft - HDR_NAME_HIT_PAD, HDR_NAME_Y - HDR_NAME_HIT_PAD,
                        headerNameWidth + 2 * HDR_NAME_HIT_PAD, HDR_MSG_Y - (HDR_NAME_Y - HDR_NAME_HIT_PAD));
  bool onHeaderLevelRow = headerLevelRowWidth > 0 &&
      isTouchInsideRect(x, y, headerLevelRowLeft - HDR_NAME_HIT_PAD, HDR_MSG_Y,
                        headerLevelRowWidth + 2 * HDR_NAME_HIT_PAD, HEADER_LEVEL_ROW_HIT_H + HDR_NAME_HIT_PAD);
  if (onHeaderName || onHeaderLevelRow) {
    cardOpen = true;
    cardPage = 0;
    return;
  }
  #pragma endregion

  #pragma region Evolution button: opens the evolve or keep dialog
  if (pet.wantEvolveButton() && isTouchInsideRect(x, y, EVO_BTN_X, EVO_BTN_Y, EVO_BTN_W + 1, EVO_BTN_H + 1)) {
    choiceKind = CHOICE_EVOLVE; choiceUntil = millis() + CHOICE_DIALOG_TIMEOUT_MS;
    return;
  }
  #pragma endregion

  #pragma region Ending buttons (same box): runaway is immediate, farewell opens a dialog
  if (isTouchInsideRect(x, y, FAR_BTN_X, FAR_BTN_Y, FAR_BTN_W + 1, FAR_BTN_H + 1)) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = CHOICE_FAREWELL; choiceUntil = millis() + CHOICE_DIALOG_TIMEOUT_MS; return; }
  }
  #pragma endregion

  #pragma region Main action buttons: food, play, light, clean
  for (int i = 0; i < (int)countof(buttons); i++) {
    int distanceX = x - buttons[i].cx, distanceY = y - buttons[i].cy;
    if (distanceX * distanceX + distanceY * distanceY <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      sfxPlay(SFX_TAP);
      if (i == MAIN_BUTTON_FOOD) {
        if (!pet.sleeping) feedMenuUntil = millis() + PICKER_TIMEOUT_MS;
      } else if (i == MAIN_BUTTON_PLAY) {
        if (!pet.sleeping) playMenuUntil = millis() + PICKER_TIMEOUT_MS;  // picker: ball game / coin chase / training bag / wild battle
      } else if (i == MAIN_BUTTON_LIGHT) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  #pragma endregion

  #pragma region Touching the creature = caress
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) sfxPlay(SFX_HEART);
  }
  #pragma endregion
}
#pragma endregion

#pragma region Public API
// the touch is resolved when the finger LIFTS, to tell a tap from a swipe
void handleTouch() {
  #pragma region Poll the panel
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < TOUCH_POLL_INTERVAL_MS) return;
  lastPoll = millis();
  // we only touch the bus if the chip signaled on INT or if the finger is still
  // down (the lift has to be detected). Reading the sleeping CST9217 hung ~1s
  // and froze the whole loop; SensorLib does not honor the Wire timeout.
  // (the INT gating and the CST9217 anti-hang "address-only cycle" now live
  // in touch_hal_waveshare.cpp, with their original comments)
  if (!touchHalHasEvent() && !wasPressed) return;
  int16_t x, y;
  bool pressed = touchHalGetPoint(&x, &y);
  #pragma endregion

#if BOARD_TOUCH_XPT2046
  #pragma region Screen off with the resistive panel: touch is ignored, only the BOOT button turns it back on
  // (the resistive panel also reads stray presses, which must not light the screen)
  if (screenOff) {
    wasPressed = pressed;
    return;
  }
  #pragma endregion
#endif

  // every touch event (press, move, lift, bag tap) may change what a static screen shows
  markScreenDirty();

  #pragma region Training bag: each touch counts instantly (fast mashing)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (sackOver) {
        if (isMinigameOkTapped(x, y)) {
          sackOpen = false;
          sfxPlay(SFX_TAP);
        }
      } else if (isTouchOnScreenCloseButton(x, y)) {
        sackOpen = false;  // X = quit with no reward
      } else {
        sackTap();
      }
    }
    wasPressed = pressed;
    return;
  }
  #pragma endregion

  #pragma region Gesture start, hold and lift
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
        settingsScreen == SETTINGS_CLOSED && millis() - tStart > LONG_PRESS_RELEASE_MS &&
        abs(tXl - tX0) < LONG_PRESS_MAX_DRIFT_PX && abs(tYl - tY0) < LONG_PRESS_MAX_DRIFT_PX && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + RELEASE_DIALOG_TIMEOUT_MS;
      holdFired = true;
    }
  } else if (wasPressed) {  // finger lifted: resolve gesture
    lastInteract = millis();
    int gestureDx = tXl - tX0, gestureDy = tYl - tY0;
    uint32_t gestureDurationMs = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(gestureDx) > SWIPE_MIN_DX && abs(gestureDy) < SWIPE_MAX_DY && gestureDurationMs < SWIPE_MAX_MS)
        onSwipe(gestureDx > 0 ? 1 : -1);
      else if (gestureDurationMs < TAP_MAX_MS && abs(gestureDx) < TAP_MAX_MOVE_PX && abs(gestureDy) < TAP_MAX_MOVE_PX)
        onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
  #pragma endregion
}
#pragma endregion

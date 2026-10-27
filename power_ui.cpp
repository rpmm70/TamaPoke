// Screen power: brightness level, AMOLED protection dimming, screen off/on and
// the saves that only run while idle (see power_ui.h).

#pragma region Includes
#include "power_ui.h"
#include "app_state.h"      // pet, dimStage, lastInteract, screenOff, the open screen flags
#include "board_display.h"  // boardSetBrightness
#include "rtcbat.h"         // pwrShortPressed, rtcPersistIfDue
#include "settings_screens.h"  // saveClockFormatToNvs
#pragma endregion

#pragma region Constants
// inactivity before the first dim step and before the panel is nearly off
static const uint32_t DIM_AFTER_IDLE_MS = 90000;
static const uint32_t NEARLY_OFF_AFTER_IDLE_MS = 300000;
// panel brightness targets (never above the user's own level)
static const uint8_t SLEEP_DIM_BRIGHTNESS = 25;          // creature asleep on the main screen
static const uint8_t IDLE_DIM_SLEEPING_BRIGHTNESS = 10;  // first dim step while asleep
static const uint8_t IDLE_DIM_BRIGHTNESS = 60;           // first dim step while awake
static const uint8_t NEARLY_OFF_BRIGHTNESS = 8;          // second dim step
// the power key is polled this often
static const uint32_t POWER_KEY_POLL_MS = 250;
#pragma endregion

#pragma region Public API
uint8_t brightnessForSettingsLevel() {
  return SCREEN_BRIGHTNESS_MIN +
         (SCREEN_BRIGHTNESS_MAX - SCREEN_BRIGHTNESS_MIN) * settingsBrightnessLevel / SETTINGS_BRIGHTNESS_MAX;
}

bool isMainScreenShowing() {
  return !pet.awaitingStarter() && !galleryOpen && !gameOpen && !coinOpen && !battleOpen && !sackOpen && !kbOpen &&
         settingsScreen == SETTINGS_CLOSED && !cardOpen && !feedMenuUntil && !playMenuUntil &&
         !wildPromptUntil && !confirmUntil && !choiceKind;
}

// toggleLight already skips eggs and ceremonies. Turning the screen back on does
// not wake the creature. Round board: a touch or the PWR button wakes the screen.
// CYD: only the BOOT button.
void turnScreenOff() {
  screenOff = true;
  if (!pet.sleeping) pet.toggleLight();
}

void updateBrightness(uint32_t now) {
  #pragma region Visible events wake the screen on their own
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  #pragma endregion

  #pragma region Pick the target brightness
  uint32_t idleMs = now - lastInteract;
  dimStage = (idleMs > NEARLY_OFF_AFTER_IDLE_MS) ? 2 : (idleMs > DIM_AFTER_IDLE_MS) ? 1 : 0;
  uint8_t normalBrightness = brightnessForSettingsLevel();
  // the sleep dim only applies to the main screen: any other screen or menu
  // opened while the creature sleeps comes back to the normal brightness
  bool sleepDim = pet.sleeping && isMainScreenShowing();
  uint8_t targetBrightness = sleepDim ? SLEEP_DIM_BRIGHTNESS : normalBrightness;
  if (dimStage == 1) targetBrightness = sleepDim ? IDLE_DIM_SLEEPING_BRIGHTNESS : IDLE_DIM_BRIGHTNESS;
  else if (dimStage == 2) targetBrightness = NEARLY_OFF_BRIGHTNESS;
  if (targetBrightness > normalBrightness) targetBrightness = normalBrightness;  // dimming never brightens a low setting
  if (screenOff) targetBrightness = 0;
  #pragma endregion

  #pragma region Apply it only when it changes
  static uint8_t currentBrightness = 255;
  if (targetBrightness != currentBrightness) {
    currentBrightness = targetBrightness;
    boardSetBrightness(targetBrightness);
  }
  #pragma endregion
}

void handlePowerKey(uint32_t now) {
  static uint32_t lastPwr = 0;
  if (now - lastPwr > POWER_KEY_POLL_MS) {
    lastPwr = now;
    if (pwrShortPressed()) {
      if (screenOff) {
        screenOff = false;
        lastInteract = now;
        markScreenDirty();  // static screens repaint right away after waking
      } else {
        turnScreenOff();
      }
    }
  }
}

// After 90 s of inactivity the screen already dims, so it flushes right away;
// active use still persists through each action's saves (eat/play/...).
void persistStateWhileIdle() {
  if (!(screenOff || dimStage >= 1 || pet.sleeping)) return;
  if (pet.savePending()) pet.flushSave();
  saveClockFormatToNvs();  // no-op unless a 12h/24h toggle is still unsaved
  rtcPersistIfDue();
}
#pragma endregion

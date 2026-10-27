// TamaPoke - pixel art tamagotchi inspired by gen 1
// Supports TWO boards (see boards/board_select.h); exactly one board flag is required:
//   - Waveshare ESP32-S3-Touch-AMOLED-1.75 (round 466x466): -D BOARD_WAVESHARE_ROUND
//     (PlatformIO: pio run -e waveshare-round)
//   - ESP32-2432S028R "Cheap Yellow Display" / ELEGOO 2.8" (rect. 320x240): -D BOARD_CYD_RECT
//     (PlatformIO: pio run -e cyd-rect). See CYD_PORT_NOTES.md for known limitations.
// Arduino IDE: uncomment the board #define at the top of boards/board_select.h.
//
// Libraries: "GFX Library for Arduino" (moononournation), U8g2, and on the Waveshare
// "SensorLib" + "XPowersLib" (Lewis He). Versions are pinned in platformio.ini.
//
// This file only holds setup() and loop(). Every screen and subsystem lives in its own
// module next to it (main_screen, touch_input, scene, battle_ui, gallery, ...).
// The sprites and the species table are generated with tools/sprites.py (emit).

#pragma region Includes
#include <Arduino.h>
#include <Wire.h>
#include "boards/board_select.h"  // picks the board (round Waveshare vs rect. CYD)
#include "app_state.h"            // shared game state: pet, open screens, timers
#include "board_display.h"        // displayBegin()
#include "ui_text.h"              // applyLangFont()
#include "touch_hal.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"
#include "wifi_manager.h"
#include "power_ui.h"             // brightness, dimming, power key, idle saves
#include "serial_console.h"       // serial commands and the health heartbeat
#include "settings_screens.h"     // saved clock format and brightness level
#include "touch_input.h"          // handleTouch()
#include "pet_render.h"           // ensureMon()
#include "battle_ui.h"            // maybeOfferWildEncounter()
#include "main_screen.h"          // render()
#pragma endregion

#pragma region Constants
// First boot with a truly blank RTC: 2026-01-01 00:00 UTC
#define RTC_BLANK_SEED_EPOCH 1767225600UL
// Real time is copied into the pet this often (persisted on every game save)
#define CLOCK_SAMPLE_INTERVAL_MS 30000
// 85 ms in game/bag: safe margin so the redraw does not stomp on the previous
// frame's DMA transfer (at 40-65 ms it overlapped and caused black flashes;
// with big sprites drawing takes longer, so some slack is left)
#define MINIGAME_RENDER_INTERVAL_MS 85
#define DEFAULT_RENDER_INTERVAL_MS 100
#pragma endregion

#pragma region Setup
void setup() {
  #pragma region Serial
  Serial.setRxBufferSize(8192);  // the SD transfer arrives in 2 KB blocks
  Serial.begin(115200);
  // CRITICAL: without this, Serial.print BLOCKS the game when no serial
  // monitor is open on the host (the USB CDC TX buffer fills up and nobody
  // drains it) -> with timeout 0 the messages are dropped
#if BOARD_USB_CDC_SERIAL
  Serial.setTxTimeoutMs(0);  // USB CDC only; a UART TX does drain
#endif
  Serial.printf("TamaPoke fw v%s\n", FW_VERSION);
  #pragma endregion

  #pragma region Saved settings
  loadLang();  // saved language (ES by default)
  loadClockFormatFromNvs();  // 12h/24h choice of the time screen
  loadBrightnessLevelFromNvs();  // brightness slider of the audio/video screen
  #pragma endregion

  #pragma region Shared I2C bus
#if BOARD_HAS_I2C_BUS
  Wire.begin(IIC_SDA, IIC_SCL);
  // touch, PMU and RTC share this I2C bus.
  // Safety net for PMU/RTC (SensorLib does NOT honor this timeout for the
  // touch; the sleeping-touch hang is solved by gating on INT, see
  // handleTouch).
  Wire.setTimeOut(50);
#endif
  #pragma endregion

  #pragma region Display
  // CRITICAL: turn on the panel power (BLDO1=OLED VDD 3.3V) BEFORE initializing
  // the display. If the PMU reset (full drain), this rail stays OFF and the
  // screen looks black even though the rest of the board works.
  pmuEnablePanel();
  // on band-canvas boards this also mounts the SD before allocating the bands
  displayBegin(brightnessForSettingsLevel());
  applyLangFont();  // font for the saved language (classic unless CJK)
  #pragma endregion

  touchHalBegin();

  #pragma region Pet and SD sprites
  pet.begin();
#if !BOARD_USES_BAND_CANVAS
  sdBegin();  // band-canvas boards mount it inside displayBegin()
#endif
  thumbs.load();
  #pragma endregion

  #pragma region Real clock: applies the time it was powered off
  rtcBegin();
  batBegin();
  pwrSetup();
  uint32_t bootEpoch = rtcEpoch();
  if (bootEpoch == 0) {
    // Without a backup cell, the RTC loses the time when power is cut.
    // Always seeding a fixed date would make game time go BACKWARDS relative
    // to what was already saved, and that breaks the streak, the age and the
    // scene's time of day. It is seeded with the last time seen, if later, so
    // time never goes backwards. The minutes spent off are lost anyway
    // (without an RTC there is no way to know them), but nothing gets out of sync.
    uint32_t seedEpoch = RTC_BLANK_SEED_EPOCH;
    uint32_t lastSavedEpoch = pet.savedEpoch();  // uses the NVS pet already has open
    if (lastSavedEpoch > seedEpoch) seedEpoch = lastSavedEpoch;
    rtcSetEpoch(seedEpoch);
    bootEpoch = rtcEpoch();
    Serial.printf("RTC has no time: seeded with %u%s\n", (unsigned)seedEpoch,
                  lastSavedEpoch > RTC_BLANK_SEED_EPOCH ? " (from the last saved time)" : "");
  }
#if !BOARD_HAS_RTC_CHIP
  // The soft clock is persisted only while idle, but every game save records the
  // time too: after a long active session and a power pull, the save is newer.
  if (pet.savedEpoch() > bootEpoch) {
    rtcSetEpoch(pet.savedEpoch());
    bootEpoch = rtcEpoch();
  }
#endif
  pet.syncClock(bootEpoch);
  #pragma endregion

  if (pet.awaitingStarter()) openClockForFirstRunSetup();  // first boot: date and time before the starter choice

  audioBegin();  // codec/speaker (plays a startup jingle)
  wifiBegin();   // after the framebuffer: the WiFi stack's RAM comes from what is left

  lastInteract = millis();
}
#pragma endregion

#pragma region Main loop
void loop() {
  uint32_t now = millis();
  pet.update(now);

  // the amplifier follows the sleep state (the call returns early if unchanged).
  // Here it covers every path: the light button, night sleep and sleep that
  // arrives applied from offline progression.
  audioSetSleeping(pet.sleeping);

  #pragma region Sound cues for evolve and runaway
  // plays a sound when the creature becomes ready to evolve (including the
  // case where it qualifies on waking up). canEvolveNow is false while asleep.
  static bool wasEvolveReady = false;
  bool evolveReady = pet.wantEvolveButton();
  if (evolveReady && !wasEvolveReady) sfxPlay(SFX_MEDAL);
  wasEvolveReady = evolveReady;
  // somber warning when the creature is about to run away from neglect
  static bool wasRunawayReady = false;
  bool runawayReady = pet.canRunawayNow();
  if (runawayReady && !wasRunawayReady) sfxPlay(SFX_DENY);
  wasRunawayReady = runawayReady;
  #pragma endregion

  handleTouch();
  handleSerial();
  wifiUpdate(now);  // scan results, connection result and timeout
  ensureMon();
  maybeOfferWildEncounter(now);  // random wild prompt (only on the idle main screen)
  handlePowerKey(now);  // short PWR press (BOOT button on the CYD): screen on/off
  updateBrightness(now);
  // flushes the autosave (and the soft RTC) only with the screen dimmed/off or asleep
  persistStateWhileIdle();

  #pragma region Record the real time
  static uint32_t lastClockSampleMs = 0;
  if (now - lastClockSampleMs > CLOCK_SAMPLE_INTERVAL_MS) {
    lastClockSampleMs = now;
    uint32_t currentEpoch = rtcEpoch();
    if (currentEpoch) pet.lastSeenEpoch = currentEpoch;
  }
  #pragma endregion

  printHealthHeartbeatIfDue(now);  // every 5 min, for the soak test

  #pragma region Render
  // Nothing is drawn with the screen off: at brightness 0 nothing is visible,
  // but the redraw and DMA flush of the 466x466 framebuffer kept running at
  // 10 fps, and measured that is 70 ms of work out of every 100 (46 of them just
  // the flush). It is the board's biggest avoidable load. Each render repaints
  // the whole scene, so nothing is left half-done; and since lastRender stays
  // frozen, the first frame after waking comes out immediately.
  uint32_t renderIntervalMs = (gameOpen || coinOpen || sackOpen) ? MINIGAME_RENDER_INTERVAL_MS
                            : battleOpen                        ? BATTLE_RENDER_INTERVAL_MS
                                                                : DEFAULT_RENDER_INTERVAL_MS;
  if (!screenOff && now - lastRender >= renderIntervalMs) {
    lastRender = now;
    render();
  }
  #pragma endregion
}
#pragma endregion

// Serial console: SD provisioning and the debugging commands (see serial_console.h).

#pragma region Includes
#include "serial_console.h"
#include "app_state.h"    // pet, mon, pmd, galleryPmd, gallery / battle / wild prompt state
#include "audio.h"        // sfxPlay
#include "battle.h"       // battleLevelFor, canStartWildBattle, pickWildSpecies, dayPhaseFromHour
#include "battle_ui.h"    // startBattleWith, startBattle, openWildPrompt
#include "dex.h"          // DEX_TBL, DEX_COUNT
#include "rtcbat.h"       // rtcEpoch, rtcSetEpoch, battery and USB readings
#include "scene.h"        // sceneHour
#include "sdmon.h"        // sdSerialCommand, sdReady
#include "wifi_manager.h" // wifiIsEnabled
#pragma endregion

#pragma region Constants
// HEALTH heartbeat period for the soak test
static const uint32_t HEALTH_HEARTBEAT_INTERVAL_MS = 300000;
// seconds in a day (CAREDAY moves the clock one day ahead)
static const uint32_t SECONDS_PER_DAY = 86400;
#pragma endregion

#pragma region Internal helpers
// the battle that BATTLE / BATTLE n opened (or did not open)
static void printBattleState() {
  Serial.printf("battle=%d #%d %s lv=%u shiny=%d caught=%d\n", battleOpen, battleDex,
                (battleDex >= 1 && battleDex <= DEX_COUNT) ? DEX_TBL[battleDex].name : "?",
                battleLevel, battleShiny, pet.isCaught(battleDex));
}
#pragma endregion

#pragma region Public API
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
    int speciesNumber = line.substring(5).toInt();
    if (speciesNumber >= 1 && speciesNumber <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = speciesNumber;
      Serial.printf("species #%d %s\n", speciesNumber, DEX_TBL[speciesNumber].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.ageMinutes = (uint32_t)line.substring(4).toInt() * MINUTES_PER_LEVEL;
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t epoch = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(epoch);
    pet.setClock(epoch);
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
    galleryFilterDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    #pragma region Simulate 20 egg rolls (does not change the game state)
    for (int i = 0; i < 20; i++) {
      int16_t rolledDex = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", rolledDex, DEX_TBL[rolledDex].name, DEX_TBL[rolledDex].rarity);
    }
    Serial.println();
    Serial.println("DONE");
    #pragma endregion
  } else if (line == "SHINY") {  // toggles shiny on the current one (testing)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simulates a new day of care (testing)
    pet.setClock(pet.lastSeenEpoch + SECONDS_PER_DAY);
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
    printBattleState();
    Serial.println("DONE");
  } else if (line == "BATTLE") {  // random wild battle (testing)
    startBattle();
    printBattleState();
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
    Serial.printf("pokedex %u/%d:", pet.registeredCount(), DEX_COUNT);
    for (int i = 1; i <= DEX_COUNT; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u maxblock=%u sd=%d mon=%d wifi=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), sdReady, pmd.loaded || mon.loaded,
                  wifiIsEnabled());
    Serial.println("DONE");
  } else if (line == "STATS") {
    #pragma region Creature stats dump
    Serial.printf("spec=%d lvl=%u full=%u joy=%u energy=%u hyg=%u mistakes=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("weight=%u atk=%u def=%u spe=%u genes=%u/%u/%u tr=%u/%u/%u berry=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
    #pragma endregion
  }
}

void printHealthHeartbeatIfDue(uint32_t now) {
  static uint32_t lastHealth = 0;
  if (now - lastHealth > HEALTH_HEARTBEAT_INTERVAL_MS) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u bat=%d%% mv=%d chg=%d usb=%d dim=%u off=%d\n",
                  (unsigned long)(now / 1000), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  batPercent(), batMillivolts(), batCharging() ? 1 : 0,
                  usbPresent() ? 1 : 0, dimStage, screenOff ? 1 : 0);
  }
}
#pragma endregion

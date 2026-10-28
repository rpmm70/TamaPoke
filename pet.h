#pragma once

#pragma region Includes
#include <Arduino.h>
#include <Preferences.h>
#include "dex.h"       // DEX_COUNT, DEX_BITMAP_BYTES (dex.h only includes i18n.h: no cycle)
#include "ceremony.h"  // CER_NONE, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE
#include "medals.h"    // MED_* bits, MED_COUNT
#pragma endregion

#pragma region Timing constants
// 1 tick = 1 game minute. Lower this value to test faster
// (e.g. 5000UL = stats drop 12x faster).
#define PET_TICK_MS 60000UL
// Game minutes per level. With 60, CHARMANDER evolves after ~16 h
// of play with perfect care. Lower to 1 to see evolutions right away.
#define MINUTES_PER_LEVEL 60
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // evolution animation (longer = more epic)
#define CEREMONY_MS 10000UL                // duration of the farewell on screen
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // says goodbye after 3 days of play (in final form)
#define RUNAWAY_TICKS 60                   // runs away after 1 h with EVERYTHING at zero
#pragma endregion

#pragma region Timer helper
// milliseconds left until `deadline` (0 if already past, or if deadline==0:
// "timer inactive"). All game timers (dialogs, animations) are stored as an
// absolute instant "deadline = millis() + duration" and used to be compared
// with millis() < deadline / millis() > deadline; that breaks when millis()
// wraps around after ~49.7 days of uptime. The unsigned subtraction,
// reinterpreted as signed, is safe against that wrap as long as the real
// interval is far below ~24.8 days (here the longest timer is ~12 s), without
// changing how the deadline is stored.
static inline uint32_t timeLeft(uint32_t deadline) {
  if (!deadline) return 0;
  int32_t left = (int32_t)(deadline - millis());
  return left > 0 ? (uint32_t)left : 0;
}
#pragma endregion

#pragma region Mood
enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };
#pragma endregion

#pragma region Wild battle rewards
// training stat raised by a wild battle win
enum BattleRewardStat : uint8_t { BATTLE_REWARD_NONE = 0, BATTLE_REWARD_ATK, BATTLE_REWARD_DEF, BATTLE_REWARD_SPE };
struct BattleReward {
  BattleRewardStat stat = BATTLE_REWARD_NONE;
  uint8_t amount = 0;
};
#pragma endregion

#pragma region Pet class
class Pet {
public:
  #pragma region Persistent state
  // Stats 0..100
  uint8_t fullness = 80;  // food
  uint8_t joy = 80;       // happiness
  uint8_t energy = 80;    // energy
  uint8_t hygiene = 100;  // cleanliness
  uint8_t poops = 0;      // poops on screen (max 3)
  uint8_t weight = 0;     // 0-100: candy fattens, the minigame burns it off
  // genes (90-110%, rolled at hatching) and training (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // has already discovered its favorite food
  bool shiny = false;       // rare color variant (rolled in the egg)
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1;      // Pokedex number (1-151), -1 = egg
  int16_t prevSpeciesId = -1;  // for the evolution animation
  uint8_t careMistakes = 0;   // neglect events: each delays evolution by 1 level
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // last RTC time seen (for offline progression)
  uint32_t hatchedEpoch = 0;    // RTC time it hatched (0 = egg, or no clock at the time)
  uint8_t ceremony = CER_NONE;  // farewell/runaway/release in progress
  uint8_t lastEnd = CER_NONE;   // how the previous one ended (affects the egg)
  uint8_t dexReg[DEX_BITMAP_BYTES] = { 0 };       // pokedex of raised species (DEX_COUNT-bit bitmap)
  uint8_t dexShinyReg[DEX_BITMAP_BYTES] = { 0 };  // raised or caught in shiny version
  #pragma region Wild battle state
  uint8_t dexCaught[DEX_BITMAP_BYTES] = { 0 };    // pokedex of caught wild species
  uint16_t battleWins = 0, battleLosses = 0;      // the player's: persist across raisings
  uint16_t battleStreak = 0, bestBattleStreak = 0;  // consecutive wins / best ever
  #pragma endregion
  // daily care streak (the player's: persists across raisings)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // bond (the creature's: rises slowly with care, resets when another is born)
  uint8_t bond = 0;
  char nick[12] = "";    // nickname (empty = species name)
  // medals: the individual's + cumulative counter across all raisings
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // just earned, to celebrate
  uint16_t lastMilestone = 0;  // streak milestone already celebrated
  uint16_t gameHi = 0;     // minigame record (the player's)
  uint16_t coinHi = 0;     // coin chase record (the player's)
  uint16_t strHi = 0;      // punching bag hits record
  #pragma endregion

  #pragma region Lifecycle
  void begin();                 // loads state from NVS (or creates the first egg)
  void update(uint32_t nowMs);  // call on every loop()
  #pragma endregion

  #pragma region Actions (touch buttons)
  void feed();              // apple (compatibility)
  void feedBerry(uint8_t color);  // 0 apple, 1 hamburger, 2 pizza
  void feedCandy();
  void feedCoffee();  // +energy, -food; never a favorite (lovesBerry only covers 0-2)
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // hidden per-species taste
  }
  void playResult(uint8_t score);  // ball game reward (trains ATK, at most +2)
  uint8_t coinChaseResult(uint8_t points);  // coin chase reward (trains SPE), returns the SPE gain
  uint8_t trainStrength(uint16_t hits);  // training bag (trains STR)

  // battle stats: real gen 1 base x genes + level + training
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  void play();
  void toggleLight();  // sleep / wake up
  void clean();
  void caress();  // touch the creature
  void eggTap();  // touch the egg: 3 taps and it hatches
  void newEgg();   // start from scratch with a random starter
  void release();  // release (long press + confirm)
  void syncClock(uint32_t nowEpoch);  // applies the time elapsed while off
  void setClock(uint32_t nowEpoch);   // sets the time without applying progression
  void startFarewell();  // also usable from the serial console (BYE)
  void startRunaway();   // also usable from the serial console (RUN)
  #pragma endregion

  #pragma region State queries
  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return timeLeft(eatUntil) > 0; }
  bool showHeart() const { return timeLeft(heartUntil) > 0; }
  bool evolving() const { return timeLeft(evolveUntil) > 0; }
  float evolveT() const {     // evolution animation progress 0..1
    return 1.0f - (float)timeLeft(evolveUntil) / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // evolution conditions met (ready)
  void evolve();              // triggers the transformation (called by a user tap)
  bool canFarewellNow() const;  // final form + 7 days: ready to say goodbye (button)
  bool canRunawayNow() const;   // total neglect for 1h: ready to run away (sad button)
  // the user decides in a dialog; "keep/stay together" postpones and re-offers later
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // re-offers on level up
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-offers within 1 day
  // first game: the player picks the starter (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  // wipes the whole NVS namespace, the save blob included (test: WIPE serial command)
  void factoryReset() { prefs.clear(); }
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  // uint16_t, not uint8_t: with MINUTES_PER_LEVEL=60 the level is the hours, so
  // a uint8_t wraps to 0 at level 256, after ~10.6 days. And that is reachable
  // in normal play, because "stay together" lets you postpone the farewell
  // indefinitely. Capped at 999 instead of overflowing.
  uint16_t level() const {
    uint32_t lv = 1 + ageMinutes / MINUTES_PER_LEVEL;
    return lv > 999 ? 999 : (uint16_t)lv;
  }
  // age is real time since hatching (the level stays on play time, ageMinutes);
  // without a hatch time or a clock it falls back to the play time
  uint32_t ageDays() const {
    if (!hatchedEpoch || !lastSeenEpoch) return ageMinutes / 1440;
    return lastSeenEpoch > hatchedEpoch ? (lastSeenEpoch - hatchedEpoch) / 86400 : 0;
  }
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && bitmapTest(dexReg, dex);
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && bitmapTest(dexShinyReg, dex);
  }
  uint16_t registeredCount() const;  // raised species only (caught ones are separate)
  #pragma endregion

  #pragma region Wild battles and catching
  bool isCaught(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && bitmapTest(dexCaught, dex);
  }
  uint16_t caughtCount() const;
  // marks a wild species as caught; a shiny catch also sets the shiny registry
  void registerCaught(int16_t dex, bool shinyVariant = false);
  // catch chance in percent after a win: 0 for legendaries, rare <= 60, otherwise 10..75.
  // requestedBattle (started from the play menu, not a wild encounter) scales it
  // by REQUESTED_BATTLE_CATCH_PCT (70 percent, pet.cpp).
  uint8_t catchChanceForWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel, bool closeWin,
                             bool requestedBattle = false) const;
  // lower chance offered after a loss (the wild one respects the effort): 0 or 5..25
  uint8_t respectCatchChanceForWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel,
                                    bool requestedBattle = false) const;
  // luckRoll is taken % 100 and compared with the chance; returns true if caught
  bool tryCatchWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel, bool closeWin,
                    uint8_t luckRoll, bool shinyVariant = false, bool requestedBattle = false);
  bool tryRespectCatchWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel,
                           uint8_t luckRoll, bool shinyVariant = false, bool requestedBattle = false);
  // a win trains the stat opposite to the wild one's strongest base stat
  BattleReward applyBattleWin(int16_t wildDex, bool closeWin);
  void applyBattleLoss();
  #pragma endregion

  #pragma region Eggs and ceremonies
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // rarity of the current egg (without revealing the species)
  int16_t pickEggSpecies();        // public so rolls can be simulated (EGGS)
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progress of the farewell/runaway ceremony, 0..1 (to animate it)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    return 1.0f - (float)timeLeft(ceremonyUntil) / (float)CEREMONY_MS;
  }
  #pragma endregion

  #pragma region Streak, bond, medals and name
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return timeLeft(medalUntil) > 0; }
  bool showMilestone() const { return timeLeft(milestoneUntil) > 0; }
  int careBonus() const;  // egg improvement from streak + bond
  #pragma endregion

  #pragma region Saving
  // deferred periodic save: tick() marks it pending and the loop flushes it
  // when the screen is dimmed/off (writing to flash freezes both cores for
  // ~1s: this way it is not visible and does not cut out the touch)
  bool savePending() const { return pendingSave; }
  // last persisted real time; used to reseed an RTC that lost its time
  uint32_t savedEpoch() { return prefs.getUInt("seen", 0); }
  void flushSave();
  #pragma endregion

private:
  #pragma region Runtime state (not persisted unless noted)
  Preferences prefs;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // hidden dex that will come out of the egg (persisted)
  bool eggShiny = false;       // surprise rolled when the egg is created (persisted)
  uint8_t eggTaps = 0;         // (persisted)
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // periodic save waiting to be flushed
  uint16_t evoDeclinedLv = 0;   // "keep form": do not offer evolution until next level
  uint32_t farDeclinedAge = 0;  // "stay together": do not offer farewell until this age
  bool starterPick = false;     // first game: waiting for the player to pick a starter (persisted)
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // well-cared streak: forges DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // daily cap on bond gain
  uint32_t medalUntil = 0;     // medal celebration on screen
  uint32_t milestoneUntil = 0; // streak milestone celebration
  #pragma endregion

  #pragma region Internal helpers
  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  // Helpers below never write to flash: the public action that calls them saves
  // once at the end.
  bool registerCare();   // first care of the day: streak + bond; true if it counted
  void addBond(uint8_t amt);
  void applyPlaySessionEffects(uint8_t score);  // shared by the ball game and the coin chase
  bool checkMedals();    // true if a new medal was earned
  void applySleepingMinute();  // one minute of rest: energy up, needs drop slowly with floors
  // raises a training stat by gain, capped at 100; returns what actually went in
  static uint8_t addTrainingPoints(uint8_t &trainingStat, uint8_t gain);
  void beginCeremony(uint8_t ceremonyKind, bool showHearts);
  bool markCaught(int16_t dex, bool shinyVariant);  // false if dex is out of range
  // fills branchesOut with the Eevee evolutions missing from the pokedex; returns how many
  uint8_t collectUnregisteredEeveeBranches(int16_t *branchesOut) const;
  void tick();
  void hatch();
  void registerSpecies(int16_t dex);
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
  #pragma endregion

  // dex 1 to DEX_COUNT maps to bit dex-1; callers check the range
  #pragma region Dex bitmaps
  static bool bitmapTest(const uint8_t *bitmap, int16_t dex) {
    return bitmap[(dex - 1) >> 3] & (1 << ((dex - 1) & 7));
  }
  static void bitmapSet(uint8_t *bitmap, int16_t dex) {
    bitmap[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  }
  static uint16_t bitmapCount(const uint8_t *bitmap);  // set bits for dex 1..DEX_COUNT only
  #pragma endregion

  #pragma region Storage (pet_storage.cpp)
  void save();
  void load();
  bool loadSaveBlob();     // true if a valid versioned blob was read
  void loadLegacyKeys();   // pre-blob format: one NVS key per field
  #pragma endregion
};
#pragma endregion

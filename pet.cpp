// Pet logic: needs, care, evolution, eggs, ceremonies and wild battle rewards.
// NVS save/load lives in pet_storage.cpp.

#pragma region Includes
#include "pet.h"
#include "dex.h"
#include "audio.h"
#include "nvs_settings.h"
#pragma endregion

#pragma region Constants
// Eevee (DEX_EEVEE) branches at random into the next three dex numbers
#define EEVEE_FIRST_BRANCH (DEX_EEVEE + 1)
#define EEVEE_BRANCH_COUNT 3

// a battle started from the play menu (swords) instead of a wild encounter keeps
// this percent of the normal catch chance (the encounter is the rarer event)
#define REQUESTED_BATTLE_CATCH_PCT 70

// offline progression is capped at 2 weeks
#define OFFLINE_MAX_MINUTES (14UL * 24 * 60)
#pragma endregion

#pragma region Internal helpers
// offline progression: time passed even though it was off, but mercifully -
// the bars drop with a floor at 15 (comes back hungry, not dead), with no
// care mistakes or runaways while absent
static uint8_t dropTo(uint8_t value, uint8_t amount, uint8_t floorValue) {
  if (value <= floorValue) return value;
  return (value - floorValue > amount) ? value - amount : floorValue;
}

static uint16_t calcStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base * gene / 100 + lvl + tr;
}
#pragma endregion

#pragma region Lifecycle
void Pet::begin() {
  prefs.begin(NVS_NAMESPACE, false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  lastTick = millis();
}

void Pet::newEgg() {
  ceremony = CER_NONE;
  neglectTicks = 0;
  weight = 0;
  speciesId = -1;
  prevSpeciesId = -1;
  eggTarget = pickEggSpecies();  // hidden species based on rarity and pokedex
  starterPick = (registeredCount() == 0);  // first game: the player picks the starter
  // shiny roll: 1/48 base, better after a farewell and with high streak/bond
  int shinyBase = (lastEnd == CER_FAREWELL ? 24 : 48) - careBonus();
  if (shinyBase < 8) shinyBase = 8;
  eggShiny = (random(shinyBase) == 0);
  eggTaps = 0;
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  hatchedEpoch = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  save();
}

void Pet::update(uint32_t nowMs) {
  #pragma region End of ceremony: the creature leaves and a new egg remains
  if (ceremony != CER_NONE && !timeLeft(ceremonyUntil)) {
    newEgg();
    return;
  }
  #pragma endregion

  #pragma region One tick per elapsed game minute
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
  #pragma endregion
}
#pragma endregion

#pragma region Clock and offline progression
void Pet::setClock(uint32_t nowEpoch) {
  lastSeenEpoch = nowEpoch;
  if (nowEpoch) save();  // persist now: a power cut does not lose the reference
}

void Pet::syncClock(uint32_t nowEpoch) {
  #pragma region Minutes elapsed while off
  uint32_t seenEpoch = prefs.getUInt("seen", 0);
  lastSeenEpoch = nowEpoch;
  if (nowEpoch == 0) return;
  uint32_t mins = (seenEpoch && nowEpoch > seenEpoch) ? (nowEpoch - seenEpoch) / 60 : 0;
  if (mins < 2 || ceremony != CER_NONE || starterPick) {
    save();  // first time, no time to apply, or still picking the starter
    return;
  }
  if (mins > OFFLINE_MAX_MINUTES) mins = OFFLINE_MAX_MINUTES;
  #pragma endregion

  #pragma region Replay each minute
  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) {  // hatches while you are away
        hatch();
        hatchedEpoch = seenEpoch + (i + 1) * 60;  // the minute it hatched, not the boot time
      }
      continue;
    }
    if (sleeping) {  // resting: drops slowly and with a floor, same as live
      applySleepingMinute();
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  #pragma endregion

  #pragma region Mess left while away
  if (!isEgg()) {
    if (!sleeping) {  // no mess while sleeping
      uint8_t poopTotal = poops + mins / 240;
      poops = poopTotal > 3 ? 3 : poopTotal;
    }
    // evolution is NOT applied offline: it stays ready and the user triggers it
    // by touching the creature when they return (so they see the transformation)
  }
  #pragma endregion

  Serial.printf("offline: %u min applied (lv.%u)\n", (unsigned)mins, (unsigned)level());
  save();
}
#pragma endregion

#pragma region Tick (one game minute)
// sleep is rest: energy recovers and needs drop MUCH slower than when awake,
// with a floor (wakes up wanting some attention, not at zero, with no care
// mistakes or runaways). Shared by the live tick and the offline replay.
void Pet::applySleepingMinute() {
  energy = clamp100(energy + 6);
  if (ageMinutes % 2 == 0) {                 // ~4x slower than awake
    fullness = dropTo(fullness, 1, 30);
    joy = dropTo(joy, 1, 35);
  }
  if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
}

void Pet::tick() {
  if (ceremony != CER_NONE) return;  // time stops during the farewell
  if (starterPick) return;  // the game does not start until the starter is
                            // picked: if time ran here, the egg would hatch on
                            // its own after 3 min with the rolled species and
                            // the player's choice would be lost
  ageMinutes++;

  #pragma region Egg
  if (isEgg()) {
    if (ageMinutes >= 3) {  // if you do not touch it, it hatches on its own after 3 min
      hatch();
      save();
    }
    return;
  }
  #pragma endregion

  #pragma region Sleeping
  // awake: food -2/min, hyg/joy -1/min. Asleep: see applySleepingMinute().
  // Weight still burns off; the good-care streak (goodTicks) is paused.
  if (sleeping) {
    applySleepingMinute();
    if (weight > 0 && ageMinutes % 3 == 0) weight--;
    if (checkMedals()) save();  // it can still cross a level by age while asleep
    if (++ticksSinceSave >= 5) pendingSave = true;
    return;
  }
  #pragma endregion

  #pragma region Awake needs
  if (ageMinutes % MINUTES_PER_LEVEL == 0) sfxPlay(SFX_LEVEL);  // leveled up (awake)

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // being overweight makes it lazy: energy drops twice as fast
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;
  #pragma endregion

  #pragma region Discipline forges defense: 12 h straight of good care = +1 DEF
  if (lowestStat() >= 40) {
    if (++goodTicks >= 720) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
    }
  } else {
    goodTicks = 0;
  }
  #pragma endregion

  #pragma region Joy
  int joyChange = -1;
  if (fullness < 30) joyChange -= 2;
  if (hygiene < 30) joyChange -= 2;
  joy = clamp100(joy + joyChange);
  #pragma endregion

  #pragma region Neglect
  // letting a stat bottom out counts as a care mistake (with a cooldown so the
  // same neglect is not counted every minute)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // neglect cools the bond, but without wiping it out:
                           // at -3 every 30 min far more was lost than could be
                           // gained in a whole day and the bond got stuck
  }
  #pragma endregion

  // evolution is triggered by the user (canEvolveNow + tap), not the tick
  if (checkMedals()) save();

  #pragma region Total neglect
  // with EVERYTHING at zero for an hour it becomes ready to run away; it does
  // NOT leave on its own, the user triggers it with the button (sad ending,
  // they witness it)
  if (fullness == 0 && joy == 0 && energy == 0 && hygiene == 0) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // a single act of care saves it
  }
  #pragma endregion

  // full cycle (final form + 7 days): the farewell does NOT fire on its own; it
  // becomes ready (canFarewellNow) and the user triggers it with the button, so they see it

  // periodic autosave: do NOT write to flash here (runs inside the loop, while
  // animating); just mark it and let the loop flush it when dimming
  if (++ticksSinceSave >= 5) pendingSave = true;
}
#pragma endregion

#pragma region Pokedex and eggs
// fills branchesOut (EEVEE_BRANCH_COUNT entries) with the unregistered Eevee branches
uint8_t Pet::collectUnregisteredEeveeBranches(int16_t *branchesOut) const {
  uint8_t branchTotal = 0;
  for (int16_t branch = EEVEE_FIRST_BRANCH; branch < EEVEE_FIRST_BRANCH + EEVEE_BRANCH_COUNT; branch++)
    if (!isRegistered(branch)) branchesOut[branchTotal++] = branch;
  return branchTotal;
}

// are there unregistered members left in this base's evolution line?
bool Pet::lineHasUnregistered(int16_t base) const {
  int16_t current = base;
  for (int guard = 0; current >= 1 && current <= DEX_COUNT && guard < 6; guard++) {
    if (!isRegistered(current)) return true;
    if (current == DEX_EEVEE) {
      int16_t missingBranches[EEVEE_BRANCH_COUNT];
      return collectUnregisteredEeveeBranches(missingBranches) > 0;
    }
    current = DEX_TBL[current].evolvesTo;
  }
  return false;
}

uint8_t Pet::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= DEX_COUNT) ? DEX_TBL[eggTarget].rarity : R_COMMON;
}

// picks the egg's species: rarity roll (improved by a completed farewell,
// penalized by a runaway) and a bias toward incomplete lines
int16_t Pet::pickEggSpecies() {
  #pragma region First game: classic starter
  if (registeredCount() == 0) {
    return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];
  }
  #pragma endregion

  #pragma region Rarity roll
  uint8_t tier = R_COMMON;
  if (lastEnd != CER_RUNAWAY) {
    bool blessed = (lastEnd == CER_FAREWELL);
    int rareChance = (blessed ? 45 : 27) + careBonus();
    int legendaryChance = (registeredCount() >= 25) ? (blessed ? 10 : 3) + careBonus() / 3 : 0;
    int roll = random(100);
    if (roll < legendaryChance) tier = R_LEGENDARY;
    else if (roll < legendaryChance + rareChance) tier = R_RARE;
  }
  #pragma endregion

  #pragma region Candidates
  // candidates of the tier with an incomplete line; if none, drop a tier;
  // if the tier's pokedex is complete, any of the tier will do
  for (int pass = 0; pass < 2; pass++) {
    for (int rarity = tier; rarity >= R_COMMON; rarity--) {
      int16_t candidates[DEX_COUNT];
      int candidateTotal = 0;
      for (int16_t dex = 1; dex <= DEX_COUNT && candidateTotal < DEX_COUNT; dex++) {
        if (DEX_TBL[dex].rarity != rarity) continue;
        if (pass == 0 && !lineHasUnregistered(dex)) continue;
        candidates[candidateTotal++] = dex;
      }
      if (candidateTotal > 0) return candidates[random(candidateTotal)];
    }
  }
  #pragma endregion
  return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];  // unreachable, just in case
}

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
  bitmapSet(dexReg, dex);
  if (shiny) bitmapSet(dexShinyReg, dex);
}

uint16_t Pet::bitmapCount(const uint8_t *bitmap) {
  // the last byte has bits past DEX_COUNT: they never count
  const uint8_t lastByteMask = (DEX_COUNT % 8) ? (uint8_t)((1u << (DEX_COUNT % 8)) - 1) : (uint8_t)0xFF;
  uint16_t setBits = 0;
  for (int byteIndex = 0; byteIndex < DEX_BITMAP_BYTES; byteIndex++) {
    uint8_t byteValue = bitmap[byteIndex];
    if (byteIndex == DEX_BITMAP_BYTES - 1) byteValue &= lastByteMask;
    setBits += (uint16_t)__builtin_popcount(byteValue);
  }
  return setBits;
}

uint16_t Pet::registeredCount() const {
  return bitmapCount(dexReg);
}
#pragma endregion

#pragma region Streak, bond and medals
// streak and bond improve the egg roll (0..~14)
int Pet::careBonus() const {
  int cappedStreak = streak > 30 ? 30 : streak;
  return cappedStreak / 3 + bond / 25;
}

// first care of the day: advances the streak and strengthens the bond.
// Returns true if it counted (the caller saves).
bool Pet::registerCare() {
  if (isEgg() || ceremony != CER_NONE) return false;
  uint32_t currentDay = today();
  if (currentDay == 0 || currentDay == lastCareDay) return false;  // no clock, or already counted today
  if (lastCareDay == 0 || currentDay == lastCareDay + 1) {
    streak++;
  } else {
    streak = 1;        // there was a gap of days
    lastMilestone = 0;
  }
  lastCareDay = currentDay;
  bondToday = 0;
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + 4);
  uint16_t milestone = (streak >= 100) ? 100 : (streak >= 30) ? 30
                     : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (milestone > lastMilestone) {
    lastMilestone = milestone;
    milestoneUntil = millis() + 4500;
  }
  checkMedals();
  return true;
}

void Pet::addBond(uint8_t amt) {
  if (bondToday >= 20) return;  // daily cap: the bond cannot be farmed
  bond = clamp100(bond + amt);
  bondToday += amt;
}

// returns true if a new medal was earned (the caller saves)
bool Pet::checkMedals() {
  if (isEgg()) return false;
  uint16_t before = medals;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (DEX_TBL[speciesId].evolvesTo == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (!gained) return false;
  totalMedals += (uint16_t)__builtin_popcount(gained);  // one per bit, not per event
  newMedal = gained;
  medalUntil = millis() + 4000;
  if (!sleeping) sfxPlay(SFX_MEDAL);
  return true;
}

void Pet::rename(const char *name) {
  strncpy(nick, name, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  save();
}
#pragma endregion

#pragma region Battle stats
uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bAtk, geneAtk, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bDef, geneDef, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpe, geneSpe, level(), trSpe);
}
#pragma endregion

#pragma region Wild battles and catching
uint16_t Pet::caughtCount() const {
  return bitmapCount(dexCaught);
}

bool Pet::markCaught(int16_t dex, bool shinyVariant) {
  if (dex < 1 || dex > DEX_COUNT) return false;
  bitmapSet(dexCaught, dex);
  if (shinyVariant) bitmapSet(dexShinyReg, dex);
  return true;
}

void Pet::registerCaught(int16_t dex, bool shinyVariant) {
  if (!markCaught(dex, shinyVariant)) return;
  save();
}

uint8_t Pet::catchChanceForWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel, bool closeWin,
                                bool requestedBattle) const {
  if (wildDex < 1 || wildDex > DEX_COUNT) return 0;
  const DexEntry &wild = DEX_TBL[wildDex];
  if (wild.rarity == R_LEGENDARY) return 0;
  int chance = wild.rarity == R_RARE ? 28 : 55;
  int levelGap = (int)wildLevel - (int)(petLevel ? petLevel : 1);
  if (levelGap > 0) chance -= levelGap * 4;
  else if (levelGap < 0) chance += (-levelGap) * 2;
  if (closeWin) chance += 8;
  chance += bond / 20;
  // the fork adds a step-counter bonus here; this port has no pedometer (bonus 0)
  if (wild.rarity == R_RARE && chance > 60) chance = 60;
  if (chance > 75) chance = 75;
  if (chance < 10) chance = 10;
  if (requestedBattle) chance = chance * REQUESTED_BATTLE_CATCH_PCT / 100;  // 7..52
  return (uint8_t)chance;
}

uint8_t Pet::respectCatchChanceForWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel,
                                       bool requestedBattle) const {
  uint8_t normalChance = catchChanceForWild(wildDex, wildLevel, petLevel, true, requestedBattle);
  if (normalChance == 0) return 0;
  uint8_t chance = (uint8_t)((uint16_t)normalChance * 40 / 100);
  if (chance < 5) chance = 5;
  if (chance > 25) chance = 25;
  return chance;
}

bool Pet::tryCatchWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel, bool closeWin,
                       uint8_t luckRoll, bool shinyVariant, bool requestedBattle) {
  uint8_t chance = catchChanceForWild(wildDex, wildLevel, petLevel, closeWin, requestedBattle);
  if (chance == 0) return false;
  if ((luckRoll % 100) < chance) {
    markCaught(wildDex, shinyVariant);
    joy = clamp100((int)joy + 4);
    addBond(1);
    save();
    return true;
  }
  return false;
}

bool Pet::tryRespectCatchWild(int16_t wildDex, uint8_t wildLevel, uint8_t petLevel,
                              uint8_t luckRoll, bool shinyVariant, bool requestedBattle) {
  uint8_t chance = respectCatchChanceForWild(wildDex, wildLevel, petLevel, requestedBattle);
  if (chance == 0) return false;
  if ((luckRoll % 100) < chance) {
    markCaught(wildDex, shinyVariant);
    save();
    return true;
  }
  return false;
}

BattleReward Pet::applyBattleWin(int16_t wildDex, bool closeWin) {
  BattleReward reward;
  if (ceremony != CER_NONE || isEgg()) return reward;
  if (wildDex < 1 || wildDex > DEX_COUNT) wildDex = 1;
  const DexEntry &wild = DEX_TBL[wildDex];

  #pragma region Training reward
  reward.amount = (wild.rarity == R_RARE) ? 2 : 1;
  if (closeWin) reward.amount++;
  if (wild.bAtk >= wild.bDef && wild.bAtk >= wild.bSpe) {
    reward.stat = BATTLE_REWARD_DEF;  // a hard hitter teaches defense
    trDef = clamp100((int)trDef + reward.amount);
  } else if (wild.bDef >= wild.bAtk && wild.bDef >= wild.bSpe) {
    reward.stat = BATTLE_REWARD_ATK;  // a tank teaches attack
    trAtk = clamp100((int)trAtk + reward.amount);
  } else {
    reward.stat = BATTLE_REWARD_SPE;
    trSpe = clamp100((int)trSpe + reward.amount);
  }
  #pragma endregion

  #pragma region Counters and mood
  battleWins++;
  battleStreak++;
  if (battleStreak > bestBattleStreak) bestBattleStreak = battleStreak;
  joy = clamp100((int)joy + 8 + (closeWin ? 4 : 0));
  energy = dropTo(energy, 8, 20);
  fullness = dropTo(fullness, 3, 10);
  addBond(closeWin ? 3 : 2);
  registerCare();
  #pragma endregion

  save();
  return reward;
}

void Pet::applyBattleLoss() {
  if (ceremony != CER_NONE || isEgg()) return;
  battleLosses++;
  battleStreak = 0;
  joy = dropTo(joy, 12, 20);
  energy = dropTo(energy, 18, 20);
  fullness = dropTo(fullness, 4, 10);
  save();
}
#pragma endregion

#pragma region Ceremonies
// final form that has completed its cycle (7 days): ready to say goodbye. The
// user triggers the farewell with the button (it does not fire on its own, so they see it)
bool Pet::canFarewellNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN;
}

// total neglect for 1h: ready to run away. The user triggers it with the
// button (sad ending); caring for it for a single tick saves it (neglectTicks resets)
bool Pet::canRunawayNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE && neglectTicks >= RUNAWAY_TICKS;
}

// shared start of every ceremony; showHearts keeps hearts up for the whole ceremony
void Pet::beginCeremony(uint8_t ceremonyKind, bool showHearts) {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = ceremonyKind;
  ceremony = ceremonyKind;
  ceremonyUntil = millis() + CEREMONY_MS;
  if (showHearts) heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::startFarewell() {
  beginCeremony(CER_FAREWELL, true);
}

void Pet::startRunaway() {
  beginCeremony(CER_RUNAWAY, false);
}

void Pet::release() {
  beginCeremony(CER_RELEASE, true);
}
#pragma endregion

#pragma region Hatching and evolution
// does not save: every caller saves right after
void Pet::hatch() {
  speciesId = eggTarget;
  shiny = eggShiny;
  hatchedEpoch = lastSeenEpoch;  // 0 if there is no clock yet: ageDays() falls back to play time
  // the individual's genes: 90-110% per stat (each raising is unique)
  geneAtk = 90 + random(21);
  geneDef = 90 + random(21);
  geneSpe = 90 + random(21);
  trAtk = trDef = trSpe = 0;
  berryKnown = false;
  bond = 0;          // bond, medals and name belong to the individual
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  battleWins = battleLosses = 0;  // the battle record belongs to the individual too
  battleStreak = bestBattleStreak = 0;
  registerSpecies(speciesId);  // raised = registered in the pokedex
  checkMedals();     // in case it is born already in final form (legendary)
  sfxPlay(SFX_HATCH);
}

// are the conditions to evolve met yet? Each care mistake delays evolution by
// 1 level, and it also has to be well cared for at that moment (no stat below
// 40). It does NOT evolve on its own: the user triggers it by touching the
// creature (evolve()), so they see the transformation.
bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  const DexEntry &entry = DEX_TBL[speciesId];
  if (entry.evolvesTo == 0) return false;
  return level() >= (uint16_t)entry.evolveLevel + careMistakes && lowestStat() >= 40;
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  const DexEntry &entry = DEX_TBL[speciesId];
  prevSpeciesId = speciesId;
  int16_t nextSpecies = entry.evolvesTo;
  if (speciesId == DEX_EEVEE) {
    // Eevee branch: prefers the evolution missing from the pokedex
    int16_t missingBranches[EEVEE_BRANCH_COUNT];
    uint8_t missingTotal = collectUnregisteredEeveeBranches(missingBranches);
    nextSpecies = missingTotal > 0 ? missingBranches[random(missingTotal)]
                                   : (int16_t)(EEVEE_FIRST_BRANCH + random(EEVEE_BRANCH_COUNT));
  }
  speciesId = nextSpecies;
  registerSpecies(speciesId);
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}
#pragma endregion

#pragma region Feeding
void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "loves it!"
    berryKnown = true;                 // discovered: shown on the info card
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // candy takes its toll
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

#pragma region Coffee
// a pick-me-up: restores energy but spoils the appetite. Not a berry color, so
// it can never be the favorite food (no heart, no bond, no berryKnown)
void Pet::feedCoffee() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  energy = clamp100(energy + 25);
  fullness = dropTo(fullness, 10, 0);
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}
#pragma endregion
#pragma endregion

#pragma region Training and minigames
uint8_t Pet::addTrainingPoints(uint8_t &trainingStat, uint8_t gain) {
  uint8_t statBefore = trainingStat;
  uint16_t raisedStat = (uint16_t)trainingStat + gain;
  trainingStat = raisedStat > 100 ? 100 : (uint8_t)raisedStat;
  return trainingStat - statBefore;  // what actually went in after capping at 100
}

#pragma region Play session effects
// mood, tiredness, weight and bond after a ball game or a coin chase; saves
void Pet::applyPlaySessionEffects(uint8_t score) {
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  energy = dropTo(energy, 10 + score / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score * 2;  // exercise burns weight
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}
#pragma endregion

void Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return;
  // the ball game trains attack: +1 per 5 points, at most +2 per game
  uint8_t attackGain = score / 5 > 2 ? 2 : score / 5;
  addTrainingPoints(trAtk, attackGain);
  if (score > gameHi) gameHi = score;  // new record
  applyPlaySessionEffects(score);
}

#pragma region Coin chase reward
// coin chase: points (0-15) train speed. Returns the gain.
uint8_t Pet::coinChaseResult(uint8_t points) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = points >= 10 ? 2 : (points >= 5 ? 1 : 0);
  gain = addTrainingPoints(trSpe, gain);
  if (points > coinHi) coinHi = points;  // new record
  applyPlaySessionEffects(points);
  return gain;
}
#pragma endregion

// training bag: hits train strength. Returns the gain.
uint8_t Pet::trainStrength(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 4;          // ~4 hits = 1 training point
  if (gain > 18) gain = 18;         // per-session cap: STR is forged slowly
  // what actually went in: when capping at 100 the screen announced +18 even if less fit
  gain = addTrainingPoints(trAtk, gain);
  energy = dropTo(energy, 12, 5);   // tiring
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / 3;  // also burns weight
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // hits record
  addBond(2);
  registerCare();
  save();
  return gain;
}
#pragma endregion

#pragma region Care
void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

// saves only when it was the first care of the day (a caress is frequent)
void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  if (registerCare()) save();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}
#pragma endregion

// Pet persistence (NVS). The whole persistent state is one versioned struct
// written with a single putBytes(); the RTC reference ("seen") stays a key of
// its own because savedEpoch() and syncClock() read it on their own. Saves from
// before the blob (one NVS key per field) are read once, rewritten as the blob
// and their keys removed.

#pragma region Includes
#include "pet.h"
#include "countof.h"
#include <stddef.h>  // offsetof
#pragma endregion

#pragma region Save blob format
#define PET_BLOB_KEY "pet_v2"        // key kept from version 2 so the upgrade reads it in place
#define PET_BLOB_MAGIC 0x54455054UL  // "TPET" little-endian
#define PET_BLOB_VERSION 3           // version 1 was the per-key format, 2 had no hatchedEpoch

// Field order: widest first, so the layout has no interior padding (save()
// zeroes the whole struct, trailing padding included). Changing any
// field means bumping PET_BLOB_VERSION (and migrating the previous version).
struct PetSaveBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;  // sizeof(PetSaveBlob)
  uint32_t ageMinutes;
  uint32_t lastCareDay;
  uint32_t hatchedEpoch;  // new in version 3
  int16_t speciesId;
  int16_t eggTarget;
  uint16_t streak, bestStreak;
  uint16_t medals, totalMedals, lastMilestone;
  uint16_t gameHi, coinHi, strHi;
  uint16_t battleWins, battleLosses, battleStreak, bestBattleStreak;
  uint8_t fullness, joy, energy, hygiene, poops, weight;
  uint8_t geneAtk, geneDef, geneSpe;
  uint8_t trAtk, trDef, trSpe;
  uint8_t eggTaps, careMistakes, lastEnd, bond;
  uint8_t berryKnown, shiny, eggShiny, starterPick, sleeping;
  uint8_t dexReg[DEX_BITMAP_BYTES];
  uint8_t dexShinyReg[DEX_BITMAP_BYTES];
  uint8_t dexCaught[DEX_BITMAP_BYTES];
  char nick[sizeof(Pet::nick)];
};

// version 2 was this same layout without hatchedEpoch (both sizes are multiples of 4)
#define PET_BLOB_V2_VERSION 2
#define PET_BLOB_V2_SIZE (sizeof(PetSaveBlob) - sizeof(uint32_t))

// keys written by the per-key format ("spec"/"eggT" by the even older one)
static const char *const LEGACY_PET_KEYS[] = {
  "full", "joy", "ene", "hyg", "poop", "wgt", "gatk", "gdef", "gspe",
  "tatk", "tdef", "tspe", "bk", "shy", "eshy", "stpk", "dexsh", "age",
  "dexn", "eggT2", "crack", "mist", "sleep", "lend", "dexreg", "strk",
  "bstrk", "cday", "bond", "medal", "tmedal", "mstone", "ghi", "chi",
  "shi", "nick", "dexcgt", "bwin", "bloss", "bstk", "bbstk", "spec", "eggT",
};
#pragma endregion

#pragma region Internal helpers
// a version 2 blob read into the start of `blob` (readLength bytes): shifts the
// fields after lastCareDay over to make room for hatchedEpoch (0 = unknown, load()
// estimates it). False if it is not a valid version 2 blob.
static bool upgradeVersion2SaveBlob(PetSaveBlob &blob, size_t readLength) {
  if (readLength != PET_BLOB_V2_SIZE || blob.magic != PET_BLOB_MAGIC ||
      blob.version != PET_BLOB_V2_VERSION || blob.size != PET_BLOB_V2_SIZE) return false;
  uint8_t *blobBytes = (uint8_t *)&blob;
  size_t hatchedOffset = offsetof(PetSaveBlob, hatchedEpoch);
  memmove(blobBytes + hatchedOffset + sizeof(blob.hatchedEpoch), blobBytes + hatchedOffset,
          PET_BLOB_V2_SIZE - hatchedOffset);
  blob.hatchedEpoch = 0;
  blob.version = PET_BLOB_VERSION;
  blob.size = sizeof(PetSaveBlob);
  return true;
}

// reads the blob and checks magic, version and size (upgrading a version 2 one);
// false if missing or invalid
static bool readValidSaveBlob(Preferences &preferences, PetSaveBlob &blob) {
  if (!preferences.isKey(PET_BLOB_KEY)) return false;
  size_t readLength = preferences.getBytes(PET_BLOB_KEY, &blob, sizeof(blob));
  if (readLength != sizeof(blob)) return upgradeVersion2SaveBlob(blob, readLength);
  return blob.magic == PET_BLOB_MAGIC && blob.version == PET_BLOB_VERSION &&
         blob.size == sizeof(PetSaveBlob);
}

// Copies an unreadable blob to PET_BLOB_UNREADABLE_KEY (once: an existing copy is kept)
// so a downgrade or corruption never destroys the only copy of the save.
#define PET_BLOB_UNREADABLE_KEY "pet_v2_bad"
#define PET_BLOB_UNREADABLE_MAX_BYTES 512
static void preserveUnreadableSaveBlob(Preferences &preferences) {
  if (preferences.isKey(PET_BLOB_UNREADABLE_KEY)) return;
  size_t storedLength = preferences.getBytesLength(PET_BLOB_KEY);
  if (storedLength == 0 || storedLength > PET_BLOB_UNREADABLE_MAX_BYTES) return;
  uint8_t rawBytes[PET_BLOB_UNREADABLE_MAX_BYTES];
  size_t readLength = preferences.getBytes(PET_BLOB_KEY, rawBytes, storedLength);
  if (readLength) preferences.putBytes(PET_BLOB_UNREADABLE_KEY, rawBytes, readLength);
}

static void removeLegacyPetKeys(Preferences &preferences) {
  for (size_t keyIndex = 0; keyIndex < countof(LEGACY_PET_KEYS); keyIndex++)
    if (preferences.isKey(LEGACY_PET_KEYS[keyIndex])) preferences.remove(LEGACY_PET_KEYS[keyIndex]);
}
#pragma endregion

#pragma region Deferred save
// flushes the pending periodic save (called by the loop at a moment with no
// animation so the flash-write stall is not visible)
void Pet::flushSave() {
  if (pendingSave) save();
}
#pragma endregion

#pragma region Save
void Pet::save() {
  ticksSinceSave = 0;
  pendingSave = false;

  #pragma region Pack the blob
  PetSaveBlob blob;
  memset(&blob, 0, sizeof(blob));
  blob.magic = PET_BLOB_MAGIC;
  blob.version = PET_BLOB_VERSION;
  blob.size = sizeof(PetSaveBlob);
  blob.ageMinutes = ageMinutes;
  blob.lastCareDay = lastCareDay;
  blob.hatchedEpoch = hatchedEpoch;
  blob.speciesId = speciesId;
  blob.eggTarget = eggTarget;
  blob.streak = streak;
  blob.bestStreak = bestStreak;
  blob.medals = medals;
  blob.totalMedals = totalMedals;
  blob.lastMilestone = lastMilestone;
  blob.gameHi = gameHi;
  blob.coinHi = coinHi;
  blob.strHi = strHi;
  blob.battleWins = battleWins;
  blob.battleLosses = battleLosses;
  blob.battleStreak = battleStreak;
  blob.bestBattleStreak = bestBattleStreak;
  blob.fullness = fullness;
  blob.joy = joy;
  blob.energy = energy;
  blob.hygiene = hygiene;
  blob.poops = poops;
  blob.weight = weight;
  blob.geneAtk = geneAtk;
  blob.geneDef = geneDef;
  blob.geneSpe = geneSpe;
  blob.trAtk = trAtk;
  blob.trDef = trDef;
  blob.trSpe = trSpe;
  blob.eggTaps = eggTaps;
  blob.careMistakes = careMistakes;
  blob.lastEnd = lastEnd;
  blob.bond = bond;
  blob.berryKnown = berryKnown;
  blob.shiny = shiny;
  blob.eggShiny = eggShiny;
  blob.starterPick = starterPick;
  blob.sleeping = sleeping;
  memcpy(blob.dexReg, dexReg, sizeof(blob.dexReg));
  memcpy(blob.dexShinyReg, dexShinyReg, sizeof(blob.dexShinyReg));
  memcpy(blob.dexCaught, dexCaught, sizeof(blob.dexCaught));
  memcpy(blob.nick, nick, sizeof(blob.nick));
  #pragma endregion

  prefs.putBytes(PET_BLOB_KEY, &blob, sizeof(blob));
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
}
#pragma endregion

#pragma region Load
void Pet::load() {
  #pragma region Blob, or one-time migration from the per-key format
  bool migrating = !loadSaveBlob();
  if (migrating) {
    // A blob that exists but does not validate (corrupt, or written by a newer
    // firmware) would be overwritten by the next save: keep a raw copy aside first.
    if (prefs.isKey(PET_BLOB_KEY)) preserveUnreadableSaveBlob(prefs);
    loadLegacyKeys();
  }
  #pragma endregion

  #pragma region Fixes for old saves
  if (geneAtk == 0) {  // pet predating genes: one-time roll now
    geneAtk = 90 + random(21);
    geneDef = 90 + random(21);
    geneSpe = 90 + random(21);
  }
  // seed: the current pet counts as raised (old saves)
  if (speciesId >= 1) registerSpecies(speciesId);
  // hatched before the hatch time was saved: estimate it from the play time
  uint32_t savedSeenEpoch = savedEpoch();
  uint32_t playedSeconds = ageMinutes * 60;
  if (speciesId >= 1 && !hatchedEpoch && savedSeenEpoch > playedSeconds)
    hatchedEpoch = savedSeenEpoch - playedSeconds;
  #pragma endregion

  #pragma region Finish the migration
  if (migrating) {
    save();
    PetSaveBlob writtenBlob;
    if (readValidSaveBlob(prefs, writtenBlob)) removeLegacyPetKeys(prefs);  // keep them if the write failed
  }
  #pragma endregion
}

bool Pet::loadSaveBlob() {
  PetSaveBlob blob;
  if (!readValidSaveBlob(prefs, blob)) return false;
  ageMinutes = blob.ageMinutes;
  lastCareDay = blob.lastCareDay;
  hatchedEpoch = blob.hatchedEpoch;
  speciesId = blob.speciesId;
  eggTarget = blob.eggTarget;
  streak = blob.streak;
  bestStreak = blob.bestStreak;
  medals = blob.medals;
  totalMedals = blob.totalMedals;
  lastMilestone = blob.lastMilestone;
  gameHi = blob.gameHi;
  coinHi = blob.coinHi;
  strHi = blob.strHi;
  battleWins = blob.battleWins;
  battleLosses = blob.battleLosses;
  battleStreak = blob.battleStreak;
  bestBattleStreak = blob.bestBattleStreak;
  fullness = blob.fullness;
  joy = blob.joy;
  energy = blob.energy;
  hygiene = blob.hygiene;
  poops = blob.poops;
  weight = blob.weight;
  geneAtk = blob.geneAtk;
  geneDef = blob.geneDef;
  geneSpe = blob.geneSpe;
  trAtk = blob.trAtk;
  trDef = blob.trDef;
  trSpe = blob.trSpe;
  eggTaps = blob.eggTaps;
  careMistakes = blob.careMistakes;
  lastEnd = blob.lastEnd;
  bond = blob.bond;
  berryKnown = blob.berryKnown != 0;
  shiny = blob.shiny != 0;
  eggShiny = blob.eggShiny != 0;
  starterPick = blob.starterPick != 0;
  sleeping = blob.sleeping != 0;
  memcpy(dexReg, blob.dexReg, sizeof(dexReg));
  memcpy(dexShinyReg, blob.dexShinyReg, sizeof(dexShinyReg));
  memcpy(dexCaught, blob.dexCaught, sizeof(dexCaught));
  memcpy(nick, blob.nick, sizeof(nick));
  nick[sizeof(nick) - 1] = 0;
  return true;
}

void Pet::loadLegacyKeys() {
  fullness = prefs.getUChar("full", 80);
  joy = prefs.getUChar("joy", 80);
  energy = prefs.getUChar("ene", 80);
  hygiene = prefs.getUChar("hyg", 100);
  poops = prefs.getUChar("poop", 0);
  weight = prefs.getUChar("wgt", 0);
  geneAtk = prefs.getUChar("gatk", 0);
  geneDef = prefs.getUChar("gdef", 0);
  geneSpe = prefs.getUChar("gspe", 0);
  trAtk = prefs.getUChar("tatk", 0);
  trDef = prefs.getUChar("tdef", 0);
  trSpe = prefs.getUChar("tspe", 0);
  berryKnown = prefs.getBool("bk", false);
  shiny = prefs.getBool("shy", false);
  eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  prefs.getBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  ageMinutes = prefs.getUInt("age", 0);

  #pragma region Species and egg target
  if (prefs.isKey("dexn")) {
    speciesId = prefs.getShort("dexn", -1);
    eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migration from the version with flash indices (0-8)
    static const uint8_t OLD2DEX[] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t oldSpecies = prefs.getChar("spec", -1);
    speciesId = (oldSpecies >= 0 && (size_t)oldSpecies < countof(OLD2DEX)) ? OLD2DEX[oldSpecies] : -1;
    int8_t oldEggTarget = prefs.getChar("eggT", 0);
    eggTarget = (oldEggTarget >= 0 && (size_t)oldEggTarget < countof(OLD2DEX)) ? OLD2DEX[oldEggTarget] : 4;
  }
  #pragma endregion

  eggTaps = prefs.getUChar("crack", 0);
  careMistakes = prefs.getUChar("mist", 0);
  sleeping = prefs.getBool("sleep", false);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  coinHi = prefs.getUShort("chi", 0);
  strHi = prefs.getUShort("shi", 0);
  prefs.getString("nick", nick, sizeof(nick));

  #pragma region Wild battle state
  prefs.getBytes("dexcgt", dexCaught, sizeof(dexCaught));
  battleWins = prefs.getUShort("bwin", 0);
  battleLosses = prefs.getUShort("bloss", 0);
  battleStreak = prefs.getUShort("bstk", 0);
  bestBattleStreak = prefs.getUShort("bbstk", 0);
  #pragma endregion
}
#pragma endregion

// Wild battle logic: pure functions with no hardware dependencies, so the
// whole module can be tested on the host. All randomness comes in as a
// luck roll (0..99) supplied by the caller.

#pragma region Includes
#include "battle.h"
#include "dex.h"
#include "ceremony.h"
#include "countof.h"
#pragma endregion

namespace {

#pragma region Constants
constexpr uint8_t MAX_BATTLE_ROUNDS = 50;  // resolveBattle() auto battle (turn battles have no limit)
#pragma endregion

#pragma region Shared helpers
// HP used by the auto battle when the stats do not carry an explicit HP
uint16_t legacyHpFor(const BattleStats &stats) {
  if (stats.hp > 0) return stats.hp;
  return (uint16_t)(18 + stats.level * 3 + stats.def / 2);
}

uint8_t clampLuckRoll(uint8_t luckRoll) {
  return luckRoll > 99 ? 99 : luckRoll;
}

// value * percent / 100, truncated to 16 bits, never below 1
uint16_t scaleByPercentAtLeastOne(uint16_t value, uint16_t percent) {
  uint16_t scaledValue = (uint16_t)((uint32_t)value * percent / 100);
  return scaledValue == 0 ? 1 : scaledValue;
}
#pragma endregion

#pragma region Type chart
// relation of one attacking type against one defending type (modern chart).
// Immunities are softened to a strong resistance by computeTypeEffectPct().
enum TypeRelation : int8_t { IMMUNE = -2, RESIST = -1, EVEN = 0, SUPER = 1 };

// [attack type][defend type], indexed by the TYPE_* enum in dex.h. TYPE_NONE
// (row and column 0) is always EVEN.
// Columns: NONE NORMAL FIRE WATER ELECTRIC GRASS ICE FIGHTING POISON GROUND
//          FLYING PSYCHIC BUG ROCK GHOST DRAGON DARK STEEL FAIRY
constexpr int8_t TYPE_RELATIONS[TYPE_COUNT][TYPE_COUNT] = {
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN },  // NONE
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   RESIST, IMMUNE, EVEN,   EVEN,   RESIST, EVEN },  // NORMAL
  { EVEN,   EVEN,   RESIST, RESIST, EVEN,   SUPER,  SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  RESIST, EVEN,   RESIST, EVEN,   SUPER,  EVEN },  // FIRE
  { EVEN,   EVEN,   SUPER,  RESIST, EVEN,   RESIST, EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   RESIST, EVEN,   EVEN,   EVEN },  // WATER
  { EVEN,   EVEN,   EVEN,   SUPER,  RESIST, RESIST, EVEN,   EVEN,   EVEN,   IMMUNE, SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   RESIST, EVEN,   EVEN,   EVEN },  // ELECTRIC
  { EVEN,   EVEN,   RESIST, SUPER,  EVEN,   RESIST, EVEN,   EVEN,   RESIST, SUPER,  RESIST, EVEN,   RESIST, SUPER,  EVEN,   RESIST, EVEN,   RESIST, EVEN },  // GRASS
  { EVEN,   EVEN,   RESIST, RESIST, EVEN,   SUPER,  RESIST, EVEN,   EVEN,   SUPER,  SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   RESIST, EVEN },  // ICE
  { EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   RESIST, EVEN,   RESIST, RESIST, RESIST, SUPER,  IMMUNE, EVEN,   SUPER,  SUPER,  RESIST },  // FIGHTING
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   RESIST, RESIST, EVEN,   EVEN,   EVEN,   RESIST, RESIST, EVEN,   EVEN,   IMMUNE, SUPER },  // POISON
  { EVEN,   EVEN,   SUPER,  EVEN,   SUPER,  RESIST, EVEN,   EVEN,   SUPER,  EVEN,   IMMUNE, EVEN,   RESIST, SUPER,  EVEN,   EVEN,   EVEN,   SUPER,  EVEN },  // GROUND
  { EVEN,   EVEN,   EVEN,   EVEN,   RESIST, SUPER,  EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  RESIST, EVEN,   EVEN,   EVEN,   RESIST, EVEN },  // FLYING
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  SUPER,  EVEN,   EVEN,   RESIST, EVEN,   EVEN,   EVEN,   EVEN,   IMMUNE, RESIST, EVEN },  // PSYCHIC
  { EVEN,   EVEN,   RESIST, EVEN,   EVEN,   SUPER,  EVEN,   RESIST, RESIST, EVEN,   RESIST, SUPER,  EVEN,   EVEN,   RESIST, EVEN,   SUPER,  RESIST, RESIST },  // BUG
  { EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   SUPER,  RESIST, EVEN,   RESIST, SUPER,  EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   RESIST, EVEN },  // ROCK
  { EVEN,   IMMUNE, EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   SUPER,  EVEN,   RESIST, EVEN,   EVEN },  // GHOST
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   RESIST, IMMUNE },  // DRAGON
  { EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   RESIST, EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   SUPER,  EVEN,   RESIST, EVEN,   RESIST },  // DARK
  { EVEN,   EVEN,   RESIST, RESIST, RESIST, EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  EVEN,   EVEN,   EVEN,   RESIST, SUPER },  // STEEL
  { EVEN,   EVEN,   RESIST, EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  RESIST, EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   EVEN,   SUPER,  SUPER,  RESIST, EVEN },  // FAIRY
};

// out-of-range types (never produced by dex.h) are neutral
int8_t typeRelationFor(uint8_t attackType, uint8_t defendType) {
  if (attackType >= TYPE_COUNT || defendType >= TYPE_COUNT) return EVEN;
  return TYPE_RELATIONS[attackType][defendType];
}

// light type multiplier in percent: x1.2 per weakness, x0.85 per resistance,
// x0.7 per immunity, applied once per defending type
uint8_t computeTypeEffectPct(uint8_t attackType, uint8_t defendType1, uint8_t defendType2) {
  uint32_t effectPercent = 100;
  const uint8_t defendTypes[2] = { defendType1, defendType2 };
  for (size_t i = 0; i < countof(defendTypes); i++) {
    int8_t relation = typeRelationFor(attackType, defendTypes[i]);
    if (relation == SUPER) effectPercent = effectPercent * 120 / 100;
    else if (relation == RESIST) effectPercent = effectPercent * 85 / 100;
    else if (relation == IMMUNE) effectPercent = effectPercent * 70 / 100;
  }
  if (effectPercent < 1) effectPercent = 1;
  return effectPercent > 255 ? 255 : (uint8_t)effectPercent;
}

uint16_t applyTypeMultiplierToDamage(uint16_t damage, const BattleStats &attacker, const BattleStats &defender) {
  uint32_t scaledDamage = (uint32_t)damage * computeTypeEffectPct(attacker.type1, defender.type1, defender.type2) / 100;
  if (scaledDamage < 1) scaledDamage = 1;
  return scaledDamage > 65535 ? 65535 : (uint16_t)scaledDamage;
}
#pragma endregion

#pragma region Damage
uint16_t computeDamage(const BattleStats &attacker, const BattleStats &defender, uint8_t luckRoll) {
  uint16_t attackerLevel = attacker.level > 0 ? attacker.level : 1;
  uint32_t pressure = (uint32_t)attacker.atk * (10 + attackerLevel / 4);
  uint32_t guard = (uint32_t)defender.def + 35;
  uint32_t baseDamage = 1 + pressure / guard;
  uint32_t variationPercent = 90 + (clampLuckRoll(luckRoll) % 21);  // 90..110 %
  uint32_t damage = baseDamage * variationPercent / 100;
  if (damage == 0) damage = 1;
  if (damage > 65535) damage = 65535;
  return applyTypeMultiplierToDamage((uint16_t)damage, attacker, defender);
}

void applyHitToHp(uint16_t &hpLeft, uint16_t damage, uint16_t &damageTotal) {
  uint16_t dealt = damage > hpLeft ? hpLeft : damage;
  hpLeft -= dealt;
  damageTotal += dealt;
}
#pragma endregion

#pragma region Turn battle helpers
// turn battles get more HP than the auto battle so a fight lasts a few rounds
uint16_t computeTurnMaxHp(const BattleStats &stats) {
  uint16_t statsLevel = stats.level > 0 ? stats.level : 1;
  uint32_t scaledHp = 30 + (uint32_t)statsLevel * 5 + stats.def;
  uint16_t legacyHp = legacyHpFor(stats);
  if (scaledHp < legacyHp) scaledHp = legacyHp;
  return scaledHp > 65535 ? 65535 : (uint16_t)scaledHp;
}

// one hit can never take more than 35% of the defender's max HP
uint16_t computeCappedTurnDamage(const BattleStats &attacker,
                                 const BattleStats &defender,
                                 uint16_t defenderMaxHp,
                                 uint8_t luckRoll,
                                 bool counter,
                                 uint8_t powerPercent) {
  uint16_t damage = computeDamage(attacker, defender, luckRoll);
  damage = scaleByPercentAtLeastOne(damage, powerPercent);
  if (counter) damage = (uint16_t)((uint32_t)damage * 3 / 2);
  uint16_t damageCap = (uint16_t)((uint32_t)defenderMaxHp * 35 / 100);
  if (damageCap < 1) damageCap = 1;
  if (damage > damageCap) damage = damageCap;
  return damage;
}

bool isBattleFinished(const BattleRuntime &battle) {
  return battle.playerHp == 0 || battle.enemyHp == 0;
}

// the side still standing wins; the HP comparison only matters if both reached 0
// in the same exchange (ties go to the player)
bool isPlayerWinner(const BattleRuntime &battle) {
  if (battle.enemyHp == 0) return true;
  if (battle.playerHp == 0) return false;
  return battle.playerHp >= battle.enemyHp;
}

bool isAttackAction(BattleAction action) {
  return action == BATTLE_ATTACK || action == BATTLE_ATTACK_QUICK || action == BATTLE_ATTACK_HEAVY;
}

uint8_t attackPowerPctFor(BattleAction action) {
  if (action == BATTLE_ATTACK_QUICK) return 85;
  if (action == BATTLE_ATTACK_HEAVY) return 125;
  return 100;
}

uint8_t enemyDodgeChanceFor(const BattleRuntime &battle, BattleAction action) {
  int dodgeChance = battle.enemy.spe > battle.player.spe ? 18 : 10;
  if (action == BATTLE_ATTACK_QUICK) dodgeChance -= 9;
  else if (action == BATTLE_ATTACK_HEAVY) dodgeChance += 22;
  if (dodgeChance < 0) dodgeChance = 0;
  if (dodgeChance > 45) dodgeChance = 45;
  return (uint8_t)dodgeChance;
}

// the wild one chose to dodge: quick attacks are harder to read, heavy ones easier
uint8_t enemyStanceDodgeChanceFor(BattleAction action) {
  if (action == BATTLE_ATTACK_QUICK) return 40;
  if (action == BATTLE_ATTACK_HEAVY) return 80;
  return 60;
}

// the player chose to dodge: a quick enemy attack is harder to avoid, a heavy one easier
uint8_t playerDodgeChanceFor(BattleAction enemyAction) {
  if (enemyAction == BATTLE_ATTACK_QUICK) return 70;
  if (enemyAction == BATTLE_ATTACK_HEAVY) return 95;
  return 85;
}

// a wild one winding up a heavy attack is left open: the player's attack lands harder
uint8_t playerPowerPctFor(BattleAction action, BattleAction enemyAction) {
  uint16_t powerPercent = attackPowerPctFor(action);
  if (enemyAction == BATTLE_ATTACK_HEAVY) powerPercent = powerPercent * 120 / 100;
  return (uint8_t)powerPercent;
}

bool isHpLow(uint16_t hp, uint16_t maxHp) {
  return (uint32_t)hp * 100 <= (uint32_t)maxHp * 30;
}
#pragma endregion

#pragma region Wild species helpers
// which types show up more often at each time of day
bool isWildTypePreferredForPhase(uint8_t type1, uint8_t type2, uint8_t phase) {
  auto hasType = [&](uint8_t wantedType) { return type1 == wantedType || type2 == wantedType; };
  switch (phase) {
    case DAY_PHASE_MORNING: return hasType(TYPE_GRASS) || hasType(TYPE_FLYING) || hasType(TYPE_NORMAL) || hasType(TYPE_BUG);
    case DAY_PHASE_EVENING: return hasType(TYPE_WATER) || hasType(TYPE_FLYING) || hasType(TYPE_FIRE);
    case DAY_PHASE_NIGHT: return hasType(TYPE_GHOST) || hasType(TYPE_POISON) || hasType(TYPE_BUG);
    default: return true;
  }
}
#pragma endregion

}  // namespace

#pragma region Day phase and level helpers
uint8_t dayPhaseFromHour(int hour) {
  if (hour >= 6 && hour < 12) return DAY_PHASE_MORNING;
  if (hour >= 12 && hour < 18) return DAY_PHASE_DAY;
  if (hour >= 18 && hour < 22) return DAY_PHASE_EVENING;
  return DAY_PHASE_NIGHT;
}

uint8_t battleLevelFor(uint16_t petLevel) {
  return petLevel > BATTLE_LEVEL_CAP ? BATTLE_LEVEL_CAP : (uint8_t)petLevel;
}
#pragma endregion

#pragma region Battle HP
uint16_t battleMaxHpFor(const BattleStats &stats) {
  return computeTurnMaxHp(stats);
}
#pragma endregion

#pragma region Wild encounter
uint8_t battleTypeEffectPct(uint8_t attackType, uint8_t defendType1, uint8_t defendType2) {
  return computeTypeEffectPct(attackType, defendType1, defendType2);
}

bool canStartWildBattle(bool isEgg, bool sleeping, uint8_t ceremony) {
  return !isEgg && !sleeping && ceremony == CER_NONE;
}

uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll) {
  int baseLevel = (int)(petLevel ? petLevel : 1);
  int levelDelta;
  if (luckRoll < 55) levelDelta = (int)(luckRoll % 3) - 1;       // -1..+1, most common
  else if (luckRoll < 85) levelDelta = -2 - (int)(luckRoll % 3); // -2..-4, fair catch-up fights
  else levelDelta = 2 + (int)(luckRoll % 2);                     // +2..+3, occasional danger
  int wildLevel = baseLevel + levelDelta;
  if (wildLevel < 1) wildLevel = 1;
  return wildLevel > BATTLE_LEVEL_CAP ? BATTLE_LEVEL_CAP : (uint8_t)wildLevel;
}

// 25% rare, otherwise common (never legendary or evolution-only); prefers the
// types of the day phase and falls back to any species of the rarity
int16_t pickWildSpecies(uint8_t roll, uint8_t phase) {
  int16_t candidatePool[DEX_COUNT];
  int candidateCount = 0;
  uint8_t targetRarity = (roll % 100) < 25 ? R_RARE : R_COMMON;

  auto fillPoolForRarity = [&](uint8_t rarity, bool preferPhaseTypes) {
    candidateCount = 0;
    for (int16_t dex = 1; dex <= DEX_COUNT; dex++) {
      const DexEntry &entry = DEX_TBL[dex];
      if (entry.rarity != rarity) continue;
      if (preferPhaseTypes && !isWildTypePreferredForPhase(entry.type1, entry.type2, phase)) continue;
      candidatePool[candidateCount++] = dex;
    }
  };

  fillPoolForRarity(targetRarity, true);
  if (candidateCount == 0) fillPoolForRarity(targetRarity, false);
  if (candidateCount == 0 && targetRarity == R_RARE) {
    fillPoolForRarity(R_COMMON, true);
    if (candidateCount == 0) fillPoolForRarity(R_COMMON, false);
  }
  return candidateCount > 0 ? candidatePool[roll % candidateCount] : 1;
}

BattleStats wildBattleStats(int16_t dex, uint8_t level) {
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  const DexEntry &entry = DEX_TBL[dex];
  uint8_t wildLevel = level ? level : 1;
  BattleStats stats = {};  // hp stays 0: derived from level and defense
  stats.atk = entry.bAtk + wildLevel;
  stats.def = entry.bDef + wildLevel;
  stats.spe = entry.bSpe + wildLevel;
  stats.level = wildLevel;
  stats.type1 = entry.type1;
  stats.type2 = entry.type2;
  return stats;
}
#pragma endregion

#pragma region Turn battle
BattleRuntime beginBattleRuntime(const BattleStats &player, const BattleStats &enemy) {
  BattleRuntime battle = {};  // counters, flags and totals start at zero / false
  battle.player = player;
  battle.enemy = enemy;
  battle.playerMaxHp = computeTurnMaxHp(player);
  battle.enemyMaxHp = computeTurnMaxHp(enemy);
  battle.playerHp = battle.playerMaxHp;
  battle.enemyHp = battle.enemyMaxHp;
  battle.restUsesLeft = 2;
  return battle;
}

BattleAction pickEnemyAction(const BattleRuntime &battle, uint8_t choiceRoll) {
  #pragma region Base weights
  int attackWeight = 40;
  int quickWeight = 20;
  int heavyWeight = 20;
  int dodgeWeight = 20;
  #pragma endregion

  #pragma region Adjust to the state of the battle
  if (battle.enemy.spe > battle.player.spe) { quickWeight += 10; attackWeight -= 10; }  // fast species jab
  if (isHpLow(battle.enemyHp, battle.enemyMaxHp)) {  // hurt: play it safe
    dodgeWeight += 10; quickWeight += 10; heavyWeight -= 15; attackWeight -= 5;
  }
  if (isHpLow(battle.playerHp, battle.playerMaxHp)) {  // go for the finish
    heavyWeight += 20; dodgeWeight -= 10; attackWeight -= 10;
  }
  if (battle.counterReady) dodgeWeight += 15;  // the player is primed to strike back hard
  // a primed counter is spent on an attack, and dodges are never chained (no stalling)
  if (battle.enemyCounterReady || battle.enemyLastAction == BATTLE_DODGE) dodgeWeight = 0;
  #pragma endregion

  #pragma region Weighted pick
  const BattleAction moves[] = { BATTLE_ATTACK, BATTLE_ATTACK_QUICK, BATTLE_ATTACK_HEAVY, BATTLE_DODGE };
  int weights[countof(moves)] = { attackWeight, quickWeight, heavyWeight, dodgeWeight };
  int totalWeight = 0;
  for (size_t i = 0; i < countof(weights); i++) {
    if (weights[i] < 0) weights[i] = 0;
    totalWeight += weights[i];
  }
  if (totalWeight == 0) return BATTLE_ATTACK;
  int pick = (int)clampLuckRoll(choiceRoll) * totalWeight / 100;
  for (size_t i = 0; i < countof(weights); i++) {
    if (pick < weights[i]) return moves[i];
    pick -= weights[i];
  }
  return BATTLE_ATTACK;
  #pragma endregion
}

BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll, BattleAction enemyAction) {
  BattleTurnResult turn = {};

  #pragma region Already finished
  if (isBattleFinished(battle)) {
    turn.battleEnded = true;
    turn.playerWon = isPlayerWinner(battle);
    return turn;
  }
  #pragma endregion

  #pragma region Type effects and rest without uses left
  uint8_t luck = clampLuckRoll(luckRoll);
  turn.playerTypePct = battleTypeEffectPct(battle.player.type1, battle.enemy.type1, battle.enemy.type2);
  turn.enemyTypePct = battleTypeEffectPct(battle.enemy.type1, battle.player.type1, battle.player.type2);
  if (action == BATTLE_REST && battle.restUsesLeft == 0) {
    turn.playerRested = true;
    turn.restFailed = true;
    return turn;  // does not cost a round
  }
  #pragma endregion

  if (enemyAction == BATTLE_REST) enemyAction = BATTLE_ATTACK;  // the wild one never rests
  turn.enemyAction = enemyAction;
  battle.enemyLastAction = enemyAction;
  battle.round++;

  #pragma region Player action
  if (action == BATTLE_REST) {
    turn.playerRested = true;
    battle.restUsesLeft--;
    uint16_t heal = (uint16_t)((uint32_t)battle.playerMaxHp * 28 / 100);
    if (heal < 6) heal = 6;
    uint16_t missingHp = battle.playerMaxHp - battle.playerHp;
    turn.playerHeal = heal > missingHp ? missingHp : heal;
    battle.playerHp += turn.playerHeal;
  } else if (isAttackAction(action)) {
    if (enemyAction == BATTLE_DODGE) {
      // a dodge stance replaces the passive dodge roll; a success primes the wild one's counter
      turn.enemyDodged = luck < enemyStanceDodgeChanceFor(action);
      if (turn.enemyDodged) {
        battle.enemyCounterReady = true;
        turn.enemyCounterReady = true;
      }
    } else {
      uint8_t dodgeChance = enemyDodgeChanceFor(battle, action);
      turn.enemyDodged = ((uint16_t)luck + battle.enemy.spe / 2) % 100 < dodgeChance;
    }
    if (!turn.enemyDodged) {
      turn.counterUsed = battle.counterReady;
      turn.playerDamage = computeCappedTurnDamage(battle.player, battle.enemy, battle.enemyMaxHp,
                                                  luck, battle.counterReady, playerPowerPctFor(action, enemyAction));
      battle.counterReady = false;
      applyHitToHp(battle.enemyHp, turn.playerDamage, battle.playerDamageTotal);
    }
  }
  #pragma endregion

  #pragma region Enemy action
  // a dodging wild one does not strike; a passive dodge also uses up its turn,
  // except against a heavy attack, which leaves the player open
  bool enemyActs = enemyAction != BATTLE_DODGE && battle.enemyHp > 0 &&
                   (!turn.enemyDodged || action == BATTLE_ATTACK_HEAVY);
  if (enemyActs) {
    uint16_t enemyHit = computeCappedTurnDamage(battle.enemy, battle.player, battle.playerMaxHp, 99 - luck,
                                                battle.enemyCounterReady, attackPowerPctFor(enemyAction));
    if (action == BATTLE_DODGE) {
      if (luck < playerDodgeChanceFor(enemyAction)) {
        turn.playerDodged = true;
        battle.counterReady = true;
        turn.counterReady = true;
        enemyHit = 0;
      } else {
        enemyHit = enemyHit > 2 ? enemyHit / 3 : 1;  // partial dodge
      }
    } else if (action == BATTLE_ATTACK_QUICK) {
      turn.quickGuard = true;
      enemyHit = scaleByPercentAtLeastOne(enemyHit, 85);
    } else if (action == BATTLE_ATTACK_HEAVY) {
      turn.heavyRisk = true;
      enemyHit = scaleByPercentAtLeastOne(enemyHit, 120);
    } else if (action == BATTLE_REST) {
      turn.playerGuarded = true;
      enemyHit = scaleByPercentAtLeastOne(enemyHit, 70);
    }
    if (enemyHit > 0) {
      turn.enemyCounterUsed = battle.enemyCounterReady;
      battle.enemyCounterReady = false;
      turn.enemyDamage = enemyHit;
      applyHitToHp(battle.playerHp, turn.enemyDamage, battle.enemyDamageTotal);
    }
  }
  #pragma endregion

  #pragma region Outcome
  turn.battleEnded = isBattleFinished(battle);
  turn.playerWon = turn.battleEnded ? isPlayerWinner(battle) : false;
  return turn;
  #pragma endregion
}
#pragma endregion

#pragma region Auto battle
BattleResult resolveBattle(const BattleStats &player,
                           const BattleStats &enemy,
                           const BattleOptions &options) {
  BattleResult result = {};
  result.playerHpLeft = legacyHpFor(player);
  result.enemyHpLeft = legacyHpFor(enemy);

  const uint8_t luck = clampLuckRoll(options.luckRoll);
  const bool playerFirst = player.spe == enemy.spe ? luck >= 50 : player.spe > enemy.spe;
  const uint16_t playerHit = computeDamage(player, enemy, luck);
  const uint16_t enemyHit = computeDamage(enemy, player, 99 - luck);

  #pragma region Rounds
  for (uint8_t round = 0; round < MAX_BATTLE_ROUNDS; round++) {
    result.rounds = round + 1;
    if (playerFirst) {
      applyHitToHp(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
      applyHitToHp(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
    } else {
      applyHitToHp(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
      applyHitToHp(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
    }
  }
  #pragma endregion

  #pragma region Winner
  if (result.enemyHpLeft == 0) {
    result.playerWon = true;
  } else if (result.playerHpLeft == 0) {
    result.playerWon = false;
  } else if (result.enemyHpLeft != result.playerHpLeft) {
    result.playerWon = result.enemyHpLeft < result.playerHpLeft;
  } else {
    result.playerWon = playerFirst;
  }
  #pragma endregion

  return result;
}
#pragma endregion

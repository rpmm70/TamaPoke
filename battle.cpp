// Wild battle logic: pure functions with no hardware dependencies, so the
// whole module can be tested on the host. All randomness comes in as a
// luck roll (0..99) supplied by the caller.

#pragma region Includes
#include "battle.h"
#include "dex.h"
#pragma endregion

namespace {

#pragma region Constants
constexpr uint8_t MAX_BATTLE_ROUNDS = 50;  // resolveBattle() auto battle
constexpr uint8_t MAX_TURN_ROUNDS = 20;    // stepBattle() turn battle
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
#pragma endregion

#pragma region Type chart
// relation of one attacking type against one defending type (modern chart):
// 1 super effective, 0 neutral, -1 not very effective, -2 immune.
// Immunities are softened to a strong resistance by computeTypeEffectPct().
int8_t typeRelationFor(uint8_t attackType, uint8_t defendType) {
  if (attackType == TYPE_NONE || defendType == TYPE_NONE) return 0;
  switch (attackType) {
    case TYPE_NORMAL:
      if (defendType == TYPE_ROCK || defendType == TYPE_STEEL) return -1;
      if (defendType == TYPE_GHOST) return -2;
      return 0;
    case TYPE_FIRE:
      if (defendType == TYPE_BUG || defendType == TYPE_STEEL || defendType == TYPE_GRASS || defendType == TYPE_ICE) return 1;
      if (defendType == TYPE_ROCK || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_WATER:
      if (defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_FIRE) return 1;
      if (defendType == TYPE_WATER || defendType == TYPE_GRASS || defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_ELECTRIC:
      if (defendType == TYPE_FLYING || defendType == TYPE_WATER) return 1;
      if (defendType == TYPE_GRASS || defendType == TYPE_ELECTRIC || defendType == TYPE_DRAGON) return -1;
      if (defendType == TYPE_GROUND) return -2;
      return 0;
    case TYPE_GRASS:
      if (defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_WATER) return 1;
      if (defendType == TYPE_FLYING || defendType == TYPE_POISON || defendType == TYPE_BUG ||
          defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_GRASS ||
          defendType == TYPE_DRAGON) return -1;
      return 0;
    case TYPE_ICE:
      if (defendType == TYPE_FLYING || defendType == TYPE_GROUND || defendType == TYPE_GRASS || defendType == TYPE_DRAGON) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_ICE) return -1;
      return 0;
    case TYPE_FIGHTING:
      if (defendType == TYPE_NORMAL || defendType == TYPE_ROCK || defendType == TYPE_STEEL ||
          defendType == TYPE_ICE || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_FLYING || defendType == TYPE_POISON || defendType == TYPE_BUG ||
          defendType == TYPE_PSYCHIC || defendType == TYPE_FAIRY) return -1;
      if (defendType == TYPE_GHOST) return -2;
      return 0;
    case TYPE_POISON:
      if (defendType == TYPE_GRASS || defendType == TYPE_FAIRY) return 1;
      if (defendType == TYPE_POISON || defendType == TYPE_GROUND || defendType == TYPE_ROCK || defendType == TYPE_GHOST) return -1;
      if (defendType == TYPE_STEEL) return -2;
      return 0;
    case TYPE_GROUND:
      if (defendType == TYPE_POISON || defendType == TYPE_ROCK || defendType == TYPE_STEEL ||
          defendType == TYPE_FIRE || defendType == TYPE_ELECTRIC) return 1;
      if (defendType == TYPE_BUG || defendType == TYPE_GRASS) return -1;
      if (defendType == TYPE_FLYING) return -2;
      return 0;
    case TYPE_FLYING:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_BUG || defendType == TYPE_GRASS) return 1;
      if (defendType == TYPE_ROCK || defendType == TYPE_STEEL || defendType == TYPE_ELECTRIC) return -1;
      return 0;
    case TYPE_PSYCHIC:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_POISON) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_PSYCHIC) return -1;
      if (defendType == TYPE_DARK) return -2;
      return 0;
    case TYPE_BUG:
      if (defendType == TYPE_GRASS || defendType == TYPE_PSYCHIC || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_FLYING || defendType == TYPE_POISON ||
          defendType == TYPE_GHOST || defendType == TYPE_STEEL || defendType == TYPE_FIRE ||
          defendType == TYPE_FAIRY) return -1;
      return 0;
    case TYPE_ROCK:
      if (defendType == TYPE_FLYING || defendType == TYPE_BUG || defendType == TYPE_FIRE || defendType == TYPE_ICE) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_GROUND || defendType == TYPE_STEEL) return -1;
      return 0;
    case TYPE_GHOST:
      if (defendType == TYPE_GHOST || defendType == TYPE_PSYCHIC) return 1;
      if (defendType == TYPE_DARK) return -1;
      if (defendType == TYPE_NORMAL) return -2;
      return 0;
    case TYPE_DRAGON:
      if (defendType == TYPE_DRAGON) return 1;
      if (defendType == TYPE_STEEL) return -1;
      if (defendType == TYPE_FAIRY) return -2;
      return 0;
    case TYPE_DARK:
      if (defendType == TYPE_GHOST || defendType == TYPE_PSYCHIC) return 1;
      if (defendType == TYPE_FIGHTING || defendType == TYPE_DARK || defendType == TYPE_FAIRY) return -1;
      return 0;
    case TYPE_STEEL:
      if (defendType == TYPE_ROCK || defendType == TYPE_ICE || defendType == TYPE_FAIRY) return 1;
      if (defendType == TYPE_STEEL || defendType == TYPE_FIRE || defendType == TYPE_WATER || defendType == TYPE_ELECTRIC) return -1;
      return 0;
    case TYPE_FAIRY:
      if (defendType == TYPE_FIGHTING || defendType == TYPE_DRAGON || defendType == TYPE_DARK) return 1;
      if (defendType == TYPE_POISON || defendType == TYPE_STEEL || defendType == TYPE_FIRE) return -1;
      return 0;
  }
  return 0;
}

// light type multiplier in percent: x1.2 per weakness, x0.85 per resistance,
// x0.7 per immunity, applied once per defending type
uint8_t computeTypeEffectPct(uint8_t attackType, uint8_t defendType1, uint8_t defendType2) {
  uint32_t effectPercent = 100;
  const uint8_t defendTypes[2] = { defendType1, defendType2 };
  for (uint8_t i = 0; i < 2; i++) {
    int8_t relation = typeRelationFor(attackType, defendTypes[i]);
    if (relation > 0) effectPercent = effectPercent * 120 / 100;
    else if (relation == -1) effectPercent = effectPercent * 85 / 100;
    else if (relation == -2) effectPercent = effectPercent * 70 / 100;
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
  damage = (uint16_t)((uint32_t)damage * powerPercent / 100);
  if (damage == 0) damage = 1;
  if (counter) damage = (uint16_t)((uint32_t)damage * 3 / 2);
  uint16_t damageCap = (uint16_t)((uint32_t)defenderMaxHp * 35 / 100);
  if (damageCap < 1) damageCap = 1;
  if (damage > damageCap) damage = damageCap;
  return damage;
}

bool isBattleFinished(const BattleRuntime &battle) {
  return battle.playerHp == 0 || battle.enemyHp == 0 || battle.round >= MAX_TURN_ROUNDS;
}

// at the round limit the side with more HP left wins (ties go to the player)
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
#pragma endregion

#pragma region Wild species helpers
// which types show up more often at each time of day
bool isWildTypePreferredForPhase(uint8_t type1, uint8_t type2, uint8_t phase) {
  auto hasType = [&](uint8_t wantedType) { return type1 == wantedType || type2 == wantedType; };
  switch (phase) {
    case 0: return hasType(TYPE_GRASS) || hasType(TYPE_FLYING) || hasType(TYPE_NORMAL) || hasType(TYPE_BUG);
    case 2: return hasType(TYPE_WATER) || hasType(TYPE_FLYING) || hasType(TYPE_FIRE);
    case 3: return hasType(TYPE_GHOST) || hasType(TYPE_POISON) || hasType(TYPE_BUG);
    default: return true;
  }
}
#pragma endregion

}  // namespace

#pragma region Day phase and level helpers
uint8_t dayPhaseFromHour(int hour) {
  if (hour >= 6 && hour < 12) return 0;
  if (hour >= 12 && hour < 18) return 1;
  if (hour >= 18 && hour < 22) return 2;
  return 3;
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
  return !isEgg && !sleeping && ceremony == 0;
}

uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll) {
  int baseLevel = (int)(petLevel ? petLevel : 1);
  int levelDelta;
  if (luckRoll < 55) levelDelta = (int)(luckRoll % 3) - 1;       // -1..+1, most common
  else if (luckRoll < 85) levelDelta = -2 - (int)(luckRoll % 3); // -2..-4, fair catch-up fights
  else levelDelta = 2 + (int)(luckRoll % 2);                     // +2..+3, occasional danger
  int wildLevel = baseLevel + levelDelta;
  if (wildLevel < 1) wildLevel = 1;
  return wildLevel > 100 ? 100 : (uint8_t)wildLevel;
}

// 25% rare, otherwise common (never legendary or evolution-only); prefers the
// types of the day phase and falls back to any species of the rarity
int16_t pickWildSpecies(uint8_t roll, uint8_t phase) {
  int16_t candidatePool[DEX_COUNT];
  int candidateCount = 0;
  uint8_t targetRarity = (roll % 100) < 25 ? R_RARO : R_COMUN;

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
  if (candidateCount == 0 && targetRarity == R_RARO) {
    fillPoolForRarity(R_COMUN, true);
    if (candidateCount == 0) fillPoolForRarity(R_COMUN, false);
  }
  return candidateCount > 0 ? candidatePool[roll % candidateCount] : 1;
}

BattleStats wildBattleStats(int16_t dex, uint8_t level) {
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  const DexEntry &entry = DEX_TBL[dex];
  uint8_t wildLevel = level ? level : 1;
  BattleStats stats = {};
  stats.hp = 0;
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
  BattleRuntime battle = {};
  battle.player = player;
  battle.enemy = enemy;
  battle.playerMaxHp = computeTurnMaxHp(player);
  battle.enemyMaxHp = computeTurnMaxHp(enemy);
  battle.playerHp = battle.playerMaxHp;
  battle.enemyHp = battle.enemyMaxHp;
  battle.restUsesLeft = 2;
  battle.counterReady = false;
  return battle;
}

BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll) {
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
    uint8_t dodgeChance = enemyDodgeChanceFor(battle, action);
    turn.enemyDodged = ((uint16_t)luck + battle.enemy.spe / 2) % 100 < dodgeChance;
    if (!turn.enemyDodged) {
      turn.counterUsed = battle.counterReady;
      turn.playerDamage = computeCappedTurnDamage(battle.player, battle.enemy, battle.enemyMaxHp,
                                                  luck, battle.counterReady, attackPowerPctFor(action));
      battle.counterReady = false;
      applyHitToHp(battle.enemyHp, turn.playerDamage, battle.playerDamageTotal);
    }
  }
  #pragma endregion

  #pragma region Enemy action
  // a dodged heavy attack leaves the player open: the enemy still strikes
  bool enemyActs = battle.enemyHp > 0 && (!turn.enemyDodged || action == BATTLE_ATTACK_HEAVY);
  if (enemyActs) {
    uint16_t enemyHit = computeCappedTurnDamage(battle.enemy, battle.player, battle.playerMaxHp, 99 - luck, false, 100);
    if (action == BATTLE_DODGE) {
      if (luck < 85) {
        turn.playerDodged = true;
        battle.counterReady = true;
        turn.counterReady = true;
        enemyHit = 0;
      } else {
        enemyHit = enemyHit > 2 ? enemyHit / 3 : 1;  // partial dodge
      }
    } else if (action == BATTLE_ATTACK_QUICK) {
      turn.quickGuard = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 85 / 100);
      if (enemyHit == 0) enemyHit = 1;
    } else if (action == BATTLE_ATTACK_HEAVY) {
      turn.heavyRisk = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 120 / 100);
      if (enemyHit == 0) enemyHit = 1;
    } else if (action == BATTLE_REST) {
      turn.playerGuarded = true;
      enemyHit = (uint16_t)((uint32_t)enemyHit * 70 / 100);
      if (enemyHit == 0) enemyHit = 1;
    }
    if (enemyHit > 0) {
      turn.enemyDamage = enemyHit;
      applyHitToHp(battle.playerHp, turn.enemyDamage, battle.enemyDamageTotal);
    }
  }
  #pragma endregion

  turn.battleEnded = isBattleFinished(battle);
  turn.playerWon = turn.battleEnded ? isPlayerWinner(battle) : false;
  return turn;
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

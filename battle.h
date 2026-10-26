#pragma once

#pragma region Includes
#include <stdint.h>
#pragma endregion

#pragma region Constants
// pet.level() goes up to 999 on this port; battles always use
// min(level, BATTLE_LEVEL_CAP) so the battle math stays in uint8_t range
#define BATTLE_LEVEL_CAP 100
#pragma endregion

#pragma region Data structures
struct BattleStats {
  uint16_t hp;      // 0 = derive from level and defense
  uint16_t atk;
  uint16_t def;
  uint16_t spe;
  uint8_t level;
  uint8_t type1 = 0;  // TYPE_* from dex.h
  uint8_t type2 = 0;  // TYPE_NONE if there is no secondary type
};

struct BattleOptions {
  // 0..99. Keeps ties and damage variation deterministic in tests.
  uint8_t luckRoll;
};

struct BattleResult {
  bool playerWon;
  uint8_t rounds;
  uint16_t playerDamage;
  uint16_t enemyDamage;
  uint16_t playerHpLeft;
  uint16_t enemyHpLeft;
};

enum BattleAction : uint8_t {
  BATTLE_ATTACK = 0,
  BATTLE_ATTACK_QUICK,
  BATTLE_ATTACK_HEAVY,
  BATTLE_DODGE,
  BATTLE_REST,
};

// state of a turn-based battle driven by the player's choices
struct BattleRuntime {
  BattleStats player;
  BattleStats enemy;
  uint16_t playerHp;
  uint16_t enemyHp;
  uint16_t playerMaxHp;
  uint16_t enemyMaxHp;
  uint8_t round;
  uint16_t playerDamageTotal;
  uint16_t enemyDamageTotal;
  uint8_t restUsesLeft;
  bool counterReady;
  bool enemyCounterReady;        // the wild one dodged an attack: its next hit lands harder
  BattleAction enemyLastAction;  // stops the wild one from chaining dodges
};

// what happened in one stepBattle() call (for the UI log and effects)
struct BattleTurnResult {
  uint16_t playerDamage;
  uint16_t enemyDamage;
  uint16_t playerHeal;
  bool playerDodged;
  bool enemyDodged;
  bool playerRested;
  bool playerGuarded;
  bool quickGuard;
  bool heavyRisk;
  bool counterReady;
  bool counterUsed;
  BattleAction enemyAction;  // move the wild one played (never BATTLE_REST)
  bool enemyCounterReady;
  bool enemyCounterUsed;
  bool restFailed;
  bool battleEnded;
  bool playerWon;
  uint8_t playerTypePct;
  uint8_t enemyTypePct;
};
#pragma endregion

#pragma region Public API
// 0 morning 6-12, 1 day 12-18, 2 evening 18-22, 3 night 22-6
uint8_t dayPhaseFromHour(int hour);
// clamps the pet level (up to 999) to the battle range (BATTLE_LEVEL_CAP)
uint8_t battleLevelFor(uint16_t petLevel);
// max HP a turn battle starts with for these stats (same formula as beginBattleRuntime)
uint16_t battleMaxHpFor(const BattleStats &stats);

bool canStartWildBattle(bool isEgg, bool sleeping, uint8_t ceremony);
uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll);
int16_t pickWildSpecies(uint8_t roll, uint8_t phase = 1);
BattleStats wildBattleStats(int16_t dex, uint8_t level);
uint8_t battleTypeEffectPct(uint8_t attackType, uint8_t defendType1, uint8_t defendType2);
BattleRuntime beginBattleRuntime(const BattleStats &player, const BattleStats &enemy);
// move the wild one plays this turn (attack, quick, heavy or dodge), weighted by
// the state of the battle; choiceRoll is 0..99
BattleAction pickEnemyAction(const BattleRuntime &battle, uint8_t choiceRoll);
// enemyAction comes from pickEnemyAction(); BATTLE_REST is played as BATTLE_ATTACK
BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll,
                            BattleAction enemyAction = BATTLE_ATTACK);

BattleResult resolveBattle(const BattleStats &player,
                           const BattleStats &enemy,
                           const BattleOptions &options);
#pragma endregion

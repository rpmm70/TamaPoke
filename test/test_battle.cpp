// Tests of the wild battle logic (battle.cpp), ported from the fork's
// tests/battle_tests.cpp. battle.cpp is pure logic: no shims needed beyond dex.h.

#pragma region Includes
#include "framework.h"
#include "shim/Arduino.h"
#include "../battle.h"
#include "../dex.h"
#pragma endregion

#pragma region Helpers
static bool dexEntryHasType(const DexEntry &entry, uint8_t type) {
  return entry.type1 == type || entry.type2 == type;
}
#pragma endregion

#pragma region Auto battle (resolveBattle)
TEST(battle, stronger_player_wins) {
  BattleStats player = { 0, 90, 70, 80, 20 };
  BattleStats enemy = { 0, 45, 35, 40, 18 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  CHECK(result.playerWon);
  CHECK(result.playerHpLeft > 0);
  CHECK_EQ(result.enemyHpLeft, (uint16_t)0);
  CHECK(result.playerDamage > result.enemyDamage);
}

TEST(battle, stronger_enemy_wins) {
  BattleStats player = { 0, 35, 30, 30, 12 };
  BattleStats enemy = { 0, 95, 75, 80, 20 };

  BattleResult result = resolveBattle(player, enemy, { 40 });

  CHECK(!result.playerWon);
  CHECK_EQ(result.playerHpLeft, (uint16_t)0);
  CHECK(result.enemyHpLeft > 0);
}

TEST(battle, faster_side_hits_first) {
  BattleStats slowPlayer = { 8, 100, 40, 20, 20 };
  BattleStats fastEnemy = { 8, 100, 40, 90, 20 };

  BattleResult result = resolveBattle(slowPlayer, fastEnemy, { 80 });

  CHECK(!result.playerWon);
  CHECK_EQ(result.playerHpLeft, (uint16_t)0);
  CHECK(result.enemyHpLeft > 0);
  CHECK_EQ(result.playerDamage, (uint16_t)0);
}

TEST(battle, speed_tie_uses_luck_roll) {
  BattleStats player = { 8, 100, 40, 50, 20 };
  BattleStats enemy = { 8, 100, 40, 50, 20 };

  BattleResult playerFirst = resolveBattle(player, enemy, { 50 });
  BattleResult enemyFirst = resolveBattle(player, enemy, { 49 });

  CHECK(playerFirst.playerWon);
  CHECK(!enemyFirst.playerWon);
}

TEST(battle, minimum_damage_prevents_stall) {
  BattleStats player = { 12, 1, 250, 50, 1 };
  BattleStats enemy = { 12, 1, 250, 40, 1 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  CHECK(result.playerWon);
  CHECK_EQ(result.rounds, (uint8_t)12);
  CHECK_EQ(result.playerDamage, (uint16_t)12);
  CHECK_EQ(result.enemyDamage, (uint16_t)11);
}

TEST(battle, round_limit_ends_defensive_case) {
  BattleStats player = { 1000, 1, 250, 50, 1 };
  BattleStats enemy = { 1000, 1, 250, 40, 1 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  CHECK(result.playerWon);
  CHECK_EQ(result.rounds, (uint8_t)50);
  CHECK_EQ(result.playerDamage, (uint16_t)50);
  CHECK_EQ(result.enemyDamage, (uint16_t)50);
  CHECK_EQ(result.playerHpLeft, (uint16_t)950);
  CHECK_EQ(result.enemyHpLeft, (uint16_t)950);
}

TEST(battle, derived_hp_uses_level_and_defense) {
  BattleStats stats = { 0, 80, 64, 40, 10 };
  BattleStats weakEnemy = { 1, 1, 1, 1, 1 };

  BattleResult result = resolveBattle(stats, weakEnemy, { 50 });

  CHECK_EQ(result.playerHpLeft, (uint16_t)80);
}
#pragma endregion

#pragma region Wild encounter
TEST(battle, wild_battle_start_gate) {
  CHECK(canStartWildBattle(false, false, 0));
  CHECK(!canStartWildBattle(true, false, 0));
  CHECK(!canStartWildBattle(false, true, 0));
  CHECK(!canStartWildBattle(false, false, 1));
}

TEST(battle, wild_level_stays_near_pet_level) {
  CHECK_EQ(wildLevelFor(1, 0), (uint8_t)1);
  CHECK_EQ(wildLevelFor(10, 0), (uint8_t)9);
  CHECK_EQ(wildLevelFor(10, 1), (uint8_t)10);
  CHECK_EQ(wildLevelFor(10, 2), (uint8_t)11);
  CHECK_EQ(wildLevelFor(10, 55), (uint8_t)7);
  CHECK_EQ(wildLevelFor(10, 56), (uint8_t)6);
  CHECK_EQ(wildLevelFor(10, 57), (uint8_t)8);
  CHECK_EQ(wildLevelFor(10, 85), (uint8_t)13);
  CHECK_EQ(wildLevelFor(10, 86), (uint8_t)12);
}

TEST(battle, wild_level_never_exceeds_100) {
  CHECK_EQ(wildLevelFor(100, 85), (uint8_t)100);
  CHECK_EQ(wildLevelFor(battleLevelFor(999), 86), (uint8_t)100);
}

TEST(battle, wild_species_uses_hatchable_non_legendary_pool) {
  for (int roll = 0; roll < 100; roll++) {
    for (uint8_t phase = 0; phase < 4; phase++) {
      int16_t dex = pickWildSpecies((uint8_t)roll, phase);
      CHECK_RANGE(dex, 1, DEX_COUNT);
      CHECK(DEX_TBL[dex].rarity == R_COMMON || DEX_TBL[dex].rarity == R_RARE);
    }
  }
}

TEST(battle, night_wild_species_prefer_ghost_poison_bug) {
  for (int roll = 0; roll < 100; roll++) {
    int16_t dex = pickWildSpecies((uint8_t)roll, 3);
    CHECK_RANGE(dex, 1, DEX_COUNT);
    const DexEntry &entry = DEX_TBL[dex];
    CHECK(entry.rarity == R_COMMON || entry.rarity == R_RARE);
    CHECK_MSG(dexEntryHasType(entry, TYPE_GHOST) || dexEntryHasType(entry, TYPE_POISON) ||
              dexEntryHasType(entry, TYPE_BUG), DEX_TBL[dex].name);
  }
}

TEST(battle, morning_wild_species_prefer_grass_flying_normal_bug) {
  for (int roll = 0; roll < 100; roll++) {
    int16_t dex = pickWildSpecies((uint8_t)roll, 0);
    const DexEntry &entry = DEX_TBL[dex];
    CHECK_MSG(dexEntryHasType(entry, TYPE_GRASS) || dexEntryHasType(entry, TYPE_FLYING) ||
              dexEntryHasType(entry, TYPE_NORMAL) || dexEntryHasType(entry, TYPE_BUG),
              DEX_TBL[dex].name);
  }
}

TEST(battle, wild_stats_use_dex_base_and_level) {
  BattleStats stats = wildBattleStats(4, 7);  // CHARMANDER
  CHECK_EQ(stats.level, (uint8_t)7);
  CHECK_EQ(stats.atk, (uint16_t)(DEX_TBL[4].bAtk + 7));
  CHECK_EQ(stats.def, (uint16_t)(DEX_TBL[4].bDef + 7));
  CHECK_EQ(stats.spe, (uint16_t)(DEX_TBL[4].bSpe + 7));
  CHECK_EQ(stats.type1, (uint8_t)TYPE_FIRE);
  CHECK_EQ(stats.type2, (uint8_t)TYPE_NONE);
}

TEST(battle, wild_stats_out_of_range_dex_falls_back_to_1) {
  BattleStats stats = wildBattleStats(0, 0);
  CHECK_EQ(stats.level, (uint8_t)1);
  CHECK_EQ(stats.atk, (uint16_t)(DEX_TBL[1].bAtk + 1));
}
#pragma endregion

#pragma region Day phase and level cap
TEST(battle, day_phase_from_hour) {
  CHECK_EQ(dayPhaseFromHour(5), (uint8_t)3);
  CHECK_EQ(dayPhaseFromHour(6), (uint8_t)0);
  CHECK_EQ(dayPhaseFromHour(11), (uint8_t)0);
  CHECK_EQ(dayPhaseFromHour(12), (uint8_t)1);
  CHECK_EQ(dayPhaseFromHour(17), (uint8_t)1);
  CHECK_EQ(dayPhaseFromHour(18), (uint8_t)2);
  CHECK_EQ(dayPhaseFromHour(21), (uint8_t)2);
  CHECK_EQ(dayPhaseFromHour(22), (uint8_t)3);
  CHECK_EQ(dayPhaseFromHour(0), (uint8_t)3);
}

TEST(battle, battle_level_is_capped_at_100) {
  CHECK_EQ(battleLevelFor(1), (uint8_t)1);
  CHECK_EQ(battleLevelFor(100), (uint8_t)100);
  CHECK_EQ(battleLevelFor(101), (uint8_t)BATTLE_LEVEL_CAP);
  CHECK_EQ(battleLevelFor(250), (uint8_t)100);
  CHECK_EQ(battleLevelFor(999), (uint8_t)100);
}
#pragma endregion

#pragma region Turn battle (stepBattle)
TEST(battle, runtime_starts_with_scaled_hp) {
  BattleRuntime battle = beginBattleRuntime({ 0, 60, 40, 50, 10 },
                                            { 0, 58, 42, 48, 10 });

  CHECK_EQ(battle.round, (uint16_t)0);
  CHECK_EQ(battle.playerMaxHp, (uint16_t)(30 + 10 * 5 + 40));
  CHECK_EQ(battle.enemyMaxHp, (uint16_t)(30 + 10 * 5 + 42));
  CHECK_EQ(battle.playerHp, battle.playerMaxHp);
  CHECK_EQ(battle.enemyHp, battle.enemyMaxHp);
  CHECK_EQ(battle.restUsesLeft, (uint8_t)2);
  CHECK(!battle.counterReady);
}

// the info card shows battleMaxHpFor(): it must match the HP a battle starts with
TEST(battle, max_hp_wrapper_matches_runtime_start) {
  BattleStats player = { 0, 60, 40, 50, 10 };
  BattleStats enemy = { 0, 58, 42, 48, 10 };
  BattleRuntime battle = beginBattleRuntime(player, enemy);
  CHECK_EQ(battleMaxHpFor(player), battle.playerMaxHp);
  CHECK_EQ(battleMaxHpFor(enemy), battle.enemyMaxHp);
  BattleStats levelZero = { 0, 0, 0, 0, 0 };
  CHECK_EQ(battleMaxHpFor(levelZero), (uint16_t)(30 + 1 * 5));
}

TEST(battle, attack_steps_one_round_and_avoids_normal_one_hit) {
  BattleRuntime battle = beginBattleRuntime({ 0, 120, 55, 70, 20 },
                                            { 0, 45, 45, 40, 19 });

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50);

  CHECK_EQ(battle.round, (uint16_t)1);
  CHECK(turn.playerDamage > 0);
  CHECK(battle.enemyHp > 0);
  CHECK(!turn.battleEnded);
}

TEST(battle, dodge_prevents_enemy_damage_deterministically) {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 40, 60, 10 },
                                            { 0, 90, 40, 50, 10 });
  uint16_t hpBefore = battle.playerHp;

  BattleTurnResult turn = stepBattle(battle, BATTLE_DODGE, 20);

  CHECK(turn.playerDodged);
  CHECK_EQ(turn.enemyDamage, (uint16_t)0);
  CHECK_EQ(battle.playerHp, hpBefore);
  CHECK(turn.counterReady);
  CHECK(battle.counterReady);
}

TEST(battle, dodge_partial_failure_still_mitigates_damage) {
  BattleStats player = { 0, 50, 40, 60, 10 };
  BattleStats enemy = { 0, 90, 40, 50, 10 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime dodged = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 95);
  BattleTurnResult dodgeTurn = stepBattle(dodged, BATTLE_DODGE, 95);

  CHECK(!dodgeTurn.playerDodged);
  CHECK(dodgeTurn.enemyDamage > 0);
  CHECK(dodgeTurn.enemyDamage < normalTurn.enemyDamage);
  CHECK(dodgeTurn.enemyDamage <= normalTurn.enemyDamage / 3 + 1);
}

TEST(battle, rest_heals_only_to_max_hp_and_consumes_use) {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 60, 50, 10 },
                                            { 0, 1, 40, 40, 10 });
  battle.playerHp = battle.playerMaxHp - 3;

  BattleTurnResult turn = stepBattle(battle, BATTLE_REST, 50);

  CHECK(turn.playerRested);
  CHECK_EQ(turn.playerHeal, (uint16_t)3);
  CHECK_EQ(battle.restUsesLeft, (uint8_t)1);
  CHECK(battle.playerHp <= battle.playerMaxHp);
  CHECK(battle.playerHp >= battle.playerMaxHp - 1);
}

TEST(battle, rest_heals_more_and_guards_incoming_damage) {
  BattleStats player = { 0, 50, 60, 50, 10 };
  BattleStats enemy = { 0, 120, 40, 40, 10 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime resting = beginBattleRuntime(player, enemy);
  resting.playerHp = resting.playerMaxHp / 2;

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult restTurn = stepBattle(resting, BATTLE_REST, 50);

  CHECK(restTurn.playerRested);
  CHECK(restTurn.playerGuarded);
  CHECK(restTurn.playerHeal >= resting.playerMaxHp / 4);
  CHECK(restTurn.enemyDamage < normalTurn.enemyDamage);
}

TEST(battle, rest_is_limited_to_two_uses) {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 60, 50, 10 },
                                            { 0, 1, 40, 40, 10 });
  battle.playerHp = battle.playerMaxHp / 2;

  BattleTurnResult first = stepBattle(battle, BATTLE_REST, 50);
  battle.playerHp = battle.playerMaxHp / 2;
  BattleTurnResult second = stepBattle(battle, BATTLE_REST, 50);
  battle.playerHp = battle.playerMaxHp / 2;
  BattleTurnResult third = stepBattle(battle, BATTLE_REST, 50);

  CHECK(first.playerHeal > 0);
  CHECK(second.playerHeal > 0);
  CHECK(third.restFailed);
  CHECK_EQ(third.playerHeal, (uint16_t)0);
  CHECK_EQ(battle.restUsesLeft, (uint8_t)0);
  CHECK_EQ(third.enemyDamage, (uint16_t)0);
  CHECK_MSG(battle.round == 2, "a failed rest does not cost a round");
}

TEST(battle, counter_boosts_next_attack_damage) {
  BattleStats player = { 0, 80, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime counter = beginBattleRuntime(player, enemy);
  counter.counterReady = true;

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult counterTurn = stepBattle(counter, BATTLE_ATTACK, 50);

  CHECK(counterTurn.counterUsed);
  CHECK(!counter.counterReady);
  CHECK(counterTurn.playerDamage > normalTurn.playerDamage);
  CHECK_EQ(counterTurn.playerDamage, (uint16_t)(normalTurn.playerDamage * 3 / 2));
}

TEST(battle, quick_and_heavy_attack_damage_tradeoff) {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 50);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK_HEAVY, 50);

  CHECK(quickTurn.playerDamage < normalTurn.playerDamage);
  CHECK(heavyTurn.playerDamage > normalTurn.playerDamage);
}

TEST(battle, heavy_attack_can_be_dodged_when_quick_would_hit) {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 0);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK_HEAVY, 0);

  CHECK(!quickTurn.enemyDodged);
  CHECK(heavyTurn.enemyDodged);
  CHECK(quickTurn.playerDamage > 0);
  CHECK_EQ(heavyTurn.playerDamage, (uint16_t)0);
  CHECK(heavyTurn.enemyDamage > 0);
  CHECK(heavyTurn.heavyRisk);
}

TEST(battle, quick_attack_reduces_incoming_damage_risk) {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 100, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime quick = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 50);

  CHECK(quickTurn.quickGuard);
  CHECK(quickTurn.enemyDamage < normalTurn.enemyDamage);
}

TEST(battle, counter_damage_still_uses_turn_cap) {
  BattleRuntime battle = beginBattleRuntime({ 0, 650, 60, 70, 30 },
                                            { 0, 10, 20, 20, 30 });
  battle.counterReady = true;

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50);
  uint16_t damageCap = (uint16_t)((uint32_t)battle.enemyMaxHp * 35 / 100);
  if (damageCap < 1) damageCap = 1;

  CHECK(turn.counterUsed);
  CHECK(turn.playerDamage <= damageCap);
  CHECK(battle.enemyHp > 0);
}

TEST(battle, turn_battle_has_no_round_limit) {
  BattleRuntime battle = beginBattleRuntime({ 0, 1, 250, 50, 1 },
                                            { 0, 1, 250, 40, 1 });

  BattleTurnResult turn = {};
  for (int i = 0; i < 20; i++) {
    turn = stepBattle(battle, BATTLE_DODGE, 20);
  }

  CHECK_MSG(!turn.battleEnded, "20 rounds with both sides standing no longer ends the battle");
  CHECK(battle.playerHp > 0);
  CHECK(battle.enemyHp > 0);
  CHECK_EQ(battle.round, (uint16_t)20);
}

TEST(battle, round_counter_does_not_wrap_past_255) {
  BattleRuntime battle = beginBattleRuntime({ 0, 1, 250, 50, 1 },
                                            { 0, 1, 250, 40, 1 });
  battle.round = 300;

  BattleTurnResult turn = stepBattle(battle, BATTLE_DODGE, 20);

  CHECK(!turn.battleEnded);
  CHECK_EQ(battle.round, (uint16_t)301);
}

TEST(battle, step_after_end_does_nothing) {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 40, 50, 10 },
                                            { 0, 50, 40, 50, 10 });
  battle.enemyHp = 0;
  uint16_t hpBefore = battle.playerHp;

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50);

  CHECK(turn.battleEnded);
  CHECK(turn.playerWon);
  CHECK_EQ(battle.round, (uint16_t)0);
  CHECK_EQ(battle.playerHp, hpBefore);
}
#pragma endregion

#pragma region Wild one's moves (pickEnemyAction and the enemyAction of stepBattle)
TEST(battle, enemy_dodge_stance_avoids_attack_and_primes_counter) {
  BattleRuntime battle = beginBattleRuntime({ 0, 90, 45, 70, 20 },
                                            { 0, 50, 35, 40, 20 });
  uint16_t enemyHpBefore = battle.enemyHp;

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 10, BATTLE_DODGE);

  CHECK(turn.enemyDodged);
  CHECK_EQ(turn.enemyAction, BATTLE_DODGE);
  CHECK_EQ(turn.playerDamage, (uint16_t)0);
  CHECK_EQ(turn.enemyDamage, (uint16_t)0);
  CHECK_EQ(battle.enemyHp, enemyHpBefore);
  CHECK(turn.enemyCounterReady);
  CHECK(battle.enemyCounterReady);
}

TEST(battle, enemy_failed_dodge_takes_full_hit_and_does_not_strike) {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime stance = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 90);
  BattleTurnResult stanceTurn = stepBattle(stance, BATTLE_ATTACK, 90, BATTLE_DODGE);

  CHECK(!stanceTurn.enemyDodged);
  CHECK(!stance.enemyCounterReady);
  CHECK_EQ(stanceTurn.playerDamage, normalTurn.playerDamage);
  CHECK_EQ(stanceTurn.enemyDamage, (uint16_t)0);
}

TEST(battle, enemy_dodge_stance_reads_heavy_better_than_quick) {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 50, BATTLE_DODGE);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK_HEAVY, 50, BATTLE_DODGE);

  CHECK(!quickTurn.enemyDodged);
  CHECK(quickTurn.playerDamage > 0);
  CHECK(heavyTurn.enemyDodged);
  CHECK_EQ(heavyTurn.playerDamage, (uint16_t)0);
  CHECK_EQ(heavyTurn.enemyDamage, (uint16_t)0);
}

TEST(battle, enemy_attack_variants_trade_power_for_openings) {
  BattleStats player = { 0, 80, 45, 50, 20 };
  BattleStats enemy = { 0, 60, 45, 50, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50, BATTLE_ATTACK);
  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK, 50, BATTLE_ATTACK_QUICK);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK, 50, BATTLE_ATTACK_HEAVY);

  CHECK(quickTurn.enemyDamage < normalTurn.enemyDamage);
  CHECK(heavyTurn.enemyDamage > normalTurn.enemyDamage);
  CHECK_MSG(heavyTurn.playerDamage > normalTurn.playerDamage, "a heavy wind-up leaves the wild one open");
  CHECK_EQ(quickTurn.playerDamage, normalTurn.playerDamage);
}

TEST(battle, enemy_quick_attack_is_harder_to_dodge) {
  BattleStats player = { 0, 50, 40, 60, 10 };
  BattleStats enemy = { 0, 90, 40, 50, 10 };
  BattleRuntime versusNormal = beginBattleRuntime(player, enemy);
  BattleRuntime versusQuick = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(versusNormal, BATTLE_DODGE, 75, BATTLE_ATTACK);
  BattleTurnResult quickTurn = stepBattle(versusQuick, BATTLE_DODGE, 75, BATTLE_ATTACK_QUICK);

  CHECK(normalTurn.playerDodged);
  CHECK_EQ(normalTurn.enemyDamage, (uint16_t)0);
  CHECK(!quickTurn.playerDodged);
  CHECK(quickTurn.enemyDamage > 0);
}

TEST(battle, enemy_counter_boosts_its_next_hit) {
  BattleStats player = { 0, 80, 45, 50, 20 };
  BattleStats enemy = { 0, 60, 45, 50, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime counter = beginBattleRuntime(player, enemy);
  counter.enemyCounterReady = true;

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50, BATTLE_ATTACK);
  BattleTurnResult counterTurn = stepBattle(counter, BATTLE_ATTACK, 50, BATTLE_ATTACK);

  CHECK(counterTurn.enemyCounterUsed);
  CHECK(!counter.enemyCounterReady);
  CHECK_EQ(counterTurn.enemyDamage, (uint16_t)(normalTurn.enemyDamage * 3 / 2));
}

TEST(battle, enemy_rest_is_played_as_attack) {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 40, 50, 10 },
                                            { 0, 50, 40, 50, 10 });

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50, BATTLE_REST);

  CHECK_EQ(turn.enemyAction, BATTLE_ATTACK);
  CHECK_EQ(battle.enemyLastAction, BATTLE_ATTACK);
}

TEST(battle, enemy_picks_every_move_over_the_roll_range) {
  BattleRuntime battle = beginBattleRuntime({ 0, 90, 45, 70, 20 },
                                            { 0, 50, 35, 40, 20 });
  bool seenMove[5] = { false, false, false, false, false };
  for (int roll = 0; roll < 100; roll++) seenMove[pickEnemyAction(battle, (uint8_t)roll)] = true;

  CHECK(seenMove[BATTLE_ATTACK]);
  CHECK(seenMove[BATTLE_ATTACK_QUICK]);
  CHECK(seenMove[BATTLE_ATTACK_HEAVY]);
  CHECK(seenMove[BATTLE_DODGE]);
  CHECK(!seenMove[BATTLE_REST]);
}

TEST(battle, enemy_never_chains_dodges_or_dodges_with_counter_ready) {
  BattleRuntime afterDodge = beginBattleRuntime({ 0, 90, 45, 70, 20 },
                                                { 0, 50, 35, 40, 20 });
  afterDodge.enemyLastAction = BATTLE_DODGE;
  BattleRuntime withCounter = afterDodge;
  withCounter.enemyLastAction = BATTLE_ATTACK;
  withCounter.enemyCounterReady = true;

  for (int roll = 0; roll < 100; roll++) {
    CHECK(pickEnemyAction(afterDodge, (uint8_t)roll) != BATTLE_DODGE);
    CHECK(pickEnemyAction(withCounter, (uint8_t)roll) != BATTLE_DODGE);
  }
}

TEST(battle, enemy_goes_heavy_when_player_hp_is_low) {
  BattleRuntime fullHp = beginBattleRuntime({ 0, 90, 45, 70, 20 },
                                            { 0, 50, 35, 40, 20 });
  BattleRuntime lowHp = fullHp;
  lowHp.playerHp = lowHp.playerMaxHp / 5;
  int fullHpHeavyCount = 0, lowHpHeavyCount = 0;
  for (int roll = 0; roll < 100; roll++) {
    if (pickEnemyAction(fullHp, (uint8_t)roll) == BATTLE_ATTACK_HEAVY) fullHpHeavyCount++;
    if (pickEnemyAction(lowHp, (uint8_t)roll) == BATTLE_ATTACK_HEAVY) lowHpHeavyCount++;
  }

  CHECK(lowHpHeavyCount > fullHpHeavyCount);
}
#pragma endregion

#pragma region Type chart
TEST(battle, type_advantage_lightly_changes_damage) {
  BattleStats neutral = { 0, 80, 50, 50, 20, TYPE_NORMAL, TYPE_NONE };
  BattleStats fire = { 0, 80, 50, 50, 20, TYPE_FIRE, TYPE_NONE };
  BattleStats grass = { 0, 80, 50, 50, 20, TYPE_GRASS, TYPE_NONE };

  BattleRuntime normalVsGrass = beginBattleRuntime(neutral, grass);
  BattleRuntime fireVsGrass = beginBattleRuntime(fire, grass);
  BattleRuntime grassVsFire = beginBattleRuntime(grass, fire);

  BattleTurnResult neutralTurn = stepBattle(normalVsGrass, BATTLE_ATTACK, 50);
  BattleTurnResult fireTurn = stepBattle(fireVsGrass, BATTLE_ATTACK, 50);
  BattleTurnResult resistedTurn = stepBattle(grassVsFire, BATTLE_ATTACK, 50);

  CHECK(fireTurn.playerDamage > neutralTurn.playerDamage);
  CHECK(resistedTurn.playerDamage < neutralTurn.playerDamage);
}

TEST(battle, type_effect_helper_handles_dual_types) {
  CHECK_EQ(battleTypeEffectPct(TYPE_FIRE, TYPE_GRASS, TYPE_NONE), (uint8_t)120);
  CHECK_EQ(battleTypeEffectPct(TYPE_GRASS, TYPE_FIRE, TYPE_NONE), (uint8_t)85);
  CHECK_EQ(battleTypeEffectPct(TYPE_FIRE, TYPE_GRASS, TYPE_ICE), (uint8_t)144);
  CHECK_EQ(battleTypeEffectPct(TYPE_NORMAL, TYPE_GHOST, TYPE_NONE), (uint8_t)70);
  CHECK_EQ(battleTypeEffectPct(TYPE_NONE, TYPE_GHOST, TYPE_NONE), (uint8_t)100);
}

TEST(battle, single_type_effect_stays_between_70_and_120) {
  for (uint8_t attackType = 0; attackType < TYPE_COUNT; attackType++)
    for (uint8_t defendType = 0; defendType < TYPE_COUNT; defendType++)
      CHECK_RANGE(battleTypeEffectPct(attackType, defendType, TYPE_NONE), 70, 120);
}
#pragma endregion

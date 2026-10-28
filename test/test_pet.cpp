// Tests of the pet logic (pet.cpp) on top of the host shims.

#pragma region Includes
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../dex.h"
#include "../battle.h"  // battleLevelFor
#include "../nvs_settings.h"  // NVS_NAMESPACE
#include <string.h>
#pragma endregion

#pragma region Helpers
static void advance(Pet &pet, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    pet.update(millis());
  }
}

// freshly hatched pet of the requested species, with a clean NVS
static void makePet(Pet &pet, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  pet.begin();
  if (pet.awaitingStarter()) pet.chooseStarter(dex);
  pet.eggTap();
  pet.eggTap();
  pet.eggTap();  // 3 taps = immediate hatching
}

static void setStats(Pet &pet, uint8_t fullness, uint8_t joy, uint8_t energy, uint8_t hygiene) {
  pet.fullness = fullness; pet.joy = joy; pet.energy = energy; pet.hygiene = hygiene;
}

static void markRegistered(Pet &pet, int16_t dex) {
  pet.dexReg[(dex - 1) >> 3] |= (uint8_t)(1 << ((dex - 1) & 7));
}
#pragma endregion

#pragma region Egg
TEST(egg, first_game_asks_for_a_starter) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK(p.awaitingStarter());
  CHECK_EQ(p.eggCracks(), (uint8_t)0);
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

// v1.3 regression: the egg hatched on its own after 3 min while the player
// was still picking a starter, and the choice was lost
TEST(egg, does_not_hatch_while_picking_a_starter) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.awaitingStarter());
  advance(p, 600);  // 10 hours of play
  CHECK_MSG(p.isEgg(), "the egg must NOT hatch during the choice");
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  p.chooseStarter(7);
  CHECK(!p.awaitingStarter());
  advance(p, 3);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)7);
}

TEST(egg, hatches_on_its_own_after_3_minutes) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  advance(p, 2);
  CHECK(p.isEgg());
  advance(p, 1);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)1);
}

TEST(egg, three_taps_hatch_it) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(4);
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)2);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, hatching_rolls_genes_and_resets_the_individual) {
  Pet p;
  makePet(p, 4);
  CHECK_RANGE(p.geneAtk, 90, 110);
  CHECK_RANGE(p.geneDef, 90, 110);
  CHECK_RANGE(p.geneSpe, 90, 110);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.bond, (uint8_t)0);
  CHECK_EQ(p.berryKnown, false);
  CHECK_STREQ(p.nick, "");
  CHECK_MSG(p.isRegistered(4), "raising it = registering it in the pokedex");
  CHECK_EQ(p.registeredCount(), (uint16_t)1);
}

TEST(egg, tapping_the_egg_does_nothing_after_hatching) {
  Pet p;
  makePet(p, 4);
  uint8_t cracks = p.eggCracks();
  p.eggTap();
  CHECK_EQ(p.eggCracks(), cracks);
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, egg_rarity_is_valid) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_RANGE(p.eggRarity(), (uint8_t)R_COMMON, (uint8_t)R_LEGENDARY);
}
#pragma endregion

#pragma region Food
TEST(feed, normal_berry_adds_25_and_caps_at_100) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  uint8_t notLoved = (uint8_t)((p.speciesId + 1) % 3);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)75);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)100);  // 100 instead of 105
}

TEST(feed, favorite_berry_adds_more_and_shows_a_heart) {
  Pet p;
  makePet(p, 4);  // 4 % 3 == 1 -> likes the blue one
  CHECK(p.lovesBerry(1));
  CHECK(!p.lovesBerry(0));
  CHECK(!p.lovesBerry(2));
  p.fullness = 10;
  p.joy = 10;
  p.feedBerry(1);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK_EQ(p.joy, (uint8_t)20);
  CHECK(p.berryKnown);
  CHECK(p.showHeart());
  CHECK(p.eating());
}

TEST(feed, candy_adds_weight) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  p.joy = 50;
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.joy, (uint8_t)62);
  CHECK_EQ(p.weight, (uint8_t)12);
}

#pragma region Coffee
TEST(feed, coffee_raises_energy_and_lowers_food) {
  Pet p;
  makePet(p, 4);
  p.energy = 50;
  p.fullness = 50;
  p.feedCoffee();
  CHECK_EQ(p.energy, (uint8_t)75);
  CHECK_EQ(p.fullness, (uint8_t)40);
}

TEST(feed, coffee_caps_energy_at_100_and_food_at_0) {
  Pet p;
  makePet(p, 4);
  p.energy = 90;
  p.fullness = 4;
  p.feedCoffee();
  CHECK_EQ(p.energy, (uint8_t)100);
  CHECK_EQ(p.fullness, (uint8_t)0);
}

TEST(feed, coffee_is_never_the_favorite) {
  for (int16_t species = 1; species <= 3; species++) {  // covers every speciesId % 3
    Pet p;
    makePet(p, 4);
    p.speciesId = species;
    uint8_t joyBefore = p.joy;
    p.feedCoffee();
    CHECK(!p.berryKnown);              // a favorite would be discovered here
    CHECK_EQ(p.joy, joyBefore);        // and give +10 joy
  }
}

TEST(feed, coffee_not_while_sleeping) {
  Pet p;
  makePet(p, 4);
  p.energy = 40;
  p.toggleLight();  // off to sleep
  p.feedCoffee();
  CHECK_EQ(p.energy, (uint8_t)40);
}
#pragma endregion

TEST(feed, sleeping_pet_or_egg_does_not_eat) {
  Pet p;
  makePet(p, 4);
  p.fullness = 40;
  p.toggleLight();  // off to sleep
  CHECK(p.sleeping);
  p.feedBerry(1);
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)40);

  Pet egg;
  mockNvsReset();
  egg.begin();
  egg.chooseStarter(4);
  egg.fullness = 40;
  egg.feedBerry(0);
  CHECK_EQ(egg.fullness, (uint8_t)40);
}
#pragma endregion

#pragma region Tick
TEST(tick, awake_food_drops_2_and_energy_1_per_minute) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 100, 100);
  p.poops = 0;
  mockForceRandom(99);  // random(100) < 15 false: no poops
  advance(p, 10);
  mockClearForcedRandom();
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK_EQ(p.energy, (uint8_t)90);
  CHECK_EQ(p.hygiene, (uint8_t)90);
  CHECK_EQ(p.joy, (uint8_t)90);
  CHECK_EQ(p.ageMinutes, (uint32_t)10);
  CHECK_EQ(p.poops, (uint8_t)0);
}

TEST(tick, bars_never_drop_below_zero) {
  Pet p;
  makePet(p, 4);
  setStats(p, 5, 3, 2, 1);
  advance(p, 30);
  CHECK_EQ(p.fullness, (uint8_t)0);
  CHECK_EQ(p.joy, (uint8_t)0);
  CHECK_EQ(p.energy, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)0);
}

TEST(tick, sleeping_restores_energy_with_floors) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 10, 100);
  p.toggleLight();
  advance(p, 20);
  CHECK_MSG(p.energy == 100, "while sleeping energy rises 6/min up to 100");
  CHECK_MSG(p.fullness > 80, "while sleeping hunger drops ~4x slower");
  advance(p, 2000);  // sleep a very long time: the floors protect it
  CHECK_MSG(p.fullness >= 30, "food floor while sleeping = 30");
  CHECK_MSG(p.joy >= 35, "joy floor while sleeping = 35");
  CHECK_MSG(p.hygiene >= 45, "hygiene floor while sleeping = 45");
  CHECK_EQ(p.careMistakes, (uint8_t)0);
}

TEST(tick, overweight_burns_off_by_itself) {
  Pet p;
  makePet(p, 4);
  p.weight = 30;
  advance(p, 30);
  CHECK_EQ(p.weight, (uint8_t)20);  // -1 every 3 minutes
}

TEST(tick, neglect_counts_once_per_hour) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  advance(p, 50);
  CHECK_MSG(p.careMistakes == 1, "the cooldown avoids counting the same neglect every minute");
  advance(p, 11);
  CHECK_EQ(p.careMistakes, (uint8_t)2);
}

// v1.4 regression: neglect took 3 bond every 30 min and it was impossible to
// recover; now it takes 1 and never leaves it at 0
TEST(tick, neglect_cools_the_bond_without_wiping_it) {
  Pet p;
  makePet(p, 4);
  p.bond = 50;
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.bond, (uint8_t)49);
  p.bond = 1;
  advance(p, 61);
  CHECK_MSG(p.bond == 1, "the bond does not drop below 1");
}

TEST(tick, twelve_hours_of_good_care_build_defense) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trDef, (uint8_t)0);
  for (int minute = 0; minute < 720; minute++) {  // keep everything >= 40 for 12 h
    setStats(p, 100, 100, 100, 100);
    advance(p, 1);
  }
  CHECK_EQ(p.trDef, (uint8_t)1);
}
#pragma endregion

#pragma region Levels
TEST(level, one_level_per_hour_of_play) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.level(), (uint8_t)1);
  p.ageMinutes = 59;
  CHECK_EQ(p.level(), (uint8_t)1);
  p.ageMinutes = 60;
  CHECK_EQ(p.level(), (uint8_t)2);
  p.ageMinutes = 24 * 60;
  CHECK_EQ(p.level(), (uint8_t)25);
}

// Fixed in v1.8: level() was uint8_t and wrapped to 0 after ~10.6 days
TEST(level, does_not_overflow_with_long_lived_pets) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 254 * 60;
  uint8_t before = p.level();
  CHECK_EQ(before, (uint8_t)255);
  p.ageMinutes = 255 * 60;  // level 256 -> should keep rising (or saturate)
  CHECK_MSG(p.level() >= before, "the level should never drop with age");
}
#pragma endregion

#pragma region Evolution
TEST(evolve, needs_level_and_good_care) {
  Pet p;
  makePet(p, 4);  // CHARMANDER evolves at level 16
  p.ageMinutes = 14 * 60;  // level 15
  CHECK(!p.canEvolveNow());
  p.ageMinutes = 15 * 60;  // level 16
  CHECK(p.canEvolveNow());
  p.fullness = 39;  // one bar below 40 blocks it
  CHECK(!p.canEvolveNow());
  p.fullness = 40;
  CHECK(p.canEvolveNow());
}

TEST(evolve, each_care_mistake_delays_one_level) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 15 * 60;  // level 16
  p.careMistakes = 1;
  CHECK(!p.canEvolveNow());
  p.ageMinutes = 16 * 60;  // level 17
  CHECK(p.canEvolveNow());
}

// Fixed in v1.8: the uint8_t cast of (evolveLevel + careMistakes) overflowed
TEST(evolve, many_care_mistakes_do_not_speed_up_evolution) {
  Pet p;
  makePet(p, 4);       // CHARMANDER: evolves at level 16
  p.careMistakes = 250;  // 250 care mistakes: should be very far from evolving
  p.ageMinutes = 9 * 60;  // level 10
  CHECK_MSG(!p.canEvolveNow(), "with 250 care mistakes it cannot evolve at level 10");
}

TEST(evolve, no_evolution_while_asleep_as_egg_or_in_ceremony) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 20 * 60;
  CHECK(p.canEvolveNow());
  p.toggleLight();
  CHECK(!p.canEvolveNow());
  p.toggleLight();
  p.startFarewell();
  CHECK(!p.canEvolveNow());

  Pet egg;
  mockNvsReset();
  egg.begin();
  CHECK(!egg.canEvolveNow());
}

TEST(evolve, transforms_registers_and_animates) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 20 * 60;
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)5);
  CHECK_EQ(p.prevSpeciesId, (int16_t)4);
  CHECK(p.isRegistered(5));
  CHECK(p.evolving());
  CHECK_RANGE(p.evolveT(), 0.0f, 1.0f);
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(!p.evolving());
}

TEST(evolve, final_form_does_not_evolve) {
  Pet p;
  makePet(p, 6);  // CHARIZARD
  p.ageMinutes = 200 * 60;
  CHECK(!p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)6);
}

TEST(evolve, eevee_branches_into_its_three_evolutions) {
  for (int seed = 1; seed <= 12; seed++) {
    randomSeed(seed);
    Pet p;
    makePet(p, DEX_EEVEE);
    p.ageMinutes = 40 * 60;
    CHECK(p.canEvolveNow());
    p.evolve();
    CHECK_RANGE(p.speciesId, (int16_t)DEX_EEVEE_FIRST_BRANCH, (int16_t)DEX_EEVEE_LAST_BRANCH);
    CHECK(p.isRegistered(p.speciesId));
  }
}

TEST(evolve, evolve_button_is_postponed_when_declined) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 15 * 60;  // level 16
  CHECK(p.wantEvolveButton());
  p.declineEvolve();
  CHECK_MSG(!p.wantEvolveButton(), "'keep form' removes the button");
  CHECK_MSG(p.canEvolveNow(), "but the evolution is still available");
  p.ageMinutes = 16 * 60;  // levels up: re-offered
  CHECK(p.wantEvolveButton());
}
#pragma endregion

#pragma region Stats and training
TEST(stats, formula_is_genes_times_base_plus_level_and_training) {
  Pet p;
  makePet(p, 4);  // CHARMANDER: atk 52, def 43, spe 65
  p.geneAtk = p.geneDef = p.geneSpe = 100;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.ageMinutes = 0;  // level 1
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 1));
  CHECK_EQ(p.defStat(), (uint16_t)(43 + 1));
  CHECK_EQ(p.speStat(), (uint16_t)(65 + 1));
  p.ageMinutes = 9 * 60;  // level 10
  p.trAtk = 20;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 10 + 20));
  p.geneAtk = 110;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 * 110 / 100 + 10 + 20));
}

TEST(stats, egg_has_no_stats) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_EQ(p.atkStat(), (uint16_t)0);
  CHECK_EQ(p.defStat(), (uint16_t)0);
  CHECK_EQ(p.speStat(), (uint16_t)0);
}

TEST(train, bag_gives_one_point_every_4_hits_with_a_cap) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trainStrength(0), (uint8_t)0);
  CHECK_EQ(p.trainStrength(40), (uint8_t)10);
  CHECK_EQ(p.trAtk, (uint8_t)10);
  CHECK_MSG(p.trainStrength(1000) == 18, "cap of 18 per session");
  CHECK_EQ(p.strHi, (uint16_t)1000);
}

// Fixed while porting the test suite: trainStrength() returned the theoretical gain
TEST(train, bag_reports_the_real_gain) {
  Pet p;
  makePet(p, 4);
  p.trAtk = 95;
  uint8_t before = p.trAtk;
  uint8_t gain = p.trainStrength(72);  // 72/4 = 18, but only 5 fit
  CHECK_EQ(gain, (uint8_t)(p.trAtk - before));
}

TEST(train, training_caps_at_100) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 20; i++) p.trainStrength(80);
  CHECK_EQ(p.trAtk, (uint8_t)100);
}

TEST(train, bag_tires_and_burns_weight) {
  Pet p;
  makePet(p, 4);
  p.weight = 40;
  p.energy = 80;
  p.fullness = 80;
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)68);
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)30);
  p.energy = 3;  // the floor keeps it from hitting zero
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)3);
}

TEST(play, ball_game_trains_attack_and_keeps_record) {
  Pet p;
  makePet(p, 4);
  p.joy = 10;
  p.weight = 50;
  uint8_t speedBefore = p.trSpe;
  p.playResult(20);
  CHECK_EQ(p.trAtk, (uint8_t)2);   // 20 points would be +4, capped at +2 per game
  CHECK_EQ(p.trSpe, speedBefore);  // speed is no longer trained by the ball game
  CHECK_EQ(p.joy, (uint8_t)45);   // +5 +30 for scoring more than 15
  CHECK_EQ(p.weight, (uint8_t)10);  // 50 - 20*2
  CHECK_EQ(p.gameHi, (uint16_t)20);
  p.playResult(5);
  CHECK_MSG(p.gameHi == 20, "a worse result does not lower the record");
  CHECK_EQ(p.trAtk, (uint8_t)3);   // 5 points: +1
  p.playResult(4);
  CHECK_EQ(p.trAtk, (uint8_t)3);   // under 5 points: no gain
  p.trAtk = 99;
  p.playResult(40);
  CHECK_EQ(p.trAtk, (uint8_t)100); // never above 100
}
#pragma endregion

#pragma region Coin chase
TEST(coin, gain_steps_at_5_and_10_points) {
  static const uint8_t POINTS[] = { 0, 4, 5, 9, 10, 15 };
  static const uint8_t EXPECTED_GAIN[] = { 0, 0, 1, 1, 2, 2 };
  for (int i = 0; i < 6; i++) {
    Pet p;
    makePet(p, 4);
    p.trSpe = 10;
    CHECK_EQ(p.coinChaseResult(POINTS[i]), EXPECTED_GAIN[i]);
    CHECK_EQ(p.trSpe, (uint8_t)(10 + EXPECTED_GAIN[i]));
  }
}

TEST(coin, speed_caps_at_100_and_reports_the_real_gain) {
  Pet p;
  makePet(p, 4);
  p.trSpe = 99;
  CHECK_EQ(p.coinChaseResult(15), (uint8_t)1);
  CHECK_EQ(p.trSpe, (uint8_t)100);
  CHECK_EQ(p.coinChaseResult(15), (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)100);
}

TEST(coin, same_side_effects_as_the_ball_game) {
  Pet p;
  makePet(p, 4);
  p.joy = 10;
  p.energy = 80;
  p.fullness = 80;
  p.weight = 50;
  p.coinChaseResult(10);
  CHECK_EQ(p.joy, (uint8_t)35);       // +5 +10*2
  CHECK_EQ(p.energy, (uint8_t)65);    // -(10 + 10/2)
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)30);    // 50 - 10*2
  CHECK(p.showHeart());
}

TEST(coin, egg_gets_nothing) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK_EQ(p.coinChaseResult(15), (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.coinHi, (uint16_t)0);
}

TEST(coin, ceremony_gets_nothing) {
  Pet p;
  makePet(p, 4);
  p.trSpe = 10;
  p.joy = 40;
  p.ceremony = CER_FAREWELL;
  CHECK_EQ(p.coinChaseResult(15), (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)10);
  CHECK_EQ(p.joy, (uint8_t)40);
  CHECK_EQ(p.coinHi, (uint16_t)0);
}

TEST(coin, record_only_rises_when_beaten) {
  Pet p;
  makePet(p, 4);
  p.coinChaseResult(8);
  CHECK_EQ(p.coinHi, (uint16_t)8);
  p.coinChaseResult(3);
  CHECK_MSG(p.coinHi == 8, "a worse result does not lower the record");
  p.coinChaseResult(8);
  CHECK_EQ(p.coinHi, (uint16_t)8);
  p.coinChaseResult(12);
  CHECK_EQ(p.coinHi, (uint16_t)12);
  CHECK_EQ(p.gameHi, (uint16_t)0);  // the ball game record is separate
}

TEST(coin, record_survives_a_reboot) {
  Pet first;
  makePet(first, 4);
  first.coinChaseResult(13);  // saves

  Pet second;
  second.begin();  // same "NVS": simulates a reboot
  CHECK_EQ(second.coinHi, (uint16_t)13);
}
#pragma endregion

#pragma region Play and care
TEST(play, playing_raises_joy_and_tires) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.play();
  CHECK_EQ(p.joy, (uint8_t)75);
  CHECK_EQ(p.energy, (uint8_t)40);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK(p.showHeart());
}

TEST(care, cleaning_removes_poops_and_fills_hygiene) {
  Pet p;
  makePet(p, 4);
  p.poops = 3;
  p.hygiene = 10;
  p.clean();
  CHECK_EQ(p.poops, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)100);
}

TEST(care, caressing_raises_joy_and_bond) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.caress();
  CHECK_EQ(p.joy, (uint8_t)55);
  CHECK_EQ(p.bond, (uint8_t)1);
  CHECK(p.showHeart());
}

TEST(care, bond_has_a_daily_cap) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 50; i++) p.play();  // +2 bond each time
  CHECK_MSG(p.bond <= 20, "cap of 20 bond points per day");
  CHECK_EQ(p.bond, (uint8_t)20);
}

TEST(mood, reflects_the_state) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 80);
  CHECK_EQ((int)p.mood(), (int)MOOD_HAPPY);
  p.joy = 20;
  CHECK_EQ((int)p.mood(), (int)MOOD_SAD);
  p.joy = 80;
  p.feedBerry(0);
  CHECK_EQ((int)p.mood(), (int)MOOD_EATING);
  mockAdvanceMillis(EAT_ANIM_MS + 1);
  p.toggleLight();
  CHECK_EQ((int)p.mood(), (int)MOOD_SLEEPING);
}
#pragma endregion

#pragma region Pokedex registry
TEST(dexreg, bitmap_covers_the_whole_dex) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0, sizeof(p.dexReg));
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    markRegistered(p, d);
    CHECK(p.isRegistered(d));
  }
  CHECK_EQ(p.registeredCount(), (uint16_t)DEX_COUNT);
}

TEST(dexreg, out_of_range_is_not_registered) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0xFF, sizeof(p.dexReg));
  CHECK(!p.isRegistered(0));
  CHECK(!p.isRegistered(-1));
  CHECK(!p.isRegistered(DEX_COUNT + 1));
  CHECK(!p.isRegistered(32767));
  CHECK_EQ(p.registeredCount(), (uint16_t)DEX_COUNT);
}

TEST(dexreg, incomplete_line_is_detected) {
  Pet p;
  makePet(p, 1);  // BULBASAUR -> IVYSAUR -> VENUSAUR
  CHECK(p.lineHasUnregistered(1));
  markRegistered(p, 2);
  markRegistered(p, 3);
  CHECK_MSG(!p.lineHasUnregistered(1), "line 1-2-3 complete");
}

TEST(dexreg, eevee_branch_counts_all_three) {
  Pet p;
  makePet(p, DEX_EEVEE);
  CHECK(p.lineHasUnregistered(DEX_EEVEE));
  for (int16_t d = DEX_EEVEE_FIRST_BRANCH; d <= DEX_EEVEE_LAST_BRANCH; d++) markRegistered(p, d);
  CHECK(!p.lineHasUnregistered(DEX_EEVEE));
}

TEST(egg, roll_always_gives_a_valid_species) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 400; i++) {
    randomSeed(i * 7919 + 1);
    int16_t d = p.pickEggSpecies();
    CHECK_RANGE(d, (int16_t)1, (int16_t)DEX_COUNT);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "an egg never gives an evolved form");
  }
}

TEST(egg, first_egg_is_a_classic_starter) {
  for (int seed = 1; seed <= 20; seed++) {
    randomSeed(seed);
    mockNvsReset();
    Pet p;
    p.begin();
    int16_t d = p.pickEggSpecies();
    bool classic = false;
    for (int i = 0; i < NUM_CLASSIC_DEX; i++)
      if (CLASSIC_DEX[i] == d) classic = true;
    CHECK_MSG(classic, "without a pokedex, only classic starters come out");
  }
}

TEST(egg, a_runaway_penalizes_rarity) {
  Pet p;
  makePet(p, 4);
  p.lastEnd = CER_RUNAWAY;
  for (int seed = 1; seed <= 60; seed++) {
    randomSeed(seed);
    int16_t d = p.pickEggSpecies();
    CHECK_MSG(DEX_TBL[d].rarity == R_COMMON, "after a runaway only common species come out");
  }
}
#pragma endregion

#pragma region Streak
TEST(streak, first_care_of_the_day_counts) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_EQ(p.bestStreak, (uint16_t)1);
  p.caress();
  CHECK_MSG(p.streak == 1, "the second care of the same day does not count");
  p.setClock(101u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)2);
}

TEST(streak, a_missed_day_resets_it) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  p.setClock(101u * 86400);
  p.caress();
  p.setClock(105u * 86400);  // gap of days
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_MSG(p.bestStreak == 2, "the best streak is kept");
}

TEST(streak, no_clock_means_no_streak) {
  Pet p;
  makePet(p, 4);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)0);
}

TEST(streak, celebrates_milestones) {
  Pet p;
  makePet(p, 4);
  for (uint32_t day = 100; day < 103; day++) {
    p.setClock(day * 86400);
    p.caress();
  }
  CHECK_EQ(p.streak, (uint16_t)3);
  CHECK_MSG(p.showMilestone(), "3 day milestone celebrated");
  CHECK_EQ(p.lastMilestone, (uint16_t)3);
}

TEST(bonus, streak_and_bond_improve_the_egg) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.careBonus(), 0);
  p.streak = 30;
  p.bond = 100;
  CHECK_EQ(p.careBonus(), 14);
  p.streak = 300;  // the streak is capped at 30
  CHECK_EQ(p.careBonus(), 14);
}
#pragma endregion

#pragma region Medals
TEST(medals, level_10_gives_a_medal) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.hasMedal(MED_LV10));
  p.ageMinutes = 9 * 60 - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.showMedal());
}

// not overweight, level 5 and zero care mistakes
TEST(medals, fit_reaches_level_5) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.ageMinutes = 4 * 60 - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_FIT));
  CHECK_EQ(p.totalMedals, (uint16_t)1);

  Pet q;
  makePet(q, 4);
  q.careMistakes = 1;   // one care mistake rules it out
  q.ageMinutes = 4 * 60 - 1;
  advance(q, 1);
  CHECK(!q.hasMedal(MED_FIT));
}

TEST(medals, each_new_medal_adds_to_the_total) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.ageMinutes = 9 * 60 - 1;
  advance(p, 1);  // crosses level 10: MED_LV10 + MED_FIT at once
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.hasMedal(MED_FIT));
  CHECK_MSG(p.totalMedals == 2, "the total counts each bit earned, not each event");
}

TEST(medals, final_form_at_birth) {
  Pet p;
  makePet(p, 143);  // SNORLAX is already a final form
  CHECK(p.hasMedal(MED_FINAL));
  CHECK_EQ(p.totalMedals, (uint16_t)1);
}

TEST(medals, favorite_berry_gives_a_medal) {
  Pet p;
  makePet(p, 4);
  p.feedBerry(1);  // its favorite
  CHECK(p.berryKnown);
  advance(p, 1);
  CHECK(p.hasMedal(MED_BERRY));
}

TEST(medals, total_accumulates_across_raisings) {
  Pet p;
  makePet(p, 143);
  CHECK_EQ(p.totalMedals, (uint16_t)1);
  p.newEgg();
  p.chooseStarter(144);  // ARTICUNO, another final form
  p.eggTap(); p.eggTap(); p.eggTap();
  CHECK_MSG(p.medals == MED_FINAL, "the individual's medals are reset");
  CHECK_MSG(p.totalMedals == 2, "the player's total is kept");
}
#pragma endregion

#pragma region Ceremonies
TEST(ceremony, farewell_only_in_final_form_after_3_days) {
  Pet p;
  makePet(p, 6);  // CHARIZARD, final form
  p.ageMinutes = FAREWELL_AGE_MIN - 1;
  CHECK(!p.canFarewellNow());
  p.ageMinutes = FAREWELL_AGE_MIN;
  CHECK(p.canFarewellNow());

  Pet q;
  makePet(q, 4);  // CHARMANDER is not a final form
  q.ageMinutes = FAREWELL_AGE_MIN * 3;
  CHECK(!q.canFarewellNow());
}

TEST(ceremony, farewell_leaves_a_new_egg) {
  Pet p;
  makePet(p, 6);
  p.ageMinutes = FAREWELL_AGE_MIN;
  p.startFarewell();
  CHECK_EQ(p.ceremony, (uint8_t)CER_FAREWELL);
  CHECK_RANGE(p.ceremonyT(), 0.0f, 1.0f);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

TEST(ceremony, runaway_needs_an_hour_of_total_neglect) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  CHECK(!p.canRunawayNow());
  advance(p, RUNAWAY_TICKS - 1);
  CHECK(!p.canRunawayNow());
  advance(p, 1);
  CHECK(p.canRunawayNow());
}

TEST(ceremony, a_single_care_prevents_the_runaway) {
  Pet p;
  makePet(p, 4);
  p.dbgRunawayReady();
  CHECK(p.canRunawayNow());
  p.feedBerry(0);
  advance(p, 1);
  CHECK_MSG(!p.canRunawayNow(), "eating resets the neglect counter");
}

TEST(ceremony, no_interaction_during_the_ceremony) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.poops = 2;
  p.startRunaway();
  p.feedBerry(0);
  p.feedCandy();
  p.play();
  p.clean();
  p.caress();
  p.toggleLight();
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.poops, (uint8_t)2);
  CHECK(!p.sleeping);
}

TEST(ceremony, release_marks_the_end_as_release) {
  Pet p;
  makePet(p, 4);
  p.release();
  CHECK_EQ(p.ceremony, (uint8_t)CER_RELEASE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
}

TEST(ceremony, an_egg_cannot_say_farewell_or_be_released) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.startFarewell();
  p.release();
  p.startRunaway();
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
}
#pragma endregion

#pragma region Offline progression
TEST(offline, applies_powered_off_time_with_a_floor_of_15) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600 * 60);  // 600 minutes powered off
  CHECK_EQ(p.ageMinutes, (uint32_t)600);
  CHECK_MSG(p.fullness == 15, "offline food floor = 15");
  CHECK_MSG(p.energy == 15, "offline energy floor = 15");
  CHECK_MSG(p.joy == 15, "offline joy floor = 15");
  CHECK_MSG(p.hygiene == 15, "offline hygiene floor = 15");
  CHECK_MSG(p.careMistakes == 0, "no care mistakes accumulate while away");
  CHECK_EQ(p.poops, (uint8_t)2);  // one poop every 4 h
}

TEST(offline, capped_at_two_weeks) {
  Pet p;
  makePet(p, 4);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90u * 86400);  // 90 days
  CHECK_EQ(p.ageMinutes, (uint32_t)(14u * 24 * 60));
}

TEST(offline, under_two_minutes_does_nothing) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

TEST(offline, egg_hatches_while_you_are_away) {
  Pet p;
  makePet(p, 4);
  p.newEgg();  // pokedex already exists: no starter prompt
  CHECK(!p.awaitingStarter());
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600);  // 10 minutes
  CHECK(!p.isEgg());
  CHECK_RANGE(p.speciesId, (int16_t)1, (int16_t)DEX_COUNT);
  CHECK_EQ(p.hatchedEpoch, (uint32_t)(1000u * 86400 + 3 * 60));  // the minute it hatched, not the boot
}

TEST(offline, sleeping_pet_also_rests_while_powered_off) {
  Pet p;
  makePet(p, 4);
  setStats(p, 90, 90, 20, 90);
  p.toggleLight();
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 120 * 60);
  CHECK_EQ(p.energy, (uint8_t)100);
  CHECK_MSG(p.fullness >= 30, "sleep floors also apply offline");
  CHECK_MSG(p.poops == 0, "a sleeping pet does not make a mess");
}

TEST(offline, without_a_clock_nothing_is_applied) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.syncClock(0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}
#pragma endregion

#pragma region Age
TEST(age, counts_real_days_since_hatching_while_level_counts_play_time) {
  mockNvsReset();
  mockSetMillis(0);
  Pet p;
  p.begin();
  p.setClock(1000u * 86400);
  p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  CHECK_EQ(p.hatchedEpoch, (uint32_t)(1000u * 86400));
  p.lastSeenEpoch = 1000u * 86400 + 5u * 86400 + 3600;  // 5 days later, not played
  CHECK_EQ(p.ageDays(), (uint32_t)5);
  CHECK_EQ(p.level(), (uint16_t)1);
}

TEST(age, falls_back_to_play_time_without_a_hatch_time) {
  Pet p;
  makePet(p, 4);  // hatched with no clock
  CHECK_EQ(p.hatchedEpoch, (uint32_t)0);
  p.ageMinutes = 3 * 1440;
  CHECK_EQ(p.ageDays(), (uint32_t)3);
}

TEST(age, clock_set_before_the_hatch_time_reads_zero) {
  Pet p;
  makePet(p, 4);
  p.hatchedEpoch = 1000u * 86400;
  p.lastSeenEpoch = 999u * 86400;
  CHECK_EQ(p.ageDays(), (uint32_t)0);
}

TEST(age, new_egg_clears_the_hatch_time) {
  Pet p;
  makePet(p, 4);
  p.hatchedEpoch = 1000u * 86400;
  p.newEgg();
  CHECK_EQ(p.hatchedEpoch, (uint32_t)0);
}
#pragma endregion

#pragma region Saving
TEST(save, state_survives_a_reboot) {
  Pet a;
  makePet(a, 25);  // PIKACHU
  a.ageMinutes = 1234;
  a.hatchedEpoch = 123456;
  a.streak = 9;
  a.bestStreak = 11;
  a.bond = 44;
  a.trAtk = 17;
  a.weight = 33;
  a.gameHi = 21;
  a.rename("SPARKY");  // rename persists the whole state

  Pet b;
  b.begin();  // same "NVS": simulates a reboot
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)1234);
  CHECK_EQ(b.hatchedEpoch, (uint32_t)123456);
  CHECK_EQ(b.streak, (uint16_t)9);
  CHECK_EQ(b.bestStreak, (uint16_t)11);
  CHECK_EQ(b.bond, (uint8_t)44);
  CHECK_EQ(b.trAtk, (uint8_t)17);
  CHECK_EQ(b.weight, (uint8_t)33);
  CHECK_EQ(b.gameHi, (uint16_t)21);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(b.isRegistered(25));
  CHECK(!b.awaitingStarter());
}

// a version 2 save (no hatchedEpoch) still loads, and the hatch time is
// estimated from the play time
TEST(save, version_2_blob_is_upgraded_in_place) {
  #pragma region Write a version 3 save, then rewrite it as version 2
  Pet a;
  makePet(a, 25);
  a.ageMinutes = 600;
  a.bond = 44;
  a.setClock(1000u * 86400);  // saves
  Preferences raw;
  raw.begin(NVS_NAMESPACE, false);
  uint8_t blobBytes[512];
  size_t blobLength = raw.getBytes("pet_v2", blobBytes, sizeof(blobBytes));
  CHECK(blobLength > 20);
  const size_t hatchedOffset = 16;  // after magic, version, size, ageMinutes, lastCareDay
  memmove(blobBytes + hatchedOffset, blobBytes + hatchedOffset + 4, blobLength - hatchedOffset - 4);
  blobLength -= 4;
  blobBytes[4] = 2;  // version (little-endian uint16)
  blobBytes[5] = 0;
  blobBytes[6] = (uint8_t)(blobLength & 0xFF);  // size
  blobBytes[7] = (uint8_t)(blobLength >> 8);
  raw.putBytes("pet_v2", blobBytes, blobLength);
  raw.end();
  #pragma endregion

  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)600);
  CHECK_EQ(b.bond, (uint8_t)44);
  CHECK_EQ(b.hatchedEpoch, (uint32_t)(1000u * 86400 - 600 * 60));
}

TEST(save, nickname_is_trimmed_to_11_characters) {
  Pet p;
  makePet(p, 4);
  p.rename("VERY-LONG-NAME-THAT-WONT-FIT");
  CHECK_EQ((int)strlen(p.nick), 11);
  CHECK_STREQ(p.nick, "VERY-LONG-N");
}

TEST(save, factory_reset_leaves_everything_as_new) {
  Pet a;
  makePet(a, 4);
  a.factoryReset();
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.awaitingStarter());
  CHECK_EQ(b.registeredCount(), (uint16_t)0);
}

TEST(save, autosave_is_flagged_and_flushed) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.savePending());
  advance(p, 5);
  CHECK_MSG(p.savePending(), "every 5 ticks a save is left pending");
  p.flushSave();
  CHECK(!p.savePending());
}
#pragma endregion

#pragma region Wild battles and catching
static int16_t findFirstDexWithRarity(uint8_t rarity) {
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
    if (DEX_TBL[dex].rarity == rarity) return dex;
  return -1;
}

TEST(caught, register_caught_sets_bitmap_and_persists) {
  Pet first;
  makePet(first, 4);
  CHECK_EQ(first.caughtCount(), (uint16_t)0);
  first.registerCaught(25);
  first.registerCaught(DEX_COUNT);
  CHECK(first.isCaught(25));
  CHECK(first.isCaught(DEX_COUNT));
  CHECK(!first.isCaught(26));
  CHECK_EQ(first.caughtCount(), (uint16_t)2);
  CHECK_MSG(!first.isRegistered(25), "registeredCount stays raised only");
  CHECK_EQ(first.registeredCount(), (uint16_t)1);
  CHECK(!first.isShinyRegistered(25));

  Pet rebooted;
  rebooted.begin();  // same "NVS": simulates a reboot
  CHECK(rebooted.isCaught(25));
  CHECK(rebooted.isCaught(DEX_COUNT));
  CHECK_EQ(rebooted.caughtCount(), (uint16_t)2);
}

TEST(caught, out_of_range_is_ignored) {
  Pet p;
  makePet(p, 4);
  p.registerCaught(0);
  p.registerCaught(-1);
  p.registerCaught(DEX_COUNT + 1);
  CHECK_EQ(p.caughtCount(), (uint16_t)0);
  memset(p.dexCaught, 0xFF, sizeof(p.dexCaught));
  CHECK(!p.isCaught(0));
  CHECK(!p.isCaught(DEX_COUNT + 1));
  CHECK_EQ(p.caughtCount(), (uint16_t)DEX_COUNT);
}

TEST(caught, shiny_catch_sets_shiny_registry) {
  Pet p;
  makePet(p, 4);
  p.registerCaught(16, true);
  CHECK(p.isCaught(16));
  CHECK(p.isShinyRegistered(16));
  CHECK(!p.isRegistered(16));

  Pet rebooted;
  rebooted.begin();
  CHECK(rebooted.isShinyRegistered(16));
}

TEST(battle_pet, win_and_loss_update_counters_and_streak) {
  Pet first;
  makePet(first, 4);
  first.applyBattleWin(16, false);
  first.applyBattleWin(16, false);
  CHECK_EQ(first.battleWins, (uint16_t)2);
  CHECK_EQ(first.battleStreak, (uint16_t)2);
  CHECK_EQ(first.bestBattleStreak, (uint16_t)2);
  first.applyBattleLoss();
  CHECK_EQ(first.battleLosses, (uint16_t)1);
  CHECK_EQ(first.battleStreak, (uint16_t)0);
  CHECK_EQ(first.bestBattleStreak, (uint16_t)2);
  first.applyBattleWin(16, false);
  CHECK_EQ(first.battleWins, (uint16_t)3);
  CHECK_EQ(first.battleStreak, (uint16_t)1);
  CHECK_EQ(first.bestBattleStreak, (uint16_t)2);

  Pet rebooted;
  rebooted.begin();
  CHECK_EQ(rebooted.battleWins, (uint16_t)3);
  CHECK_EQ(rebooted.battleLosses, (uint16_t)1);
  CHECK_EQ(rebooted.battleStreak, (uint16_t)1);
  CHECK_EQ(rebooted.bestBattleStreak, (uint16_t)2);
}

TEST(battle_pet, win_trains_the_stat_opposite_to_the_wild_strength) {
  Pet p;
  makePet(p, 4);
  // MANKEY (56): attack is its best base stat -> trains defense
  BattleReward defenseReward = p.applyBattleWin(56, false);
  CHECK_EQ((int)defenseReward.stat, (int)BATTLE_REWARD_DEF);
  CHECK_EQ(defenseReward.amount, (uint8_t)1);
  CHECK_EQ(p.trDef, (uint8_t)1);
  // SQUIRTLE (7): defense is its best base stat -> trains attack, close win +1
  BattleReward attackReward = p.applyBattleWin(7, true);
  CHECK_EQ((int)attackReward.stat, (int)BATTLE_REWARD_ATK);
  CHECK_EQ(attackReward.amount, (uint8_t)2);
  CHECK_EQ(p.trAtk, (uint8_t)2);
  // PIDGEY (16): speed is its best base stat -> trains speed
  BattleReward speedReward = p.applyBattleWin(16, false);
  CHECK_EQ((int)speedReward.stat, (int)BATTLE_REWARD_SPE);
  CHECK_EQ(p.trSpe, (uint8_t)1);
  // rare wild gives 2
  BattleReward rareReward = p.applyBattleWin(findFirstDexWithRarity(R_RARE), false);
  CHECK_EQ(rareReward.amount, (uint8_t)2);
}

TEST(battle_pet, egg_gets_no_battle_reward) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  BattleReward reward = p.applyBattleWin(16, true);
  CHECK_EQ((int)reward.stat, (int)BATTLE_REWARD_NONE);
  CHECK_EQ(p.battleWins, (uint16_t)0);
  p.applyBattleLoss();
  CHECK_EQ(p.battleLosses, (uint16_t)0);
}

TEST(catch_chance, legendary_is_never_catchable) {
  Pet p;
  makePet(p, 4);
  p.bond = 100;
  int16_t legendaryDex = findFirstDexWithRarity(R_LEGENDARY);
  CHECK(legendaryDex >= 1);
  CHECK_EQ(p.catchChanceForWild(legendaryDex, 1, 100, true), (uint8_t)0);
  CHECK_EQ(p.respectCatchChanceForWild(legendaryDex, 1, 100), (uint8_t)0);
  CHECK(!p.tryCatchWild(legendaryDex, 1, 100, true, 0));
  CHECK(!p.tryRespectCatchWild(legendaryDex, 1, 100, 0));
  CHECK(!p.isCaught(legendaryDex));
  CHECK_EQ(p.catchChanceForWild(0, 5, 5, false), (uint8_t)0);
  CHECK_EQ(p.catchChanceForWild(DEX_COUNT + 1, 5, 5, false), (uint8_t)0);
}

TEST(catch_chance, bounds_for_all_species_levels_and_bond) {
  Pet p;
  makePet(p, 4);
  const uint8_t levels[] = { 0, 1, 5, 20, 50, 100, 255 };
  const uint8_t bonds[] = { 0, 50, 100 };
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++) {
    uint8_t rarity = DEX_TBL[dex].rarity;
    if (rarity == R_LEGENDARY) continue;
    for (uint8_t bondValue : bonds) {
      p.bond = bondValue;
      for (uint8_t wildLevel : levels) {
        for (uint8_t petLevel : levels) {
          for (int closeWin = 0; closeWin < 2; closeWin++) {
            uint8_t chance = p.catchChanceForWild(dex, wildLevel, petLevel, closeWin != 0);
            CHECK_RANGE(chance, 10, 75);
            if (rarity == R_RARE) CHECK(chance <= 60);
          }
          CHECK_RANGE(p.respectCatchChanceForWild(dex, wildLevel, petLevel), 5, 25);
        }
      }
    }
  }
}

TEST(catch_chance, catch_roll_compares_with_chance) {
  Pet p;
  makePet(p, 4);
  p.bond = 0;
  int16_t commonDex = findFirstDexWithRarity(R_COMMON);
  uint8_t chance = p.catchChanceForWild(commonDex, 10, 10, false);
  CHECK_EQ(chance, (uint8_t)55);  // common, same level, no bonus
  CHECK(!p.tryCatchWild(commonDex, 10, 10, false, chance));  // roll == chance fails
  CHECK(!p.isCaught(commonDex));
  CHECK(p.tryCatchWild(commonDex, 10, 10, false, chance - 1, true));
  CHECK(p.isCaught(commonDex));
  CHECK(p.isShinyRegistered(commonDex));
}

TEST(catch_chance, respect_catch_is_harder) {
  Pet p;
  makePet(p, 4);
  p.bond = 0;
  int16_t commonDex = findFirstDexWithRarity(R_COMMON);
  uint8_t respectChance = p.respectCatchChanceForWild(commonDex, 10, 10);
  CHECK_EQ(respectChance, (uint8_t)25);  // (55 + 8) * 40% = 25
  CHECK(!p.tryRespectCatchWild(commonDex, 10, 10, respectChance));
  CHECK(p.tryRespectCatchWild(commonDex, 10, 10, respectChance - 1));
  CHECK(p.isCaught(commonDex));
}

TEST(catch_chance, requested_battle_keeps_70_percent) {
  Pet p;
  makePet(p, 4);
  p.bond = 0;
  int16_t commonDex = findFirstDexWithRarity(R_COMMON);
  CHECK_EQ(p.catchChanceForWild(commonDex, 10, 10, false, true), (uint8_t)38);  // 55 * 70%
  CHECK_EQ(p.respectCatchChanceForWild(commonDex, 10, 10, true), (uint8_t)17);  // (63 * 70% = 44) * 40%
  uint8_t requestedChance = p.catchChanceForWild(commonDex, 10, 10, false, true);
  CHECK(!p.tryCatchWild(commonDex, 10, 10, false, requestedChance, false, true));  // roll == chance fails
  CHECK(p.tryCatchWild(commonDex, 10, 10, false, requestedChance - 1, false, true));
  int16_t legendaryDex = findFirstDexWithRarity(R_LEGENDARY);
  CHECK_EQ(p.catchChanceForWild(legendaryDex, 1, 100, true, true), (uint8_t)0);
}

TEST(battle_pet, battle_level_caps_a_long_lived_pet) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 249UL * MINUTES_PER_LEVEL;  // level 250
  CHECK_EQ(p.level(), (uint16_t)250);
  CHECK_EQ(battleLevelFor(p.level()), (uint8_t)100);
  CHECK_EQ(battleLevelFor(250), (uint8_t)100);
}
#pragma endregion

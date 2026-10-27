// Wild battles on screen: the random encounter prompt and the manual turn-based
// battle (see battle_ui.h; the turn engine itself is battle.h / battle.cpp).
// BATTLE_*, WILD_PROMPT_*: boards/layout.h (one table per board, shared by drawing and battleTap)

#pragma region Includes
#include "battle_ui.h"
#include "app_state.h"      // battle and wild prompt state, pet, pmd, galleryPmd, screen flags, isNightHour
#include "audio.h"          // sfxPlay
#include "board_display.h"  // gfx
#include "boards/layout.h"  // BATTLE_*, WILD_PROMPT_*
#include "colors.h"         // WATER_BLUE_COLOR, BATTLE_NORMAL_ATTACK_COLOR
#include "countof.h"
#include "dex.h"            // DEX_TBL, DEX_COUNT, dexName, TYPE_*
#include "i18n.h"           // T(), TYPE_NAME_COUNT
#include "pet.h"            // timeLeft
#include "power_ui.h"       // isMainScreenShowing
#include "scene.h"          // drawGameScene, sceneHour
#include "sdmon.h"          // thumbs, PMD_IDLE
#include "species.h"        // SPR_ICON_PLAY
#include "sprite_draw.h"    // drawPmdAct, drawPmdActM, drawThumbCell, drawMap
#include "ui_text.h"        // FONT_H, setCur, printT, textW, shrinkTextSizeToWidth, drawTextCenteredInBox
#include "ui_widgets.h"     // drawProgressBar, drawBorderedBox, drawLabeledButton, drawResultLine, drawTypeChips,
                            // isTouchInsidePaddedRect
#pragma endregion

static_assert(TYPE_NAME_COUNT == TYPE_COUNT, "type name table out of sync");

#pragma region Move text buffers
// Declared in app_state.h, defined here so test/test_tools.py finds each buffer
// size in the same file as the snprintf calls that fill it.
char battlePlayerMoveText[64] = "";
char battleWildMoveText[64] = "";
#pragma endregion

#pragma region Constants
// chance (percent) of a wild prompt on each roll, by the day phase
#define WILD_ENCOUNTER_CHANCE_NIGHT_PCT 4
#define WILD_ENCOUNTER_CHANCE_MORNING_PCT 7
#define WILD_ENCOUNTER_CHANCE_DEFAULT_PCT 8
// ghost and poison species keep the daytime chance at night
#define WILD_ENCOUNTER_CHANCE_NIGHT_GHOST_POISON_PCT 8
#pragma endregion

#pragma region Battle button rows (one table per row, shared by drawing and battleTap)
struct BattleButton {
  int16_t left;
  uint16_t fillColor;
  StrId label;          // BATTLE_REST: drawn followed by the rests left
  BattleAction action;  // action row BATTLE_ATTACK: opens the Quick / Attack / Heavy overlay
};

struct BattleButtonRow {
  const BattleButton *buttons;
  uint8_t buttonCount;
  int16_t top, width, height, radius, hitPadX, hitPadY;
};

// Quick / Attack / Heavy overlay (after Attack)
static const BattleButton BATTLE_ATTACK_MENU_BUTTONS[] = {
  { BATTLE_QUICK_X, UI_BAR_BAD, S_QUICK_ATTACK, BATTLE_ATTACK_QUICK },
  { BATTLE_NORMAL_X, BATTLE_NORMAL_ATTACK_COLOR, S_NORMAL_ATTACK, BATTLE_ATTACK },
  { BATTLE_HEAVY_X, UI_BAR_WARN, S_HEAVY_ATTACK, BATTLE_ATTACK_HEAVY },
};
static const BattleButtonRow BATTLE_ATTACK_MENU_ROW = {
  BATTLE_ATTACK_MENU_BUTTONS, (uint8_t)countof(BATTLE_ATTACK_MENU_BUTTONS), BATTLE_ATTACK_MENU_Y, BATTLE_ATTACK_MENU_W,
  BATTLE_ATTACK_MENU_H, BATTLE_ATTACK_MENU_R, BATTLE_ATTACK_MENU_HIT_PAD_X, BATTLE_ATTACK_MENU_HIT_PAD_Y
};

// Attack / Dodge / Rest
static const BattleButton BATTLE_ACTION_ROW_BUTTONS[] = {
  { BATTLE_ATTACK_X, UI_BAR_BAD, S_ATTACK, BATTLE_ATTACK },
  { BATTLE_DODGE_X, WATER_BLUE_COLOR, S_DODGE, BATTLE_DODGE },
  { BATTLE_REST_X, UI_BAR_OK, S_REST, BATTLE_REST },
};
static const BattleButtonRow BATTLE_ACTION_ROW = {
  BATTLE_ACTION_ROW_BUTTONS, (uint8_t)countof(BATTLE_ACTION_ROW_BUTTONS), BATTLE_ACTION_Y, BATTLE_ACTION_W,
  BATTLE_ACTION_H, BATTLE_ACTION_R, BATTLE_ACTION_HIT_PAD_X, BATTLE_ACTION_HIT_PAD_Y
};

#pragma region Attack animations of the last turn
// millis() when each side's Attack pose starts (0 = not attacking this turn): our
// pet lunges first, the wild one after our lunge ends when both attacked
static uint32_t playerAttackAnimStartMs = 0;
static uint32_t wildAttackAnimStartMs = 0;

static bool isAttackMove(BattleAction action) {
  return action == BATTLE_ATTACK || action == BATTLE_ATTACK_QUICK || action == BATTLE_ATTACK_HEAVY;
}

// starts the Attack poses for the turn that was just played
static void startTurnAttackAnimations(BattleAction playerAction) {
  uint32_t now = millis();
  playerAttackAnimStartMs = 0;
  wildAttackAnimStartMs = 0;
  if (battleTurn.restFailed) return;  // no turn was played
  // a side that fainted before acting did no damage and never moved
  bool playerActed = !(battleRun.playerHp == 0 && battleTurn.playerDamage == 0);
  bool wildActed = !(battleRun.enemyHp == 0 && battleTurn.enemyDamage == 0);
  bool playerAttacked = isAttackMove(playerAction) && playerActed && pmd.has(PMD_ATTACK);
  bool wildAttacked = isAttackMove(battleTurn.enemyAction) && wildActed && galleryPmd.has(PMD_ATTACK);
  if (playerAttacked) playerAttackAnimStartMs = now;
  if (wildAttacked)
    wildAttackAnimStartMs = now + (playerAttacked ? pmdActTotalMs(pmd.acts[PMD_ATTACK]) : 0);
}

// Attack while its animation runs, Idle otherwise; elapsedMs is the time into that action
static uint8_t battleSpriteAction(const PmdMon &mon, uint32_t attackStartMs, uint32_t now, uint32_t &elapsedMs) {
  if (attackStartMs && now >= attackStartMs && now - attackStartMs < pmdActTotalMs(mon.acts[PMD_ATTACK])) {
    elapsedMs = now - attackStartMs;
    return PMD_ATTACK;
  }
  elapsedMs = now;
  return PMD_IDLE;
}
#pragma endregion

// the first button of the row whose padded box holds the touch, nullptr if none
static const BattleButton *battleButtonAt(const BattleButtonRow &row, int16_t x, int16_t y) {
  for (uint8_t i = 0; i < row.buttonCount; i++) {
    const BattleButton &button = row.buttons[i];
    if (isTouchInsidePaddedRect(x, y, button.left, row.top, row.width, row.height, row.hitPadX, row.hitPadY)) return &button;
  }
  return nullptr;
}

// All the boxes first, then all the labels in UI_BG_DAY: a label that does not
// fit at size 1 may spill past its box, and the next box must not cover it.
static void drawBattleButtonRow(const BattleButtonRow &row) {
  for (uint8_t i = 0; i < row.buttonCount; i++)
    gfx->fillRoundRect(row.buttons[i].left, row.top, row.width, row.height, row.radius, row.buttons[i].fillColor);
  gfx->setTextColor(UI_BG_DAY);
  for (uint8_t i = 0; i < row.buttonCount; i++) {
    const BattleButton &button = row.buttons[i];
    if (button.action == BATTLE_REST) {
      // 40: a CJK "rest" label plus " %u"
      char restLabel[40];
      snprintf(restLabel, sizeof(restLabel), "%s %u", T(S_REST), battleRun.restUsesLeft);
      drawTextCenteredInBox(restLabel, BATTLE_BTN_TEXT_SIZE, button.left, row.top, row.width, row.height);
    } else {
      drawTextCenteredInBox(T(button.label), BATTLE_BTN_TEXT_SIZE, button.left, row.top, row.width, row.height);
    }
  }
}
#pragma endregion

#pragma region Wild battle: stats, scheduling and the random encounter
BattleStats petBattleStats() {
  BattleStats stats = {};
  stats.level = battleLevelFor(pet.level());
  stats.atk = pet.atkStat();
  stats.def = pet.defStat();
  stats.spe = pet.speStat();
  stats.hp = 0;  // derived from level and defense by the battle engine
  if (!pet.isEgg() && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    stats.type1 = DEX_TBL[pet.speciesId].type1;
    stats.type2 = DEX_TBL[pet.speciesId].type2;
  }
  return stats;
}

static void scheduleNextWild(uint32_t now) {
  nextWildEligible = now + WILD_COOLDOWN_MS;
}

// fixed 1 in WILD_SHINY_ODDS
static bool rollWildShiny() {
  return random(WILD_SHINY_ODDS) == 0;
}

// the prompt only shows over the plain, idle main screen with the user looking at it
static bool mainScreenReadyForWild() {
  if (screenOff || dimStage > 0 || wasPressed) return false;
  if (!isMainScreenShowing()) return false;  // every screen, menu and dialog (including this prompt)
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return false;
  if (pet.evolving() || pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return false;
  return true;
}

void openWildPrompt(int16_t wildDex, uint32_t now) {
  if (wildDex < 1 || wildDex > DEX_COUNT) return;
  wildPromptDex = wildDex;
  wildPromptLevel = wildLevelFor(battleLevelFor(pet.level()), (uint8_t)random(100));
  wildPromptShiny = rollWildShiny();
  wildPromptUntil = now + WILD_PROMPT_MS;
  scheduleNextWild(now);
  sfxPlay(SFX_WILD_APPEAR);
}

// once the cooldown is over, one roll per minute; the chance depends on the day phase
void maybeOfferWildEncounter(uint32_t now) {
  if (nextWildEligible == 0) {
    scheduleNextWild(now);
    return;
  }
  if (wildPromptUntil) return;
  if (now - lastWildCheck < WILD_CHECK_MS) return;
  lastWildCheck = now;
  if (timeLeft(nextWildEligible)) return;
  if (!mainScreenReadyForWild()) return;

  #pragma region Encounter chance: lower at night, except for ghost and poison species
  DayPhase dayPhase = (DayPhase)dayPhaseFromHour(sceneHour());
  int16_t candidateDex = pickWildSpecies((uint8_t)random(100), dayPhase);
  uint8_t chancePct = dayPhase == DAY_PHASE_NIGHT     ? WILD_ENCOUNTER_CHANCE_NIGHT_PCT
                      : dayPhase == DAY_PHASE_MORNING ? WILD_ENCOUNTER_CHANCE_MORNING_PCT
                                                      : WILD_ENCOUNTER_CHANCE_DEFAULT_PCT;
  if (dayPhase == DAY_PHASE_NIGHT && candidateDex >= 1 && candidateDex <= DEX_COUNT) {
    const DexEntry &candidate = DEX_TBL[candidateDex];
    if (candidate.type1 == TYPE_GHOST || candidate.type2 == TYPE_GHOST ||
        candidate.type1 == TYPE_POISON || candidate.type2 == TYPE_POISON)
      chancePct = WILD_ENCOUNTER_CHANCE_NIGHT_GHOST_POISON_PCT;
  }
  if ((uint8_t)random(100) >= chancePct) return;
  #pragma endregion

  openWildPrompt(candidateDex, now);
}
#pragma endregion

#pragma region Wild battle: open, close, turns and result
// no catch offered or tried yet (a new battle, or the battle screen closing)
static void resetBattleCatchState() {
  battleCatchOffered = false;
  battleCatchTried = false;
  battleCatchDone = false;
  battleCatchSuccess = false;
  battleRespectCatch = false;
  battleCatchChance = 0;
}

static void closeBattle() {
  battleOpen = false;
  battleResolved = false;
  battleAttackMenuOpen = false;
  resetBattleCatchState();
  galleryPmd.unload();  // the wild sprite borrowed the gallery's PmdMon
}

// forcedDex 0 = random wild species; forcedLevel 0 = random level around the pet's;
// forcedShiny -1 = roll, 0 / 1 = forced
void startBattleWith(int16_t forcedDex, uint8_t forcedLevel, int8_t forcedShiny, bool fromWildEncounter) {
  if (!canStartWildBattle(pet.isEgg(), pet.sleeping, pet.ceremony)) return;
  // galleryPmd is shared with the gallery (never open together); full screens are left alone (serial commands)
  if (galleryOpen || gameOpen || coinOpen || sackOpen || kbOpen || settingsScreen != SETTINGS_CLOSED) return;

  #pragma region Close the prompt and any main screen menu or dialog
  wildPromptUntil = 0;
  playMenuUntil = 0;
  feedMenuUntil = 0;
  confirmUntil = 0;
  choiceKind = CHOICE_NONE;
  cardOpen = false;
  scheduleNextWild(millis());
  playerAttackAnimStartMs = 0;  // no lunge left over from the previous battle
  wildAttackAnimStartMs = 0;
  #pragma endregion

  #pragma region Pick the wild species, level and shiny
  uint8_t petBattleLevel = battleLevelFor(pet.level());
  if (forcedDex >= 1 && forcedDex <= DEX_COUNT) {
    battleDex = forcedDex;
    battleLevel = forcedLevel ? forcedLevel : wildLevelFor(petBattleLevel, (uint8_t)random(100));
  } else {
    battleDex = pickWildSpecies((uint8_t)random(100), dayPhaseFromHour(sceneHour()));
    battleLevel = wildLevelFor(petBattleLevel, (uint8_t)random(100));
  }
  if (battleDex < 1 || battleDex > DEX_COUNT) return;
  battleShiny = forcedShiny < 0 ? rollWildShiny() : forcedShiny != 0;
  #pragma endregion

  #pragma region Reset the battle state
  battlePlayer = petBattleStats();
  battleEnemy = wildBattleStats(battleDex, battleLevel);
  battleEnemy.hp = 0;
  battleRun = beginBattleRuntime(battlePlayer, battleEnemy);
  battleTurn = {};
  battleReward = {};
  battlePlayerMoveText[0] = 0;
  battleWildMoveText[0] = 0;
  battleAttackMenuOpen = false;
  battleLowHpWarned = false;
  resetBattleCatchState();
  battleResolved = false;
  battleFromWildEncounter = fromWildEncounter;
  battleOpen = true;
  #pragma endregion

  galleryPmd.unload();
  galleryPmd.load(battleDex, battleShiny);  // streaming on the CYD: headers only, one frame per draw
  sfxPlay(SFX_TAP);
}

void startBattle() {
  startBattleWith(0, 0, -1);
}

// applies the win (reward, catch offer) or the loss (respect catch after a close loss)
static void finishBattle() {
  if (battleResolved) return;
  battleResolved = true;
  battleAttackMenuOpen = false;
  if (battleTurn.playerWon) {
    bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
    battleReward = pet.applyBattleWin(battleDex, closeWin);
    battleCatchOffered = true;
    battleRespectCatch = false;
    battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                               !battleFromWildEncounter);
    sfxPlay(SFX_BATTLE_WIN);
  } else {
    battleReward = {};
    pet.applyBattleLoss();
    bool closeLoss = battleRun.enemyHp > 0 && battleRun.enemyHp * 100UL <= battleRun.enemyMaxHp * 30UL;
    battleCatchChance = closeLoss ? pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level,
                                                                  !battleFromWildEncounter) : 0;
    battleCatchOffered = battleCatchChance > 0;
    battleRespectCatch = battleCatchOffered;
    sfxPlay(SFX_BATTLE_LOSS);
  }
}

// one turn: runs the engine, writes the message line and plays the turn sounds
static void performBattleAction(BattleAction action) {
  if (battleResolved) return;
  battleAttackMenuOpen = false;
  BattleAction enemyAction = pickEnemyAction(battleRun, (uint8_t)random(100));
  battleTurn = stepBattle(battleRun, action, (uint8_t)random(100), enemyAction);
  startTurnAttackAnimations(action);

  #pragma region Our move (under our sprite): rest, dodge, or the damage our attack did
  if (battleTurn.restFailed) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_NO_REST));
  } else if (battleTurn.playerRested) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), T(S_RESTED_FMT), battleTurn.playerHeal);
  } else if (action == BATTLE_DODGE) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_DODGED));
  } else if (battleTurn.playerDamage > 0) {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), T(S_HIT_FMT), battleTurn.playerDamage);
  } else {
    snprintf(battlePlayerMoveText, sizeof(battlePlayerMoveText), "%s", T(S_MISSED));  // the wild one dodged it
  }
  #pragma endregion

  #pragma region Move of the wild one (under its sprite): dodge, or the damage its attack did
  if (battleTurn.restFailed) {
    battleWildMoveText[0] = 0;  // no turn was played
  } else if ((battleTurn.enemyAction == BATTLE_DODGE && battleRun.enemyHp > 0) ||
             (battleTurn.enemyDodged && battleTurn.enemyDamage == 0)) {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), "%s", T(S_DODGED));
  } else if (battleTurn.enemyDamage > 0) {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), T(S_HIT_FMT), battleTurn.enemyDamage);
  } else if (battleRun.enemyHp == 0) {
    battleWildMoveText[0] = 0;  // fainted before acting
  } else {
    snprintf(battleWildMoveText, sizeof(battleWildMoveText), "%s", T(S_MISSED));  // we dodged it
  }
  #pragma endregion

  if (battleTurn.battleEnded) {
    finishBattle();
    return;
  }

  #pragma region Turn sounds (queued): our move, then the hit taken, then the low HP warning
  if (battleTurn.restFailed) sfxPlay(SFX_DENY);
  else if (battleTurn.playerRested) sfxPlay(SFX_HEART);
  else if (battleTurn.playerDamage > 0) sfxPlay(SFX_BATTLE_HIT);
  else sfxPlay(SFX_TAP);
  if (battleTurn.enemyDamage > 0) sfxPlay(SFX_BATTLE_HURT);
  if (!battleLowHpWarned && battleRun.playerHp > 0 && battleRun.playerHp <= battleRun.playerMaxHp * 3 / 10) {
    battleLowHpWarned = true;
    sfxPlay(SFX_DENY);
  }
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle: touch
void battleTap(int16_t x, int16_t y) {
  #pragma region Result view: Catch / Leave, then OK
  if (battleResolved) {
    if (battleCatchOffered && !battleCatchDone) {
      if (isTouchInsidePaddedRect(x, y, BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
        bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
        battleCatchTried = true;
        battleCatchDone = true;
        if (battleRespectCatch) {
          battleCatchSuccess = pet.tryRespectCatchWild(battleDex, battleLevel, battlePlayer.level,
                                                       (uint8_t)random(100), battleShiny, !battleFromWildEncounter);
          battleCatchChance = pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level,
                                                            !battleFromWildEncounter);
        } else {
          battleCatchSuccess = pet.tryCatchWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                                (uint8_t)random(100), battleShiny, !battleFromWildEncounter);
          battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin,
                                                     !battleFromWildEncounter);
        }
        sfxPlay(battleCatchSuccess ? SFX_CATCH_OK : SFX_CATCH_FAIL);
        galleryDirty = true;        // the grid shows the new catch
        galleryFilterDirty = true;  // and the Caught / Shiny filters may now include it
        return;
      }
      if (isTouchInsidePaddedRect(x, y, BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
        battleCatchDone = true;
        battleCatchTried = false;
        sfxPlay(SFX_TAP);
      }
      return;
    }
    if (isTouchInsidePaddedRect(x, y, BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, 0, BATTLE_RESULT_HIT_PAD_Y)) {
      closeBattle();
      sfxPlay(SFX_TAP);
    }
    return;
  }
  #pragma endregion

  #pragma region Quick / Attack / Heavy overlay: any other tap closes it
  if (battleAttackMenuOpen) {
    const BattleButton *attackButton = battleButtonAt(BATTLE_ATTACK_MENU_ROW, x, y);
    if (attackButton) {
      performBattleAction(attackButton->action);
      return;
    }
    battleAttackMenuOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  #pragma endregion

  #pragma region Run, then the three action buttons
  if (isTouchInsideRect(x, y, BATTLE_RUN_HIT_X, BATTLE_RUN_HIT_Y, BATTLE_RUN_HIT_W, BATTLE_RUN_HIT_H)) {
    closeBattle();
    sfxPlay(SFX_TAP);
    return;
  }
  const BattleButton *actionButton = battleButtonAt(BATTLE_ACTION_ROW, x, y);
  if (actionButton) {
    if (actionButton->action == BATTLE_ATTACK) {
      battleAttackMenuOpen = true;
      sfxPlay(SFX_TAP);
    } else {
      performBattleAction(actionButton->action);
    }
  }
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle: drawing helpers
static void drawBattleHpBar(int x, int y, uint16_t currentHp, uint16_t maxHp, uint16_t color) {
  drawProgressBar(x, y, BATTLE_HP_W, BATTLE_HP_H, BATTLE_HP_R, currentHp, maxHp, color);
}

// "+N ATK/DEF/SPD" line for the win reward ("" when there is none)
static void battleRewardText(char *buffer, size_t bufferSize) {
  if (battleReward.amount == 0) {
    buffer[0] = 0;
    return;
  }
  StrId rewardFormat = S_SPD_GAIN_FMT;
  if (battleReward.stat == BATTLE_REWARD_ATK) rewardFormat = S_ATK_GAIN_FMT;
  else if (battleReward.stat == BATTLE_REWARD_DEF) rewardFormat = S_DEF_GAIN_FMT;
  snprintf(buffer, bufferSize, T(rewardFormat), battleReward.amount);
}

// the same pokeball as the minigame: this species is already caught
static void drawCaughtBattleMarker(int x, int y) {
  drawMap(SPR_ICON_PLAY, 16, x, y, BATTLE_CAUGHT_S, false);
}
#pragma endregion

#pragma region Wild encounter prompt drawing
void drawWildPrompt() {
  drawBorderedBox(WILD_PROMPT_X, WILD_PROMPT_Y, WILD_PROMPT_W, WILD_PROMPT_H, WILD_PROMPT_R, UI_WHITE, UI_INK);
  const int textLeft = WILD_PROMPT_X + 8, textWidth = WILD_PROMPT_W - 16;

  #pragma region Question and the wild species line
  gfx->setTextColor(UI_INK);
  drawTextCenteredInBox(T(S_WILD_Q), WILD_PROMPT_TITLE_SIZE, textLeft, WILD_PROMPT_TITLE_Y, textWidth, FONT_H * WILD_PROMPT_TITLE_SIZE);
  char wildLine[48];
  snprintf(wildLine, sizeof(wildLine), "%s%s Lv.%u", wildPromptShiny ? "*" : "", dexName(wildPromptDex), wildPromptLevel);
  drawTextCenteredInBox(wildLine, WILD_PROMPT_NAME_SIZE, textLeft, WILD_PROMPT_NAME_Y, textWidth, FONT_H * WILD_PROMPT_NAME_SIZE);
  #pragma endregion

  #pragma region FIGHT and LATER buttons (both boxes first, then both labels)
  gfx->fillRoundRect(WILD_PROMPT_BTN_X, WILD_PROMPT_FIGHT_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, WILD_PROMPT_BTN_R, UI_BAR_BAD);
  gfx->fillRoundRect(WILD_PROMPT_BTN_X, WILD_PROMPT_LATER_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H, WILD_PROMPT_BTN_R, UI_TRACK);
  gfx->setTextColor(UI_WHITE);
  drawTextCenteredInBox(T(S_FIGHT), WILD_PROMPT_BTN_TEXT_SIZE, WILD_PROMPT_BTN_X, WILD_PROMPT_FIGHT_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H);
  gfx->setTextColor(UI_INK);
  drawTextCenteredInBox(T(S_LATER), WILD_PROMPT_BTN_TEXT_SIZE, WILD_PROMPT_BTN_X, WILD_PROMPT_LATER_Y, WILD_PROMPT_BTN_W, WILD_PROMPT_BTN_H);
  #pragma endregion
}
#pragma endregion

#pragma region Wild battle screen
void renderBattle() {
  drawBattleArena();
  const uint16_t ink = UI_INK;  // the court is light sand at any hour
  const DexEntry &petEntry = DEX_TBL[pet.speciesId];
  const DexEntry &wildEntry = DEX_TBL[battleDex];

  #pragma region Title and the run button (top right, battle in progress only)
  gfx->setTextColor(ink);
  drawTextCenteredInBox(T(battleFromWildEncounter ? S_WILD_BATTLE : S_BATTLE), BATTLE_TITLE_SIZE,
                        CX - BATTLE_TITLE_MAX_W / 2, BATTLE_TITLE_Y, BATTLE_TITLE_MAX_W, FONT_H * BATTLE_TITLE_SIZE);
  if (!battleResolved) {
    drawLabeledButton(BATTLE_RUN_X, BATTLE_RUN_Y, BATTLE_RUN_W, BATTLE_RUN_H, BATTLE_RUN_R, UI_TRACK, UI_INK, T(S_RUN_BATTLE),
                      BATTLE_BTN_TEXT_SIZE);
  }
  #pragma endregion

  #pragma region Info columns: name and level, HP bar, type chips (pet left, wild right)
  // 40: the longest dex name in UTF-8 or an 11-byte nickname, plus " Lv.100"
  char petLabel[40], wildLabel[40];
  snprintf(petLabel, sizeof(petLabel), "%s Lv.%u", pet.nick[0] ? pet.nick : dexName(pet.speciesId), battlePlayer.level);
  snprintf(wildLabel, sizeof(wildLabel), "%s%s Lv.%u", battleShiny ? "*" : "", dexName(battleDex), battleLevel);
  gfx->setTextColor(ink);
  int nameSize = shrinkTextSizeToWidth(petLabel, BATTLE_NAME_SIZE, BATTLE_NAME_MAX_W);
  setCur(BATTLE_PET_NAME_X, BATTLE_NAME_Y + 4 * (BATTLE_NAME_SIZE - nameSize));
  printT(petLabel);
  nameSize = shrinkTextSizeToWidth(wildLabel, BATTLE_NAME_SIZE, BATTLE_NAME_MAX_W);
  setCur(BATTLE_WILD_NAME_RIGHT - (int)textW(wildLabel, nameSize), BATTLE_NAME_Y + 4 * (BATTLE_NAME_SIZE - nameSize));
  printT(wildLabel);

  drawBattleHpBar(BATTLE_PET_HP_X, BATTLE_HP_Y, battleRun.playerHp, battleRun.playerMaxHp, UI_BAR_OK);
  drawBattleHpBar(BATTLE_WILD_HP_X, BATTLE_HP_Y, battleRun.enemyHp, battleRun.enemyMaxHp, UI_BAR_BAD);
  drawTypeChips(BATTLE_PET_CHIP_X, BATTLE_CHIP_Y, petEntry, false);
  int wildChipsLeft = drawTypeChips(BATTLE_WILD_CHIP_RIGHT, BATTLE_CHIP_Y, wildEntry, true);
  // left of the wild type chips: next to the name it would leave the round display
  if (pet.isCaught(battleDex)) drawCaughtBattleMarker(wildChipsLeft - BATTLE_CAUGHT_DX, BATTLE_CAUGHT_Y);
  #pragma endregion

  #pragma region Sprites (idle, or the Attack pose of the last turn; thumbnail when there is no PMD sprite)
  uint32_t now = millis();
  uint32_t actionElapsedMs;
  if (pmd.loaded) {
    uint8_t petAction = battleSpriteAction(pmd, playerAttackAnimStartMs, now, actionElapsedMs);
    // keepIdleScale: the wider Attack frames would otherwise step the zoom down mid-turn
    drawPmdAct(petAction, BATTLE_PET_CX, BATTLE_GROUND_Y, actionElapsedMs, petAction == PMD_IDLE, false,
               BATTLE_SPRITE_MAX_S, true);
  } else {
    const uint8_t *petThumb = thumbs.get(pet.speciesId);
    if (petThumb) drawThumbCell(petThumb, BATTLE_PET_THUMB_X, BATTLE_THUMB_Y, BATTLE_THUMB_S, false, BATTLE_THUMB_CELL);
  }
  if (galleryPmd.loaded) {
    uint8_t wildAction = battleSpriteAction(galleryPmd, wildAttackAnimStartMs, now, actionElapsedMs);
    drawPmdActM(galleryPmd, wildAction, BATTLE_WILD_CX, BATTLE_GROUND_Y, actionElapsedMs, wildAction == PMD_IDLE, false,
                BATTLE_SPRITE_MAX_S, true);
  } else {
    const uint8_t *wildThumb = thumbs.get(battleDex);
    if (wildThumb) drawThumbCell(wildThumb, BATTLE_WILD_THUMB_X, BATTLE_THUMB_Y, BATTLE_THUMB_S, false, BATTLE_THUMB_CELL);
  }
  #pragma endregion

  if (battleResolved) {
    #pragma region Result: panel (rect board), WIN or LOSS, rounds and damage, info line
    uint16_t resultInk = ink;
    if (BATTLE_RESULT_PANEL_W > 0) {
      drawBorderedBox(BATTLE_RESULT_PANEL_X, BATTLE_RESULT_PANEL_Y, BATTLE_RESULT_PANEL_W, BATTLE_RESULT_PANEL_H,
                      BATTLE_RESULT_PANEL_R, UI_WHITE, UI_INK);
      resultInk = UI_INK;
    }
    gfx->setTextColor(battleTurn.playerWon ? UI_BAR_OK : UI_BAR_BAD);
    drawResultLine(battleTurn.playerWon ? T(S_WIN) : T(S_LOSS), BATTLE_RESULT_TITLE_SIZE, BATTLE_RESULT_TITLE_Y);
    char roundsLine[32];
    char damageLine[48];
    snprintf(roundsLine, sizeof(roundsLine), T(S_ROUNDS_FMT), (unsigned)battleRun.round);
    snprintf(damageLine, sizeof(damageLine), T(S_DAMAGE_FMT), battleRun.playerDamageTotal, battleRun.enemyDamageTotal);
    gfx->setTextColor(resultInk);
    drawResultLine(roundsLine, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_ROUNDS_Y);
    drawResultLine(damageLine, BATTLE_RESULT_LINE_SIZE, BATTLE_RESULT_DAMAGE_Y);
    if (battleCatchDone && battleCatchTried) {
      gfx->setTextColor(battleCatchSuccess ? UI_BAR_OK : UI_BAR_BAD);
      drawResultLine(battleCatchSuccess ? T(S_CAUGHT_OK) : T(S_ESCAPED), BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
    } else if (battleTurn.playerWon) {
      char rewardLine[48];
      battleRewardText(rewardLine, sizeof(rewardLine));
      if (rewardLine[0]) {
        gfx->setTextColor(UI_BAR_WARN);
        drawResultLine(rewardLine, BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
      }
    } else if (battleRespectCatch && battleCatchOffered && !battleCatchDone) {
      gfx->setTextColor(UI_BAR_WARN);
      drawResultLine(T(S_CLOSE_CHANCE), BATTLE_RESULT_INFO_SIZE, BATTLE_RESULT_INFO_Y);
    }
    #pragma endregion

    #pragma region Result buttons: Catch and Leave while a catch is offered (both boxes first), then OK
    if (battleCatchOffered && !battleCatchDone) {
      gfx->fillRoundRect(BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK);
      gfx->fillRoundRect(BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_TRACK);
      gfx->setTextColor(UI_BG_DAY);
      drawTextCenteredInBox(T(S_CATCH_WILD), BATTLE_BTN_TEXT_SIZE, BATTLE_CATCH_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H);
      gfx->setTextColor(UI_INK);
      drawTextCenteredInBox(T(S_LEAVE_WILD), BATTLE_BTN_TEXT_SIZE, BATTLE_LEAVE_X, BATTLE_RESULT_BTN_Y, BATTLE_CATCH_W, BATTLE_RESULT_BTN_H);
    } else {
      drawLabeledButton(BATTLE_OK_X, BATTLE_RESULT_BTN_Y, BATTLE_OK_W, BATTLE_RESULT_BTN_H, BATTLE_RESULT_BTN_R, UI_BAR_OK,
                        UI_BG_DAY, T(S_OK), BATTLE_OK_TEXT_SIZE);
    }
    #pragma endregion
  } else {
    #pragma region Moves of the last turn, each centered under its sprite (damage to us in red)
    if (battlePlayerMoveText[0]) {
      gfx->setTextColor(ink);
      drawTextCenteredInBox(battlePlayerMoveText, BATTLE_MOVE_SIZE, BATTLE_PET_CX - BATTLE_MOVE_W / 2, BATTLE_MOVE_Y,
                            BATTLE_MOVE_W, FONT_H * BATTLE_MOVE_SIZE);
    }
    if (battleWildMoveText[0]) {
      gfx->setTextColor(battleTurn.enemyDamage > 0 ? UI_BAR_BAD : ink);
      drawTextCenteredInBox(battleWildMoveText, BATTLE_MOVE_SIZE, BATTLE_WILD_CX - BATTLE_MOVE_W / 2, BATTLE_MOVE_Y,
                            BATTLE_MOVE_W, FONT_H * BATTLE_MOVE_SIZE);
    }
    #pragma endregion

    #pragma region Quick Attack Heavy overlay (after Attack)
    if (battleAttackMenuOpen) drawBattleButtonRow(BATTLE_ATTACK_MENU_ROW);
    #pragma endregion

    #pragma region Attack Dodge Rest action row
    drawBattleButtonRow(BATTLE_ACTION_ROW);
    #pragma endregion
  }

  gfx->flush();
}
#pragma endregion

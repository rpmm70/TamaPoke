#pragma once
#include <Arduino.h>

// Supported languages. The Arduino_GFX 5x7 font is a full CP437 table
// (256 glyphs), so accents, umlauts and n-tildes ARE available: they go as a
// single octal byte (\240 a, \244 n~, \202 e, \204 a"...), never UTF-8, because
// the UI centers with strlen(t)*6 and a two-byte character would misalign labels.
// Not in CP437: accented capitals except E (\220), and Portuguese a~/o~.
enum Lang : uint8_t { LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT,
                     LANG_JA, LANG_KO, LANG_COUNT };
// languages that need a U8g2 font and UTF-8 text instead of the CP437 table
#define LANG_IS_CJK(l) ((l) == LANG_JA || (l) == LANG_KO)
#define LANG_DEFAULT LANG_EN  // default language: English

extern Lang gLang;  // active language (defined in i18n.cpp)

// String IDs. The order must match the STRINGS table in i18n.cpp.
enum StrId : uint8_t {
  // creature status (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // farewell ceremonies
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // egg
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // shared formats
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // release dialog
  S_RELEASE_FMT, S_YES, S_NO,
  // minigame and punching bag
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_GREAT_JOY, S_PLUS_JOY,
  // clock / settings
  S_SET_TIME, S_HOUR, S_MIN, S_LANG_LABEL,
  // celebration
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // info card: profile
  S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  // info card: profile stat rows (labels, then the age value "%lud")
  S_CARD_LEVEL, S_CARD_STREAK_BEST, S_CARD_BOND, S_CARD_FAV_FOOD, S_CARD_AGE,
  S_CARD_AGE_FMT,
  // info card: battle
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_TRAIN_STR,
  // info card: medals
  S_MEDALS_FMT,
  // keyboard and gallery
  S_NAME, S_DETAIL_BACK,
  // bars
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // live minigame scoreboard
  S_REC_FMT,
  // info card: progress page
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // sound switch (settings)
  S_SND_ON, S_SND_OFF,
  S_EVO_TAP,        // evolution button text
  S_FAREWELL_BTN,   // farewell button text (includes the name: "%s ...")
  S_RUNAWAY_BTN,    // runaway-from-neglect button text (sad ending)
  // decision dialogs (evolve/keep, say goodbye/stay together)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // title of the starter choice (first time)
  S_NO_SPRITES, S_LOAD_SPRITES,  // notice when the sprite is missing from the SD
  // settings menu: title, the three section rows, then the row labels and toggle values
  S_MENU_TITLE, S_MENU_AV, S_MENU_TIME, S_MENU_LANG,
  S_SOUND, S_BRIGHTNESS, S_ON, S_OFF,
  STR_COUNT
};

const char *T(StrId id);       // text in the active language
const char *medalName(int i);  // medal banner (MED_COUNT)
const char *medalLabel(int i); // short medal label
const char *medalDesc(int i);  // long medal description

void loadLang();             // reads the language from NVS (call in setup)
void setLang(Lang l);        // changes and persists the language

// Consistency of the translations (i18n.cpp).

#pragma region Includes
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../i18n.h"
#include "../pet.h"  // MED_COUNT
#include <ctype.h>
#include <string.h>
#include <vector>
#pragma endregion

#pragma region Format signature helpers
static const char *LANG_NAME[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

// returned by joinSpecs() for a string without any printf specifier
#define NO_FORMAT_SIGNATURE "(no format)"

// extracts the printf specifier signature of a string: "%lud" -> "lu"
static std::vector<std::string> specs(const char *text) {
  std::vector<std::string> signatures;
  const char *cursor = text;
  while (*cursor) {
    if (*cursor != '%') { cursor++; continue; }
    cursor++;
    if (*cursor == '%') { cursor++; continue; }   // %% is a literal
    std::string signature;
    while (*cursor && strchr("-+ #0", *cursor)) cursor++;             // flags
    while (*cursor && isdigit((unsigned char)*cursor)) cursor++;      // width
    if (*cursor == '.') { cursor++; while (*cursor && isdigit((unsigned char)*cursor)) cursor++; }
    while (*cursor && strchr("lhzjt", *cursor)) { signature += *cursor; cursor++; }  // modifier
    if (!*cursor) { signatures.push_back(signature + "<incomplete>"); break; }
    signature += *cursor++;
    signatures.push_back(signature);
  }
  return signatures;
}

static std::string joinSpecs(const std::vector<std::string> &signatures) {
  std::string joined;
  for (size_t i = 0; i < signatures.size(); i++) { if (i) joined += ","; joined += "%" + signatures[i]; }
  return joined.empty() ? NO_FORMAT_SIGNATURE : joined;
}
#pragma endregion

#pragma region Expected signatures from the sketch's calls
// expected signature of each format string, taken from the sketch's calls
struct ExpectedFmt { StrId id; const char *sig; };
static const ExpectedFmt EXPECTED[] = {
  { S_POKEDEX_FMT,     "%u" },
  { S_RELEASE_FMT,     "%s" },
  { S_HITS_FMT,        "%u" },
  { S_RECORD_FMT,      "%u" },
  { S_SCORE_FMT,       "%u" },
  { S_STREAK_DAYS_FMT, "%u" },
  { S_CARD_AGE_FMT,    "%lu" },
  { S_REC_FMT,         "%u" },
  { S_LVL_FMT,         "%u" },
  { S_NEXT_LVL_FMT,    "%u,%u" },
  { S_EVO_IN_FMT,      "%u" },
  { S_MISTAKES_FMT,    "%u" },
  { S_FAREWELL_BTN,    "%s" },
  { S_RUNAWAY_BTN,     "%s" },
  // Wild battle formats (a comment, not a region: GCC rejects #pragma inside an initializer)
  { S_ROUNDS_FMT,      "%u" },
  { S_DAMAGE_FMT,      "%u,%u" },
  { S_ROUND_SHORT_FMT, "%u" },
  { S_HIT_FMT,         "%u" },
  { S_RESTED_FMT,      "%u" },
  { S_ATK_GAIN_FMT,    "%u" },
  { S_DEF_GAIN_FMT,    "%u" },
  { S_SPD_GAIN_FMT,    "%u" },
};
static const int NUM_EXPECTED = (int)(sizeof(EXPECTED) / sizeof(EXPECTED[0]));

static const char *expectedSig(int id) {
  for (int i = 0; i < NUM_EXPECTED; i++)
    if ((int)EXPECTED[i].id == id) return EXPECTED[i].sig;
  return NO_FORMAT_SIGNATURE;
}
#pragma endregion

#pragma region String tables
TEST(i18n, no_language_has_gaps) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *text = T((StrId)id);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], id);
      CHECK_MSG(text != NULL, where);
      if (text) CHECK_MSG(text[0] != 0, std::string("empty string ") + where);
    }
  }
  gLang = LANG_DEFAULT;
}

// The firmware font (Arduino_GFX's glcdfont.h) is a 256-glyph CP437 table,
// so it DOES support accents, but each one must take ONE byte: the UI
// centers with strlen(t)*6, and a two-byte UTF-8 character would misalign the
// label. This test does not forbid high bytes, it forbids UTF-8.
TEST(i18n, accents_take_one_byte_not_utf8) {
  // multibyte UTF-8 lead bytes: 0xC2-0xDF, 0xE0-0xEF, 0xF0-0xF4
  auto isUtf8LeadByte = [](unsigned char byteValue) {
    return (byteValue >= 0xC2 && byteValue <= 0xDF) || (byteValue >= 0xE0 && byteValue <= 0xEF) ||
           (byteValue >= 0xF0 && byteValue <= 0xF4);
  };
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // CJK is UTF-8 on purpose, with a U8g2 font
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *text = T((StrId)id);
      if (!text) continue;
      for (const char *character = text; *character; character++) {
        unsigned char leadByte = (unsigned char)*character;
        if (leadByte < 128) continue;              // ASCII: always fine
        if (!isUtf8LeadByte(leadByte)) continue;   // lone high byte: it is CP437, fine
        // a UTF-8 lead byte is only suspicious if followed by a continuation
        // byte (0x80-0xBF): then it is two glyphs where there should be one
        unsigned char nextByte = (unsigned char)*(character + 1);
        char message[200];
        snprintf(message, sizeof(message),
                 "looks like UTF-8 (0x%02X 0x%02X) in [%s][id %d]: accents go in octal, "
                 "one byte per character, or the centering breaks", leadByte, nextByte,
                 LANG_NAME[lang], id);
        CHECK_MSG(!(nextByte >= 0x80 && nextByte <= 0xBF), message);
      }
    }
  }
  gLang = LANG_DEFAULT;
}

// one %u too many or too few in a translation is a garbage read from the stack
TEST(i18n, formats_match_across_all_languages) {
  for (int id = 0; id < STR_COUNT; id++) {
    gLang = LANG_EN;
    std::vector<std::string> ref = specs(T((StrId)id));
    for (int lang = 0; lang < LANG_COUNT; lang++) {
      gLang = (Lang)lang;
      std::vector<std::string> got = specs(T((StrId)id));
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s instead of %s (EN)", LANG_NAME[lang], id,
               T((StrId)id), joinSpecs(got).c_str(), joinSpecs(ref).c_str());
      CHECK_MSG(got == ref, msg);
    }
  }
  gLang = LANG_DEFAULT;
}

// ...and they also have to match what the sketch passes at each call
TEST(i18n, formats_match_the_sketch_calls) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      std::string got = joinSpecs(specs(T((StrId)id)));
      const char *want = expectedSig(id);
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s, expected %s", LANG_NAME[lang], id,
               T((StrId)id), got.c_str(), want);
      CHECK_MSG(got == want, msg);
    }
  }
  gLang = LANG_DEFAULT;
}
#pragma endregion

#pragma region Medals and menu fit
TEST(i18n, medals_exist_in_every_language) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    // the length caps are in BYTES, which only equal characters with the
    // CP437 font. In CJK each kana takes 3 bytes and the real limit is on-screen
    // width, which cannot be measured here: that is checked on the board.
    const bool cjk = LANG_IS_CJK(lang);
    for (int m = 0; m < MED_COUNT; m++) {
      const char *name = medalName(m), *label = medalLabel(m), *description = medalDesc(m);
      char where[64];
      snprintf(where, sizeof(where), "[%s][medal %d]", LANG_NAME[lang], m);
      CHECK_MSG(name && name[0], std::string("empty medalName ") + where);
      CHECK_MSG(label && label[0], std::string("empty medalLabel ") + where);
      CHECK_MSG(description && description[0], std::string("empty medalDesc ") + where);
      // the short label goes in one cell of the medal grid
      if (!cjk && label) CHECK_MSG(strlen(label) <= 6, std::string("label too long ") + where + ": " + label);
      if (!cjk && name) CHECK_MSG(strlen(name) <= 12, std::string("name too long ") + where + ": " + name);
      if (!cjk && description)
        CHECK_MSG(strlen(description) <= 16, std::string("description too long ") + where + ": " + description);
    }
  }
  gLang = LANG_DEFAULT;
}

// Settings menu rows are drawn at text size 2 (12 px per CP437 glyph) on a
// 320 px wide screen, so a row holds 26 characters at most. The On/Off values
// sit at the right edge of a row next to its label and must stay very short.
// CJK is skipped for the same reason as the medal caps: bytes are not width.
TEST(i18n, settings_menu_fits_on_screen) {
  const StrId menuRowIds[] = { S_MENU_TITLE, S_MENU_AV, S_MENU_TIME, S_MENU_LANG,
                               S_SOUND, S_BRIGHTNESS };
  const StrId toggleValueIds[] = { S_ON, S_OFF };
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;
    gLang = (Lang)lang;
    for (StrId rowId : menuRowIds) {
      const char *rowText = T(rowId);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], (int)rowId);
      CHECK_MSG(strlen(rowText) <= 26, std::string("menu row too long ") + where + ": " + rowText);
    }
    for (StrId valueId : toggleValueIds) {
      const char *valueText = T(valueId);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], (int)valueId);
      CHECK_MSG(strlen(valueText) <= 4, std::string("On/Off value too long ") + where + ": " + valueText);
    }
  }
  gLang = LANG_DEFAULT;
}
#pragma endregion

#pragma region Wild battle strings and type labels
// The wild battle block is appended in a fixed order (see i18n.h). Every entry
// must be filled in every language, and the *_FMT ones must carry exactly the
// specifiers the battle UI passes.
TEST(i18n, wild_battle_strings_are_filled_with_the_expected_formats) {
  struct BattleStringCheck { StrId id; const char *expectedSignature; };
  const BattleStringCheck battleStringChecks[] = {
    { S_WILD_BATTLE, NO_FORMAT_SIGNATURE }, { S_WILD_Q, NO_FORMAT_SIGNATURE },
    { S_FIGHT, NO_FORMAT_SIGNATURE }, { S_LATER, NO_FORMAT_SIGNATURE },
    { S_RUN_BATTLE, NO_FORMAT_SIGNATURE }, { S_ATTACK, NO_FORMAT_SIGNATURE },
    { S_DODGE, NO_FORMAT_SIGNATURE }, { S_REST, NO_FORMAT_SIGNATURE },
    { S_QUICK_ATTACK, NO_FORMAT_SIGNATURE }, { S_HEAVY_ATTACK, NO_FORMAT_SIGNATURE },
    { S_WIN, NO_FORMAT_SIGNATURE }, { S_LOSS, NO_FORMAT_SIGNATURE },
    { S_ROUNDS_FMT, "%u" }, { S_DAMAGE_FMT, "%u,%u" }, { S_ROUND_SHORT_FMT, "%u" },
    { S_HIT_FMT, "%u" }, { S_EFFECTIVE, NO_FORMAT_SIGNATURE },
    { S_NOT_EFFECTIVE, NO_FORMAT_SIGNATURE }, { S_MISSED, NO_FORMAT_SIGNATURE },
    { S_DODGED, NO_FORMAT_SIGNATURE }, { S_ENEMY_DODGED, NO_FORMAT_SIGNATURE },
    { S_COUNTER_READY, NO_FORMAT_SIGNATURE }, { S_NO_REST, NO_FORMAT_SIGNATURE },
    { S_RESTED_FMT, "%u" }, { S_GUARD, NO_FORMAT_SIGNATURE },
    { S_CATCH_WILD, NO_FORMAT_SIGNATURE }, { S_LEAVE_WILD, NO_FORMAT_SIGNATURE },
    { S_CAUGHT_OK, NO_FORMAT_SIGNATURE }, { S_ESCAPED, NO_FORMAT_SIGNATURE },
    { S_CLOSE_CHANCE, NO_FORMAT_SIGNATURE },
    { S_ATK_GAIN_FMT, "%u" }, { S_DEF_GAIN_FMT, "%u" }, { S_SPD_GAIN_FMT, "%u" },
    { S_OK, NO_FORMAT_SIGNATURE }, { S_CAUGHT_MARK, NO_FORMAT_SIGNATURE },
  };
  const int battleStringCount = (int)(sizeof(battleStringChecks) / sizeof(battleStringChecks[0]));
  #pragma region The block is complete and in order
  CHECK_EQ(battleStringCount, 35);
  for (int i = 0; i < battleStringCount; i++)
    CHECK_EQ((int)battleStringChecks[i].id, (int)S_WILD_BATTLE + i);
  #pragma endregion
  #pragma region Every language has text and the right specifiers
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (const BattleStringCheck &stringCheck : battleStringChecks) {
      const char *text = T(stringCheck.id);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], (int)stringCheck.id);
      CHECK_MSG(text != NULL && text[0] != 0, std::string("empty battle string ") + where);
      if (!text) continue;
      std::string gotSignature = joinSpecs(specs(text));
      CHECK_MSG(gotSignature == stringCheck.expectedSignature,
                std::string("battle format ") + where + " \"" + text + "\": " + gotSignature +
                ", expected " + stringCheck.expectedSignature);
    }
  }
  gLang = LANG_DEFAULT;
  #pragma endregion
}

// Battle buttons on the 320x240 screen hold 7 CP437 characters at most.
// CJK is skipped: bytes are not on-screen width there (checked on the board).
TEST(i18n, wild_battle_button_labels_fit_in_seven_characters) {
  const StrId buttonLabelIds[] = { S_FIGHT, S_LATER, S_RUN_BATTLE, S_ATTACK, S_DODGE, S_REST,
                                   S_CATCH_WILD, S_LEAVE_WILD, S_OK, S_QUICK_ATTACK,
                                   S_HEAVY_ATTACK };
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;
    gLang = (Lang)lang;
    for (StrId labelId : buttonLabelIds) {
      const char *labelText = T(labelId);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], (int)labelId);
      CHECK_MSG(strlen(labelText) <= 7, std::string("battle button label too long ") + where + ": " + labelText);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, type_names_are_filled_in_every_language) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    char where[64];
    snprintf(where, sizeof(where), "[%s]", LANG_NAME[lang]);
    #pragma region TYPE_NONE and out of range give an empty label
    CHECK_MSG(typeName(0) != NULL && typeName(0)[0] == 0, std::string("TYPE_NONE not empty ") + where);
    CHECK_MSG(typeName(TYPE_NAME_COUNT) != NULL && typeName(TYPE_NAME_COUNT)[0] == 0,
              std::string("out of range type not empty ") + where);
    CHECK_MSG(typeName(255) != NULL && typeName(255)[0] == 0, std::string("type 255 not empty ") + where);
    #pragma endregion
    #pragma region Every real type has a short label without format specifiers
    for (int type = 1; type < TYPE_NAME_COUNT; type++) {
      const char *label = typeName((uint8_t)type);
      snprintf(where, sizeof(where), "[%s][type %d]", LANG_NAME[lang], type);
      CHECK_MSG(label != NULL && label[0] != 0, std::string("empty type label ") + where);
      if (!label) continue;
      CHECK_MSG(strchr(label, '%') == NULL, std::string("type label has a % ") + where);
      if (!LANG_IS_CJK(lang))
        CHECK_MSG(strlen(label) <= 8, std::string("type label too long ") + where + ": " + label);
    }
    #pragma endregion
  }
  gLang = LANG_DEFAULT;
}
#pragma endregion

#pragma region Medal accents and language persistence
TEST(i18n, medals_have_no_accents) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // CJK is UTF-8 with a U8g2 font
    gLang = (Lang)lang;
    for (int m = 0; m < MED_COUNT; m++) {
      const char *medalTexts[3] = { medalName(m), medalLabel(m), medalDesc(m) };
      for (int t = 0; t < 3; t++)
        for (const char *character = medalTexts[t]; character && *character; character++)
          CHECK_MSG((unsigned char)*character < 128, std::string("not ASCII: ") + medalTexts[t]);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, language_is_saved_and_restored) {
  mockNvsReset();
  setLang(LANG_DE);
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_DEFAULT;
}

TEST(i18n, without_a_saved_setting_starts_in_the_default_language) {
  mockNvsReset();
  gLang = LANG_IT;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, invalid_language_in_nvs_breaks_nothing) {
  mockNvsReset();
  Preferences preferences;
  preferences.begin("tamapoke", false);
  preferences.putUChar("lang", 200);  // corrupt value
  preferences.end();
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, setLang_rejects_out_of_range_values) {
  mockNvsReset();
  setLang(LANG_FR);
  setLang((Lang)LANG_COUNT);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  setLang((Lang)250);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  gLang = LANG_DEFAULT;
}
#pragma endregion

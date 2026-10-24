// Consistency of the translations (i18n.cpp).
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../i18n.h"
#include "../pet.h"  // MED_COUNT
#include <ctype.h>
#include <string.h>
#include <vector>

static const char *LANG_NAME[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

// extracts the printf specifier signature of a string: "%lud" -> "lu"
static std::vector<std::string> specs(const char *s) {
  std::vector<std::string> out;
  const char *p = s;
  while (*p) {
    if (*p != '%') { p++; continue; }
    p++;
    if (*p == '%') { p++; continue; }   // %% is a literal
    std::string sig;
    while (*p && strchr("-+ #0", *p)) p++;             // flags
    while (*p && isdigit((unsigned char)*p)) p++;      // width
    if (*p == '.') { p++; while (*p && isdigit((unsigned char)*p)) p++; }
    while (*p && strchr("lhzjt", *p)) { sig += *p; p++; }  // modifier
    if (!*p) { out.push_back(sig + "<incompleto>"); break; }
    sig += *p++;
    out.push_back(sig);
  }
  return out;
}

static std::string joinSpecs(const std::vector<std::string> &v) {
  std::string s;
  for (size_t i = 0; i < v.size(); i++) { if (i) s += ","; s += "%" + v[i]; }
  return s.empty() ? "(sin formato)" : s;
}

// expected signature of each format string, taken from the sketch's calls
struct ExpectedFmt { StrId id; const char *sig; };
static const ExpectedFmt EXPECTED[] = {
  { S_POKEDEX_FMT,     "%u" },
  { S_NAME_FMT,        "%s,%s,%u" },
  { S_RELEASE_FMT,     "%s" },
  { S_HITS_FMT,        "%u" },
  { S_STR_GAIN_FMT,    "%u" },
  { S_RECORD_FMT,      "%u" },
  { S_SCORE_FMT,       "%u" },
  { S_STREAK_DAYS_FMT, "%u" },
  { S_CARD_AGE_FMT,    "%lu" },
  { S_MEDALS_FMT,      "%d,%d" },
  { S_REC_FMT,         "%u" },
  { S_LVL_FMT,         "%u" },
  { S_NEXT_LVL_FMT,    "%u,%u" },
  { S_EVO_IN_FMT,      "%u" },
  { S_MISTAKES_FMT,    "%u" },
  { S_FAREWELL_BTN,    "%s" },
  { S_RUNAWAY_BTN,     "%s" },
};
static const int NUM_EXPECTED = (int)(sizeof(EXPECTED) / sizeof(EXPECTED[0]));

static const char *expectedSig(int id) {
  for (int i = 0; i < NUM_EXPECTED; i++)
    if ((int)EXPECTED[i].id == id) return EXPECTED[i].sig;
  return "(sin formato)";
}

TEST(i18n, ningun_idioma_tiene_huecos) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *s = T((StrId)id);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], id);
      CHECK_MSG(s != NULL, where);
      if (s) CHECK_MSG(s[0] != 0, std::string("cadena vacia ") + where);
    }
  }
  gLang = LANG_DEFAULT;
}

// The firmware font (Arduino_GFX's glcdfont.h) is a 256-glyph CP437 table,
// so it DOES support accents, but each one must take ONE byte: the UI
// centers with strlen(t)*6, and a two-byte UTF-8 character would misalign the
// label. This test does not forbid high bytes, it forbids UTF-8.
TEST(i18n, los_acentos_van_en_un_solo_byte_no_en_utf8) {
  // multibyte UTF-8 lead bytes: 0xC2-0xDF, 0xE0-0xEF, 0xF0-0xF4
  auto arranqueUtf8 = [](unsigned char c) {
    return (c >= 0xC2 && c <= 0xDF) || (c >= 0xE0 && c <= 0xEF) || (c >= 0xF0 && c <= 0xF4);
  };
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // CJK is UTF-8 on purpose, with a U8g2 font
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *s = T((StrId)id);
      if (!s) continue;
      for (const char *c = s; *c; c++) {
        unsigned char u = (unsigned char)*c;
        if (u < 128) continue;                 // ASCII: always fine
        if (!arranqueUtf8(u)) continue;        // lone high byte: it is CP437, fine
        // a UTF-8 lead byte is only suspicious if followed by a continuation
        // byte (0x80-0xBF): then it is two glyphs where there should be one
        unsigned char sig = (unsigned char)*(c + 1);
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "parece UTF-8 (0x%02X 0x%02X) en [%s][id %d]: los acentos van en octal, "
                 "un byte por caracter, o se descuadra el centrado", u, sig, LANG_NAME[lang], id);
        CHECK_MSG(!(sig >= 0x80 && sig <= 0xBF), msg);
      }
    }
  }
  gLang = LANG_DEFAULT;
}

// one %u too many or too few in a translation is a garbage read from the stack
TEST(i18n, los_formatos_coinciden_en_todos_los_idiomas) {
  for (int id = 0; id < STR_COUNT; id++) {
    gLang = LANG_EN;
    std::vector<std::string> ref = specs(T((StrId)id));
    for (int lang = 0; lang < LANG_COUNT; lang++) {
      gLang = (Lang)lang;
      std::vector<std::string> got = specs(T((StrId)id));
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s en vez de %s (EN)", LANG_NAME[lang], id,
               T((StrId)id), joinSpecs(got).c_str(), joinSpecs(ref).c_str());
      CHECK_MSG(got == ref, msg);
    }
  }
  gLang = LANG_DEFAULT;
}

// ...and they also have to match what the sketch passes at each call
TEST(i18n, los_formatos_cuadran_con_las_llamadas_del_sketch) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      std::string got = joinSpecs(specs(T((StrId)id)));
      const char *want = expectedSig(id);
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s, se esperaba %s", LANG_NAME[lang], id,
               T((StrId)id), got.c_str(), want);
      CHECK_MSG(got == want, msg);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, las_medallas_estan_en_todos_los_idiomas) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    // the length caps are in BYTES, which only equal characters with the
    // CP437 font. In CJK each kana takes 3 bytes and the real limit is on-screen
    // width, which cannot be measured here: that is checked on the board.
    const bool cjk = LANG_IS_CJK(lang);
    for (int m = 0; m < MED_COUNT; m++) {
      const char *n = medalName(m), *l = medalLabel(m), *d = medalDesc(m);
      char where[64];
      snprintf(where, sizeof(where), "[%s][medalla %d]", LANG_NAME[lang], m);
      CHECK_MSG(n && n[0], std::string("medalName vacio ") + where);
      CHECK_MSG(l && l[0], std::string("medalLabel vacio ") + where);
      CHECK_MSG(d && d[0], std::string("medalDesc vacio ") + where);
      // the short label goes in one cell of the medal grid
      if (!cjk && l) CHECK_MSG(strlen(l) <= 6, std::string("etiqueta demasiado larga ") + where + ": " + l);
      if (!cjk && n) CHECK_MSG(strlen(n) <= 12, std::string("nombre demasiado largo ") + where + ": " + n);
      if (!cjk && d) CHECK_MSG(strlen(d) <= 16, std::string("descripcion demasiado larga ") + where + ": " + d);
    }
  }
  gLang = LANG_DEFAULT;
}

// Settings menu rows are drawn at text size 2 (12 px per CP437 glyph) on a
// 320 px wide screen, so a row holds 26 characters at most. The On/Off values
// sit at the right edge of a row next to its label and must stay very short.
// CJK is skipped for the same reason as the medal caps: bytes are not width.
TEST(i18n, el_menu_de_ajustes_cabe_en_pantalla) {
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
      CHECK_MSG(strlen(rowText) <= 26, std::string("fila de menu demasiado larga ") + where + ": " + rowText);
    }
    for (StrId valueId : toggleValueIds) {
      const char *valueText = T(valueId);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], (int)valueId);
      CHECK_MSG(strlen(valueText) <= 4, std::string("valor On/Off demasiado largo ") + where + ": " + valueText);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, las_medallas_no_llevan_acentos) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // CJK is UTF-8 with a U8g2 font
    gLang = (Lang)lang;
    for (int m = 0; m < MED_COUNT; m++) {
      const char *tab[3] = { medalName(m), medalLabel(m), medalDesc(m) };
      for (int t = 0; t < 3; t++)
        for (const char *c = tab[t]; c && *c; c++)
          CHECK_MSG((unsigned char)*c < 128, std::string("no ASCII: ") + tab[t]);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, el_idioma_se_guarda_y_se_recupera) {
  mockNvsReset();
  setLang(LANG_DE);
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_DEFAULT;
}

TEST(i18n, sin_ajuste_guardado_arranca_en_el_idioma_por_defecto) {
  mockNvsReset();
  gLang = LANG_IT;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, un_idioma_invalido_en_la_nvs_no_rompe_nada) {
  mockNvsReset();
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("lang", 200);  // corrupt value
  p.end();
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, setLang_rechaza_valores_fuera_de_rango) {
  mockNvsReset();
  setLang(LANG_FR);
  setLang((Lang)LANG_COUNT);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  setLang((Lang)250);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  gLang = LANG_DEFAULT;
}

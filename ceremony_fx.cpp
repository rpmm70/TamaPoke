// Big moments on the main screen (see ceremony_fx.h): farewell / runaway
// ceremony, evolution FX, the decision dialog and the pulsing CTA buttons.
// The FX distances are 466-px design values scaled with SX()/SY()/SK() (an exact
// no-op on the 466x466 round board).

#pragma region Includes
#include "ceremony_fx.h"
#include <math.h>
#include "app_state.h"      // pet, pmd, evoPmd, choiceKind, CHOICE_EVOLVE, PET_GROUND
#include "board_display.h"  // gfx
#include "boards/layout.h"  // CX, SX()/SY()/SK(), CH_*, EVO_*, FAR_*, PET_HEART_SCALE
#include "ceremony.h"       // CER_RUNAWAY
#include "colors.h"
#include "dex.h"            // dexName
#include "i18n.h"           // T()
#include "pet_render.h"     // drawPet
#include "species.h"        // SPR_HEART
#include "sprite_draw.h"
#include "ui_text.h"
#include "ui_widgets.h"     // drawBorderedBox, drawPulsingCtaButton, sineOfMillis, millisPhaseRadians
#pragma endregion

#pragma region Constants
// zoom limit of the creature during the ceremonies and the evolution
static const uint8_t CEREMONY_PET_MAX_SCALE = 5;
static const int HEART_MAP_SIZE = 32;
// corner radius of the two buttons inside the decision dialog
static const int CHOICE_BUTTON_RADIUS = 12;
// horizontal room left for a decision dialog line / button label
static const int CHOICE_QUESTION_SIDE_ROOM = 16;
static const int CHOICE_BUTTON_SIDE_ROOM = 12;
#pragma endregion

#pragma region Ceremony colors
#define CEREMONY_RAIN_COLOR C565(0x6a, 0x84, 0xb0)
#define CEREMONY_TEAR_COLOR C565(0x9a, 0xc4, 0xe8)
#define CEREMONY_HALO_COLOR C565(0xff, 0xdf, 0x8a)
#define CEREMONY_SPARK_GOLD_COLOR C565(0xff, 0xe7, 0x9f)
#define CEREMONY_SPARK_PINK_COLOR C565(0xff, 0x9a, 0xc0)
#define EVOLVE_SPARK_COLOR C565(0xff, 0xe0, 0x70)
#define RUNAWAY_BUTTON_FILL_COLOR C565(0x3a, 0x44, 0x5a)
#define RUNAWAY_BUTTON_BORDER_COLOR C565(0x70, 0x80, 0x98)
#define RUNAWAY_BUTTON_TEXT_COLOR C565(0xc8, 0xd2, 0xe0)
#pragma endregion

#pragma region Ceremony
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // fallback if there is no PMD sprite
  uint32_t now = millis();
  float ceremonyProgress = pet.ceremonyT();  // 0..1 over the 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int creatureX = CX, creatureGroundY = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    #pragma region Sad ending: bluish gloom and rain
    for (int i = 0; i < 46; i++) {
      int rainX = (i * 47 + now / 3) % LCD_WIDTH;
      int rainY = (i * 91 + now / 2) % (LCD_HEIGHT + 4);
      gfx->drawLine(rainX, rainY, rainX - 3, rainY + 12, CEREMONY_RAIN_COLOR);
    }
    #pragma endregion
    #pragma region Head down and trembling, then it walks away slowly and fades out
    bool fade = false;
    if (ceremonyProgress < 0.30f) {
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      creatureX = CX + (int)(4 * sineOfMillis(now, 0.04f));
    } else {
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      creatureX = CX - (int)(((ceremonyProgress - 0.30f) / 0.70f) * (CX + SX(120)));
      fade = (ceremonyProgress > 0.6f) && ((now / 160) % 2 == 0);  // flickers toward the silhouette
    }
    drawPmdAct(act, creatureX, creatureGroundY, now, true, fade, CEREMONY_PET_MAX_SCALE);  // fade=silhouette: it blurs as it leaves
    #pragma endregion
    #pragma region Tear falling from the creature
    if (ceremonyProgress < 0.55f) {
      int tearY = creatureGroundY - SY(150) + (int)((now / 6) % 40);
      gfx->fillRect(creatureX + 6, tearY, 3, 6, CEREMONY_TEAR_COLOR);
    }
    #pragma endregion
    return;
  }

  #pragma region Epic farewell: pulsing golden halo, rising sparks and hearts
  int haloCenterY = PET_GROUND - SY(96);
  for (int k = 0; k < 4; k++) {
    int radius = SY(60) + k * SY(34) + (int)(10 * sineOfMillis(now, 0.02f));
    gfx->drawCircle(CX, haloCenterY, radius, CEREMONY_HALO_COLOR);
  }
  for (int i = 0; i < 16; i++) {
    int sparkX = (i * 71 + 28) % LCD_WIDTH;
    int sparkY = SY(410) - (int)((now / 8 + i * 70) % SY(360));  // rise and reappear at the bottom
    if (sparkY < SY(30)) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, HEART_MAP_SIZE, sparkX - 8, sparkY - 8, 1, false);  // little heart
    else gfx->fillRect(sparkX, sparkY, 4, 4, (i % 2) ? CEREMONY_SPARK_GOLD_COLOR : CEREMONY_SPARK_PINK_COLOR);
  }
  #pragma endregion

  #pragma region Bow, then it walks off to the right
  if (ceremonyProgress < 0.45f) {
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    creatureX = CX + (int)(((ceremonyProgress - 0.45f) / 0.55f) * (CX + SX(140)));
  }
  drawPmdAct(act, creatureX, creatureGroundY, now, true, false, CEREMONY_PET_MAX_SCALE);
  if (pet.showHeart())  // big heart following the creature
    drawMap(SPR_HEART, HEART_MAP_SIZE, creatureX + SX(50), creatureGroundY - SY(190), PET_HEART_SCALE, false);
  #pragma endregion
}
#pragma endregion

#pragma region Decision dialog
void drawChoiceDialog() {
  const char *question, *actionLabel, *keepLabel;
  uint16_t actionFillColor, keepFillColor, actionTextColor, keepTextColor;
  if (choiceKind == CHOICE_EVOLVE) {
    question = T(S_EVO_Q); actionLabel = T(S_EVO_TAP); keepLabel = T(S_EVO_KEEP);
    actionFillColor = UI_BAR_BAD; actionTextColor = UI_WHITE; keepFillColor = UI_TRACK; keepTextColor = UI_INK;
  } else {  // farewell
    question = T(S_FAR_Q); actionLabel = T(S_FAR_GO); keepLabel = T(S_FAR_STAY);
    actionFillColor = UI_BAR_WARN; actionTextColor = UI_INK; keepFillColor = UI_BAR_OK; keepTextColor = UI_WHITE;
  }
  const int actionButtonTop = CH_Y + CH_B1_REL_Y, keepButtonTop = CH_Y + CH_B2_REL_Y;
  drawBorderedBox(CH_X, CH_Y, CH_W, CH_H, CH_R, UI_WHITE, UI_INK);
  gfx->setTextColor(UI_INK);
  ctext(question, 2, CH_Y + CH_QY, CH_W - CHOICE_QUESTION_SIDE_ROOM);
  gfx->fillRoundRect(CH_BX, actionButtonTop, CH_BW, CH_BH, CHOICE_BUTTON_RADIUS, actionFillColor);  // action button
  gfx->setTextColor(actionTextColor);
  ctext(actionLabel, 2, actionButtonTop + CH_BTY, CH_BW - CHOICE_BUTTON_SIDE_ROOM);
  gfx->fillRoundRect(CH_BX, keepButtonTop, CH_BW, CH_BH, CHOICE_BUTTON_RADIUS, keepFillColor);  // keep/stay together button
  gfx->setTextColor(keepTextColor);
  ctext(keepLabel, 2, keepButtonTop + CH_BTY, CH_BW - CHOICE_BUTTON_SIDE_ROOM);
}
#pragma endregion

#pragma region Pulsing CTA buttons
void drawEvolveButton() {
  static const PulsingCtaButtonStyle evolveButtonStyle = {
    EVO_BTN_X, EVO_BTN_Y, EVO_BTN_W, EVO_BTN_H, EVO_R, 5, 0.006f, UI_BAR_BAD, UI_WHITE, UI_WHITE, true, TS(3), true
  };
  drawPulsingCtaButton(evolveButtonStyle, T(S_EVO_TAP), millis());
}

// "<name> wants to tell you something..." / "<name> feels abandoned..."
static const char *ctaCreatureName() { return pet.nick[0] ? pet.nick : dexName(pet.speciesId); }

void drawFarewellButton() {
  static const PulsingCtaButtonStyle farewellButtonStyle = {
    FAR_BTN_X, FAR_BTN_Y, FAR_BTN_W, FAR_BTN_H, FAR_R, 4, 0.005f, UI_BAR_WARN, UI_INK, UI_INK, false, TS(2), false
  };
  char buf[52];
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), ctaCreatureName());
  drawPulsingCtaButton(farewellButtonStyle, buf, millis());
}

// sad ending: dark blue-gray, slow and muted pulse
void drawRunawayButton() {
  static const PulsingCtaButtonStyle runawayButtonStyle = {
    FAR_BTN_X, FAR_BTN_Y, FAR_BTN_W, FAR_BTN_H, FAR_R, 3, 0.003f,
    RUNAWAY_BUTTON_FILL_COLOR, RUNAWAY_BUTTON_BORDER_COLOR, RUNAWAY_BUTTON_TEXT_COLOR, false, TS(2), false
  };
  char buf[52];
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), ctaCreatureName());
  drawPulsingCtaButton(runawayButtonStyle, buf, millis());
}
#pragma endregion

#pragma region Evolution FX
void drawEvolveFX(uint32_t now) {
  float evolveProgress = pet.evolveT();  // 0..1
  int centerX = CX, centerY = PET_GROUND - SY(96);

  #pragma region Radial halo that grows and pulses
  int haloRadius = SK(36) + (int)(evolveProgress * SK(150)) + (int)(8 * sineOfMillis(now, 0.02f));
  for (int k = 0; k < 4; k++) {
    int radius = haloRadius - k * 7;
    if (radius > 0) gfx->drawCircle(centerX, centerY, radius, UI_WHITE);
  }
  #pragma endregion

  #pragma region Spinning rays from the creature center
  float rayBaseAngle = millisPhaseRadians(now, 0.004f);
  float rayPulsePhase = millisPhaseRadians(now, 0.012f);
  for (int i = 0; i < 12; i++) {
    float rayAngle = rayBaseAngle + i * (float)(PI / 6);
    int rayLength = SK(90) + (int)(SK(70) * (0.5f + 0.5f * sinf(rayPulsePhase + i)));
    gfx->drawLine(centerX, centerY, centerX + (int)(cosf(rayAngle) * rayLength), centerY + (int)(sinf(rayAngle) * rayLength),
                  UI_WHITE);
  }
  #pragma endregion

  #pragma region Accelerating flashing between the PREVIOUS and the NEW form (silhouettes)
  // at the end (progress > 0.9) it stays on the new one for the reveal flash
  int flashPeriodMs = 60 + (int)(220 * (1.0f - evolveProgress));
  bool showOld = evolveProgress < 0.9f && evoPmd.loaded && ((now / flashPeriodMs) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, centerX, PET_GROUND, 0, true, true, CEREMONY_PET_MAX_SCALE);
  else drawPmdAct(PMD_IDLE, centerX, PET_GROUND, 0, true, true, CEREMONY_PET_MAX_SCALE);
  #pragma endregion

  #pragma region Sparks shooting out
  for (int i = 0; i < 10; i++) {
    float sparkAngle = i * (float)(PI / 5) + evolveProgress * 4.0f;
    int sparkDistance = (int)((now / 14 + i * 33) % SK(200));
    int sparkX = centerX + (int)(cosf(sparkAngle) * sparkDistance), sparkY = centerY + (int)(sinf(sparkAngle) * sparkDistance);
    gfx->fillRect(sparkX - 2, sparkY - 2, 5, 5, (i & 1) ? EVOLVE_SPARK_COLOR : UI_WHITE);
  }
  #pragma endregion

  #pragma region Final flash before revealing the new form
  if (evolveProgress > 0.9f) gfx->fillCircle(centerX, centerY, (int)(SK(300) * (evolveProgress - 0.9f) / 0.1f), UI_WHITE);
  #pragma endregion
}
#pragma endregion

// Settings menu (hamburger button) and its sub-screens: audio / video, time,
// language and WiFi (boards with BOARD_HAS_WIFI only). See settings_screens.h.

#pragma region Includes
#include "settings_screens.h"
#include <Preferences.h>
#include <time.h>           // gmtime_r, mktime (date fields of the time screen)
#include "app_state.h"      // settingsScreen, clockH/clockM, settingsBrightnessLevel, pet
#include "audio.h"          // audioEnabled, audioSetEnabled, sfxPlay
#include "board_display.h"  // gfx, clearScreenToDayBackground
#include "boards/layout.h"  // SETTINGS_*, CLK_*, WIFI_*, CLOSE_BTN_*
#include "colors.h"
#include "i18n.h"           // T(), setLang, gLang, LANG_COUNT
#include "nvs_settings.h"   // NVS_NAMESPACE
#include "power_ui.h"       // turnScreenOff
#include "rtcbat.h"         // rtcEpoch, rtcSetEpoch
#include "ui_text.h"
#include "ui_widgets.h"     // drawBorderedBox, drawOnOffPill, drawInsetProgressBar, drawScreenTitle, close button
#if BOARD_HAS_WIFI
#include "keyboard.h"       // openKeyboardForWifiPassword
#include "wifi_manager.h"
#endif
#pragma endregion

#pragma region Forward declarations
#if BOARD_HAS_WIFI
static void openWifiSettings();
#endif
#pragma endregion

#pragma region On-screen date and time setting helpers (settings menu > Time)
// The user sets their LOCAL date and time by eye; the firmware uses it as-is, so
// there is no time zone to manage. One field (year, month, day, hour or minute)
// is selected at a time by tapping it, and the single -/+ pair steps that field.
// Moving the date moves the day the streak counts from, like a real calendar day.

enum ClockField : uint8_t { CLOCK_FIELD_YEAR, CLOCK_FIELD_MONTH, CLOCK_FIELD_DAY, CLOCK_FIELD_HOUR, CLOCK_FIELD_MINUTE,
                            CLOCK_FIELD_COUNT };
// years the editor offers: the RTC chip treats years before 2025 as "no time", and uint32 epochs end in 2106
#define CLOCK_YEAR_MIN 2025
#define CLOCK_YEAR_MAX 2099
// "2026-10-27" and "14:35": characters per line, and the width of one character cell (5x7 font + 1 px gap)
#define CLOCK_DATE_CHAR_COUNT 10
#define CLOCK_TIME_CHAR_COUNT 5
#define CLOCK_CHAR_CELL_W 6

// where each field sits: on the date line or the time line, first character and character count
struct ClockFieldSpan {
  bool onDateLine;
  uint8_t firstChar, charCount;
};
static const ClockFieldSpan CLOCK_FIELD_SPANS[CLOCK_FIELD_COUNT] = {
  { true, 0, 4 }, { true, 5, 2 }, { true, 8, 2 },  // year, month, day
  { false, 0, 2 }, { false, 3, 2 },                // hour, minute
};

// date being edited (the time is clockH / clockM in app_state) and the field the -/+ pair steps
static int clockYear = CLOCK_YEAR_MIN, clockMonth = 1, clockDay = 1;
static ClockField clockSelectedField = CLOCK_FIELD_HOUR;
// first boot: the time screen opens on its own before the starter choice; OK is the
// only way out (no X) and it closes the settings instead of going back to the menu
static bool clockIsFirstRunSetup = false;

// time the editor starts from: the last saved time, else the RTC
static uint32_t clockEditorBaseEpoch() {
  return pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
}

static int daysInClockMonth(int year, int month) {
  static const uint8_t DAYS_PER_MONTH[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  bool leapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return (month == 2 && leapYear) ? 29 : DAYS_PER_MONTH[month - 1];
}

// after a month or year change: Jan 31 -> Feb becomes Feb 28 (or 29)
static void clampClockDayToMonth() {
  int lastDay = daysInClockMonth(clockYear, clockMonth);
  if (clockDay > lastDay) clockDay = lastDay;
}

static void openClock() {
  #pragma region Split the base time into date and time fields
  time_t epochSeconds = clockEditorBaseEpoch();
  struct tm calendarTime;
  gmtime_r(&epochSeconds, &calendarTime);
  clockYear = calendarTime.tm_year + 1900;
  clockMonth = calendarTime.tm_mon + 1;
  clockDay = calendarTime.tm_mday;
  clockH = calendarTime.tm_hour;
  clockM = calendarTime.tm_min;
  #pragma endregion
  #pragma region No clock yet (or out of range): start the date on Jan 1 of the first offered year
  if (clockYear < CLOCK_YEAR_MIN || clockYear > CLOCK_YEAR_MAX) {
    clockYear = CLOCK_YEAR_MIN;
    clockMonth = 1;
    clockDay = 1;
  }
  #pragma endregion
  clockSelectedField = CLOCK_FIELD_HOUR;
  settingsScreen = SETTINGS_TIME;
}

void openClockForFirstRunSetup() {
  openClock();
  clockIsFirstRunSetup = true;
}

// OK: sets the date and time and goes back to the settings menu (first boot: on to the starter choice)
static void applyClock() {
  struct tm calendarTime = {};
  calendarTime.tm_year = clockYear - 1900;
  calendarTime.tm_mon = clockMonth - 1;
  calendarTime.tm_mday = clockDay;
  calendarTime.tm_hour = clockH;
  calendarTime.tm_min = clockM;
  uint32_t newEpoch = (uint32_t)mktime(&calendarTime);  // default TZ = UTC, consistent with gmtime_r (as in rtcbat.cpp)
  rtcSetEpoch(newEpoch);
  pet.setClock(newEpoch);
  saveClockFormatToNvs();  // the 12h/24h choice is persisted when the screen closes
  settingsScreen = clockIsFirstRunSetup ? SETTINGS_CLOSED : SETTINGS_MENU;
  clockIsFirstRunSetup = false;
}

static void drawClockBtn(int buttonLeft, int buttonTop, const char *label) {
  drawBorderedBox(buttonLeft, buttonTop, CLK_BTN, CLK_BTN_H, CLK_BTN_R, UI_WHITE, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(TS(4));
  setCur(buttonLeft + CLK_GLYPH_DX, buttonTop + CLK_GLYPH_DY);
  printT(label);
}
#pragma endregion

#pragma region Settings shared helpers
// SETTINGS_*, CLK_*, CLOSE_BTN_*: boards/layout.h (one table per board, shared by
// drawing and touch). Titles: drawScreenTitle(); close X: drawScreenCloseButton() (ui_widgets).

// Line of text centered on CX in the label-wide box starting at top (section labels)
static void drawSettingsLabel(const char *text, int top) {
  drawTextCenteredInBox(text, TS(3), SETTINGS_TITLE_X, top, SETTINGS_TITLE_MAX_W, FONT_H * TS(3));
}

// Clears the screen and draws the title shared by every settings screen
static void beginSettingsScreen(const char *title) {
  clearScreenToDayBackground();
  gfx->setTextColor(UI_INK);
  drawScreenTitle(title);
}
#pragma endregion

#pragma region Settings menu screen
// Title, four large rows and the firmware version. Row 0 is split in two buttons,
// WiFi (left, green while WiFi is on) and Screen Off (right); on a board without
// WiFi (the CYD) Screen Off takes the whole row. The other rows are Audio / Video,
// Time and Language (row 0 entry in the label table is the Screen Off button label).
#define SETTINGS_MENU_ROW_COUNT 4
static const StrId SETTINGS_MENU_ROW_LABELS[SETTINGS_MENU_ROW_COUNT] = { S_MENU_SCREEN_OFF, S_MENU_AV, S_MENU_TIME, S_MENU_LANG };
// icon slot of a row 0 button: 1.5 x the icon size (the WiFi fan is ~1.4 x as wide as tall)
#define SETTINGS_MENU_ICON_SLOT_W (SETTINGS_MENU_ICON_SIZE * 3 / 2)

void openSettingsMenu() { settingsScreen = SETTINGS_MENU; }

static int settingsMenuRowTop(int rowIndex) {
  return SETTINGS_MENU_ROW_Y + rowIndex * (SETTINGS_MENU_ROW_H + SETTINGS_MENU_ROW_GAP);
}

#pragma region Row 0: WiFi (left half) and Screen Off (right half, or the whole row without WiFi)
static int settingsMenuHalfButtonWidth() { return (SETTINGS_MENU_ROW_W - SETTINGS_MENU_SPLIT_GAP) / 2; }
static int settingsMenuScreenOffButtonWidth() { return BOARD_HAS_WIFI ? settingsMenuHalfButtonWidth() : SETTINGS_MENU_ROW_W; }
static int settingsMenuScreenOffButtonLeft() { return SETTINGS_MENU_ROW_X + SETTINGS_MENU_ROW_W - settingsMenuScreenOffButtonWidth(); }
static int settingsMenuButtonLabelWidth(int buttonWidth) { return buttonWidth - 2 * SETTINGS_MENU_ICON_INSET - SETTINGS_MENU_ICON_SLOT_W; }

// Icon (WiFi fan or screen off) centered in its slot at the left, label centered in the rest of the button
static void drawSettingsMenuIconButton(int buttonLeft, int buttonTop, int buttonWidth, bool showsWifiIcon, bool filled,
                                       const char *label, int labelTextSize) {
  uint16_t fillColor = filled ? UI_BAR_OK : UI_WHITE;
  uint16_t foregroundColor = filled ? UI_BG_DAY : UI_INK;
  drawBorderedBox(buttonLeft, buttonTop, buttonWidth, SETTINGS_MENU_ROW_H, SETTINGS_MENU_ROW_R, fillColor, UI_INK);

  #pragma region Icon
  int iconSlotLeft = buttonLeft + SETTINGS_MENU_ICON_INSET;
  if (showsWifiIcon) {
    drawWifiIcon(gfx, iconSlotLeft + SETTINGS_MENU_ICON_SLOT_W / 2, buttonTop + (SETTINGS_MENU_ROW_H + SETTINGS_MENU_ICON_SIZE) / 2,
                 SETTINGS_MENU_ICON_SIZE, 4, foregroundColor, foregroundColor);
  } else {
    drawScreenOffIcon(gfx, iconSlotLeft + (SETTINGS_MENU_ICON_SLOT_W - SETTINGS_MENU_ICON_SIZE) / 2,
                      buttonTop + (SETTINGS_MENU_ROW_H - SETTINGS_MENU_ICON_SIZE) / 2, SETTINGS_MENU_ICON_SIZE,
                      foregroundColor, fillColor);
  }
  #pragma endregion

  gfx->setTextColor(foregroundColor);
  drawTextCenteredInBox(label, labelTextSize, iconSlotLeft + SETTINGS_MENU_ICON_SLOT_W, buttonTop,
                        settingsMenuButtonLabelWidth(buttonWidth), SETTINGS_MENU_ROW_H);
}

static void drawSettingsMenuFirstRow() {
  int rowTop = settingsMenuRowTop(0);
  const int screenOffLabelMaxWidth = settingsMenuButtonLabelWidth(settingsMenuScreenOffButtonWidth()) - 2 * SETTINGS_TEXT_SIDE_PAD;
  int labelTextSize = shrinkTextSizeToWidth(T(SETTINGS_MENU_ROW_LABELS[0]), TS(3), screenOffLabelMaxWidth);
#if BOARD_HAS_WIFI
  #pragma region WiFi button, sharing one text size with Screen Off (the largest at which both fit)
  const int wifiLabelMaxWidth = settingsMenuButtonLabelWidth(settingsMenuHalfButtonWidth()) - 2 * SETTINGS_TEXT_SIDE_PAD;
  labelTextSize = shrinkTextSizeToWidth(T(S_MENU_WIFI), labelTextSize, wifiLabelMaxWidth);
  drawSettingsMenuIconButton(SETTINGS_MENU_ROW_X, rowTop, settingsMenuHalfButtonWidth(), true, wifiIsEnabled(), T(S_MENU_WIFI),
                             labelTextSize);
  #pragma endregion
#endif
  drawSettingsMenuIconButton(settingsMenuScreenOffButtonLeft(), rowTop, settingsMenuScreenOffButtonWidth(), false, false,
                             T(SETTINGS_MENU_ROW_LABELS[0]), labelTextSize);
}
#pragma endregion

static void renderSettingsMenu() {
  beginSettingsScreen(T(S_MENU_TITLE));

  drawSettingsMenuFirstRow();

  #pragma region Rows 1 to 3: label centered, arrow at the right (equal margin on the left keeps it centered)
  const int labelMargin = SETTINGS_MENU_ARROW_INSET + SETTINGS_MENU_ARROW_SIZE / 2;
  for (int rowIndex = 1; rowIndex < SETTINGS_MENU_ROW_COUNT; rowIndex++) {
    int rowTop = settingsMenuRowTop(rowIndex);
    drawBorderedBox(SETTINGS_MENU_ROW_X, rowTop, SETTINGS_MENU_ROW_W, SETTINGS_MENU_ROW_H, SETTINGS_MENU_ROW_R, UI_WHITE, UI_INK);
    gfx->setTextColor(UI_INK);
    drawTextCenteredInBox(T(SETTINGS_MENU_ROW_LABELS[rowIndex]), TS(3), SETTINGS_MENU_ROW_X + labelMargin, rowTop,
                          SETTINGS_MENU_ROW_W - 2 * labelMargin, SETTINGS_MENU_ROW_H);
    drawPageArrow(gfx, SETTINGS_MENU_ROW_X + SETTINGS_MENU_ROW_W - SETTINGS_MENU_ARROW_INSET, rowTop + SETTINGS_MENU_ROW_H / 2,
                  SETTINGS_MENU_ARROW_SIZE, 1, UI_INK);
  }
  #pragma endregion

  #pragma region Firmware version (discreet, at the very bottom)
  char versionText[20];
  snprintf(versionText, sizeof(versionText), "TamaPoke v%s", FW_VERSION);
  gfx->setTextColor(UI_LABEL);
  drawTextCenteredOnCx(versionText, 1, SETTINGS_MENU_VERSION_Y);
  #pragma endregion

  drawScreenCloseButton();
  gfx->flush();
}

static void handleSettingsMenuTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnScreenCloseButton(touchX, touchY)) {  // back to the main screen
    settingsScreen = SETTINGS_CLOSED;
    return;
  }
  #pragma region Row 0: WiFi screen (left, boards with WiFi only) or Screen Off
#if BOARD_HAS_WIFI
  if (isTouchInsideRect(touchX, touchY, SETTINGS_MENU_ROW_X, settingsMenuRowTop(0), settingsMenuHalfButtonWidth(), SETTINGS_MENU_ROW_H)) {
    openWifiSettings();  // only opens the screen: WiFi is switched there
    return;
  }
#endif
  if (isTouchInsideRect(touchX, touchY, settingsMenuScreenOffButtonLeft(), settingsMenuRowTop(0), settingsMenuScreenOffButtonWidth(),
                        SETTINGS_MENU_ROW_H)) {  // back to the main screen with the panel dark (see turnScreenOff)
    settingsScreen = SETTINGS_CLOSED;
    turnScreenOff();
    return;
  }
  #pragma endregion
  for (int rowIndex = 1; rowIndex < SETTINGS_MENU_ROW_COUNT; rowIndex++) {
    if (!isTouchInsideRect(touchX, touchY, SETTINGS_MENU_ROW_X, settingsMenuRowTop(rowIndex), SETTINGS_MENU_ROW_W, SETTINGS_MENU_ROW_H)) continue;
    if (rowIndex == 1) settingsScreen = SETTINGS_AUDIO_VIDEO;
    else if (rowIndex == 2) openClock();  // loads the current time into the editor
    else settingsScreen = SETTINGS_LANGUAGE;
    return;
  }
}
#pragma endregion

#pragma region Audio / video settings screen
// Sound on/off pill and a brightness bar with -/+ buttons (same squares as the
// time screen). Each step applies on the next loop (updateBrightness); the level
// is written to NVS only when the screen closes, since every NVS write freezes
// both cores for ~1s.
void loadBrightnessLevelFromNvs() {
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, true);  // read-only
  settingsBrightnessLevel = preferences.getUChar("bright", SETTINGS_BRIGHTNESS_DEFAULT);
  preferences.end();
  if (settingsBrightnessLevel > SETTINGS_BRIGHTNESS_MAX) settingsBrightnessLevel = SETTINGS_BRIGHTNESS_DEFAULT;
  settingsBrightnessSavedLevel = settingsBrightnessLevel;
}

static void saveBrightnessLevelToNvs() {
  if (settingsBrightnessLevel == settingsBrightnessSavedLevel) return;
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putUChar("bright", settingsBrightnessLevel);
  preferences.end();
  settingsBrightnessSavedLevel = settingsBrightnessLevel;
}

static void renderAudioVideoSettings() {
  beginSettingsScreen(T(S_MENU_AV));

  #pragma region Sound: label and on/off pill
  drawSettingsLabel(T(S_SOUND), SETTINGS_SOUND_LABEL_Y);
  drawOnOffPill(SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y, SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H, SETTINGS_PILL_R,
                audioEnabled(), TS(3));
  #pragma endregion

  #pragma region Brightness: label, -/+ buttons and the level bar between them
  gfx->setTextColor(UI_INK);
  drawSettingsLabel(T(S_BRIGHTNESS), SETTINGS_BRIGHTNESS_LABEL_Y);
  drawClockBtn(SETTINGS_BRIGHTNESS_MINUS_X, SETTINGS_BRIGHTNESS_ROW_Y, "-");
  drawClockBtn(SETTINGS_BRIGHTNESS_PLUS_X, SETTINGS_BRIGHTNESS_ROW_Y, "+");
  const int barTop = SETTINGS_BRIGHTNESS_ROW_Y + (CLK_BTN_H - SETTINGS_BRIGHTNESS_BAR_H) / 2;
  drawInsetProgressBar(SETTINGS_BRIGHTNESS_BAR_X, barTop, SETTINGS_BRIGHTNESS_BAR_W, SETTINGS_BRIGHTNESS_BAR_H,
                       SETTINGS_BRIGHTNESS_BAR_H / 2, (SETTINGS_BRIGHTNESS_BAR_H - 4) / 2, settingsBrightnessLevel,
                       SETTINGS_BRIGHTNESS_MAX, UI_BAR_OK);
  #pragma endregion

  drawScreenCloseButton();
  gfx->flush();
}

static void handleAudioVideoSettingsTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnScreenCloseButton(touchX, touchY)) {  // back to the menu
    saveBrightnessLevelToNvs();
    settingsScreen = SETTINGS_MENU;
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_SOUND_PILL_X, SETTINGS_SOUND_PILL_Y, SETTINGS_SOUND_PILL_W, SETTINGS_SOUND_PILL_H)) {
    audioSetEnabled(!audioEnabled());
    if (audioEnabled()) sfxPlay(SFX_TAP);  // confirms when turning on
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_BRIGHTNESS_MINUS_X, SETTINGS_BRIGHTNESS_ROW_Y, CLK_BTN, CLK_BTN_H)) {
    if (settingsBrightnessLevel > 0) settingsBrightnessLevel--;
    return;
  }
  if (isTouchInsideRect(touchX, touchY, SETTINGS_BRIGHTNESS_PLUS_X, SETTINGS_BRIGHTNESS_ROW_Y, CLK_BTN, CLK_BTN_H)) {
    if (settingsBrightnessLevel < SETTINGS_BRIGHTNESS_MAX) settingsBrightnessLevel++;
  }
}
#pragma endregion

#pragma region Time settings screen: 12h/24h format
// The choice only changes how this screen shows and steps the hour: clockH is
// always 0..23, so applyClock() is the same in both modes. Like the brightness,
// it is written to NVS only when the screen closes (X or OK), and only if it changed.
static bool clockFormatSavedUses24Hour = true;  // last value in NVS (skips no-op writes)

void loadClockFormatFromNvs() {
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, true);  // read-only
  clockUses24Hour = preferences.getBool("clk24h", true);
  preferences.end();
  clockFormatSavedUses24Hour = clockUses24Hour;
}

void saveClockFormatToNvs() {
  if (clockUses24Hour == clockFormatSavedUses24Hour) return;
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBool("clk24h", clockUses24Hour);
  preferences.end();
  clockFormatSavedUses24Hour = clockUses24Hour;
}

// clockH as a 12-hour dial shows it: 0 -> 12, 13 -> 1
static int clockHourOn12HourDial() {
  return hourOn12HourDial(clockH);
}

// hour -/+ (hourStep -1 or +1). In 12h mode it cycles 12, 1..11 inside the
// current half of the day: AM/PM only changes with its own toggle.
static void stepClockHour(int hourStep) {
  if (clockUses24Hour) {
    clockH = (clockH + 24 + hourStep) % 24;
    return;
  }
  int halfDayOffset = clockH >= 12 ? 12 : 0;
  clockH = (clockH % 12 + 12 + hourStep) % 12 + halfDayOffset;
}

// One button showing the current option; each tap switches to the other one.
// Disabled = grayed out (AM/PM in 24h mode), kept in place so the layout does not jump.
static void drawClockToggleButton(int buttonLeft, const char *currentLabel, bool enabled) {
  drawBorderedBox(buttonLeft, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H, CLK_MODE_R, UI_WHITE, enabled ? UI_INK : UI_TRACK);
  gfx->setTextColor(enabled ? UI_INK : UI_TRACK);
  drawTextCenteredInBox(currentLabel, TS(3), buttonLeft, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H);
}

#pragma endregion

#pragma region Time settings screen: field selection and stepping
// -/+ on the selected field (fieldStep -1 or +1); every field wraps around.
// Month and year changes pull the day back into the new month.
static void stepSelectedClockField(int fieldStep) {
  switch (clockSelectedField) {
    case CLOCK_FIELD_YEAR: {
      int yearRange = CLOCK_YEAR_MAX - CLOCK_YEAR_MIN + 1;
      clockYear = CLOCK_YEAR_MIN + (clockYear - CLOCK_YEAR_MIN + yearRange + fieldStep) % yearRange;
      clampClockDayToMonth();
      break;
    }
    case CLOCK_FIELD_MONTH:
      clockMonth = (clockMonth - 1 + 12 + fieldStep) % 12 + 1;
      clampClockDayToMonth();
      break;
    case CLOCK_FIELD_DAY: {
      int lastDay = daysInClockMonth(clockYear, clockMonth);
      clockDay = (clockDay - 1 + lastDay + fieldStep) % lastDay + 1;
      break;
    }
    case CLOCK_FIELD_HOUR: stepClockHour(fieldStep); break;
    default: clockM = (clockM + 60 + fieldStep) % 60; break;
  }
}

#pragma region Line geometry (shared by drawing and touch)
static int clockLineTextSize(bool dateLine) { return dateLine ? CLK_DATE_SIZE : CLK_DIG_SIZE; }
static int clockLineTop(bool dateLine) { return dateLine ? CLK_DATE_Y : CLK_DIG_Y; }
static int clockLineCharWidth(bool dateLine) { return CLOCK_CHAR_CELL_W * clockLineTextSize(dateLine); }
static int clockLineLeft(bool dateLine) {
  int charCount = dateLine ? CLOCK_DATE_CHAR_COUNT : CLOCK_TIME_CHAR_COUNT;
  return CX - charCount * clockLineCharWidth(dateLine) / 2;
}
#pragma endregion

// Field under a tap, or CLOCK_FIELD_COUNT for none. Each field owns its line's
// height (plus the highlight padding) and reaches sideways to the middle of the
// separators next to it; the outer fields reach the screen edges.
static ClockField clockFieldAtTouch(int16_t touchX, int16_t touchY) {
  for (int field = 0; field < CLOCK_FIELD_COUNT; field++) {
    const ClockFieldSpan &span = CLOCK_FIELD_SPANS[field];
    #pragma region Vertical: the line of the field
    int lineTop = clockLineTop(span.onDateLine) - CLK_FIELD_PAD;
    int lineBottom = clockLineTop(span.onDateLine) + FONT_H * clockLineTextSize(span.onDateLine) + CLK_FIELD_PAD;
    if (touchY < lineTop || touchY >= lineBottom) continue;
    #pragma endregion
    #pragma region Horizontal: between the neighboring separators
    int charCount = span.onDateLine ? CLOCK_DATE_CHAR_COUNT : CLOCK_TIME_CHAR_COUNT;
    int lineLeft = clockLineLeft(span.onDateLine), charWidth = clockLineCharWidth(span.onDateLine);
    int regionLeft = span.firstChar == 0 ? 0 : lineLeft + (span.firstChar - 1) * charWidth + charWidth / 2;
    int lastChar = span.firstChar + span.charCount;
    int regionRight = lastChar == charCount ? LCD_WIDTH : lineLeft + lastChar * charWidth + charWidth / 2;
    if (touchX >= regionLeft && touchX < regionRight) return (ClockField)field;
    #pragma endregion
  }
  return CLOCK_FIELD_COUNT;
}

// One line of digits; the selected field (if it is on this line) is inverted: ink box, white digits
static void drawClockLine(bool dateLine, const char *lineText) {
  int textSize = clockLineTextSize(dateLine), lineTop = clockLineTop(dateLine), lineLeft = clockLineLeft(dateLine);
  gfx->setTextColor(UI_INK);
  setSize(textSize);
  setCur(lineLeft, lineTop);
  printT(lineText);

  #pragma region Selected field highlight
  const ClockFieldSpan &span = CLOCK_FIELD_SPANS[clockSelectedField];
  if (span.onDateLine != dateLine) return;
  int charWidth = clockLineCharWidth(dateLine);
  int fieldLeft = lineLeft + span.firstChar * charWidth;
  int glyphsWidth = span.charCount * charWidth - textSize;  // without the last blank column
  gfx->fillRoundRect(fieldLeft - CLK_FIELD_PAD, lineTop - CLK_FIELD_PAD, glyphsWidth + 2 * CLK_FIELD_PAD,
                     (FONT_H - 1) * textSize + 2 * CLK_FIELD_PAD, CLK_BTN_R / 2, UI_INK);
  char fieldText[5];
  memcpy(fieldText, lineText + span.firstChar, span.charCount);
  fieldText[span.charCount] = '\0';
  gfx->setTextColor(UI_WHITE);
  setCur(fieldLeft, lineTop);
  printT(fieldText);
  #pragma endregion
}
#pragma endregion

#pragma region Time settings screen
static void renderClock() {
  beginSettingsScreen(T(S_MENU_TIME));  // same label as its menu row, like Audio / Video

  #pragma region Date (YYYY-MM-DD, language-neutral) and time lines
  char dateText[16];
  snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d", clockYear, clockMonth, clockDay);
  drawClockLine(true, dateText);
  char timeText[8];
  if (clockUses24Hour) snprintf(timeText, sizeof(timeText), "%02d:%02d", clockH, clockM);
  else snprintf(timeText, sizeof(timeText), "%2d:%02d", clockHourOn12HourDial(), clockM);  // same 5 chars: stays centered
  drawClockLine(false, timeText);
  #pragma endregion

  drawClockBtn(CLK_MINUS_X, CLK_BTN_Y, "-");  // selected field -
  drawClockBtn(CLK_PLUS_X, CLK_BTN_Y, "+");   // selected field +

  #pragma region 12h/24h and AM/PM toggle buttons (language-neutral literals)
  drawClockToggleButton(CLK_FORMAT_X, clockUses24Hour ? "24h" : "12h", true);
  drawClockToggleButton(CLK_AMPM_X, clockH >= 12 ? "PM" : "AM", !clockUses24Hour);
  #pragma endregion

  gfx->fillRoundRect(CLK_OK_X, CLK_OK_Y, CLK_OK_W, CLK_OK_H, CLK_OK_R, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(TS(3));
  setCur(CX - CLK_OK_TX, CLK_OK_TY);
  printT("OK");

  if (!clockIsFirstRunSetup) drawScreenCloseButton();  // first boot: OK is the only way out
  gfx->flush();
}

// The +/- row and the OK button keep their original inclusive bottom / right
// edges (<=), so they are not converted to isTouchInsideRect.
static void clockTap(int16_t touchX, int16_t touchY) {
  if (!clockIsFirstRunSetup && isTouchOnScreenCloseButton(touchX, touchY)) {  // back to the menu WITHOUT applying the date and time
    saveClockFormatToNvs();  // the 12h/24h choice is kept either way
    settingsScreen = SETTINGS_MENU;
    return;
  }
  #pragma region Tapping a field of the date or time selects it
  ClockField tappedField = clockFieldAtTouch(touchX, touchY);
  if (tappedField != CLOCK_FIELD_COUNT) {
    clockSelectedField = tappedField;
    return;
  }
  #pragma endregion
  if (touchY >= CLK_BTN_Y && touchY <= CLK_BTN_Y + CLK_BTN_H) {  // -/+ pair: steps the selected field
    if (touchX >= CLK_MINUS_X && touchX < CLK_MINUS_X + CLK_BTN) stepSelectedClockField(-1);
    else if (touchX >= CLK_PLUS_X && touchX < CLK_PLUS_X + CLK_BTN) stepSelectedClockField(1);
    return;
  }
  #pragma region 12h/24h toggle (flips, persisted on close) and AM/PM toggle (12h only)
  if (isTouchInsideRect(touchX, touchY, CLK_FORMAT_X, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H)) {
    clockUses24Hour = !clockUses24Hour;
    return;
  }
  if (!clockUses24Hour && isTouchInsideRect(touchX, touchY, CLK_AMPM_X, CLK_MODE_Y, CLK_MODE_W, CLK_MODE_H)) {
    clockH = (clockH + 12) % 24;  // same hour on the dial, other half of the day
    return;
  }
  #pragma endregion
  if (touchY >= CLK_OK_Y && touchY <= CLK_OK_Y + CLK_OK_H && touchX >= CLK_OK_X && touchX <= CLK_OK_X + CLK_OK_W) {
    applyClock();
    return;
  }
}
#pragma endregion

#pragma region Language settings screen
// One tappable cell per language, 2 columns (row-major, in Lang order). Names
// are ASCII-only so they render under the CP437 font AND the CJK U8g2 fonts.
static const char *const LANGUAGE_NATIVE_NAMES[LANG_COUNT] = {
  "Espanol", "English", "Francais", "Deutsch", "Italiano", "Portugues", "Nihongo", "Hangugeo"
};

static int languageCellLeft(int languageIndex) { return SETTINGS_LANG_CELL_X0 + (languageIndex % 2) * SETTINGS_LANG_CELL_COL_STEP; }
static int languageCellTop(int languageIndex) { return SETTINGS_LANG_CELL_Y0 + (languageIndex / 2) * SETTINGS_LANG_CELL_ROW_STEP; }

static void renderLanguageSettings() {
  beginSettingsScreen(T(S_MENU_LANG));

  #pragma region One text size for every cell: the largest at which all names fit
  int sharedTextSize = TS(3);
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    sharedTextSize = shrinkTextSizeToWidth(LANGUAGE_NATIVE_NAMES[languageIndex], sharedTextSize,
                                           SETTINGS_LANG_CELL_W - 2 * SETTINGS_TEXT_SIDE_PAD);
  }
  #pragma endregion

  #pragma region Cells (the active language is filled)
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    bool isActiveLanguage = (languageIndex == gLang);
    int cellLeft = languageCellLeft(languageIndex), cellTop = languageCellTop(languageIndex);
    drawBorderedBox(cellLeft, cellTop, SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H, SETTINGS_LANG_CELL_R,
                    isActiveLanguage ? UI_BAR_OK : UI_WHITE, UI_INK);
    gfx->setTextColor(isActiveLanguage ? UI_BG_DAY : UI_INK);
    drawTextCenteredInBox(LANGUAGE_NATIVE_NAMES[languageIndex], sharedTextSize, cellLeft, cellTop,
                          SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H);
  }
  #pragma endregion

  drawScreenCloseButton();
  gfx->flush();
}

static void handleLanguageSettingsTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnScreenCloseButton(touchX, touchY)) {  // back to the menu
    settingsScreen = SETTINGS_MENU;
    return;
  }
  for (int languageIndex = 0; languageIndex < LANG_COUNT; languageIndex++) {
    if (!isTouchInsideRect(touchX, touchY, languageCellLeft(languageIndex), languageCellTop(languageIndex),
                           SETTINGS_LANG_CELL_W, SETTINGS_LANG_CELL_H)) continue;
    setLang((Lang)languageIndex);
    applyLangFont();  // the font changes with the language
    markScreenDirty();  // every static screen shows other text now
    sfxPlay(SFX_TAP);
    settingsScreen = SETTINGS_MENU;
    return;
  }
}
#pragma endregion

#if BOARD_HAS_WIFI
#pragma region WiFi settings screen
// On/Off pill (green while on), Scan button (grayed while it cannot scan), one
// status line and the scan results, strongest first, WIFI_ROWS_PER_PAGE per page
// with up/down arrows. A row shows the signal, SSID, band, security and dBm; the
// connected network is filled green with a check mark. Tapping a network asks
// for its password on the keyboard (open networks connect right away).

static void openWifiSettings() {
  wifiListPage = 0;
  settingsScreen = SETTINGS_WIFI;
}

static int wifiListPageCount() {
  return pageCountFor(wifiScanResultCount(), WIFI_ROWS_PER_PAGE);
}

static int wifiListRowTop(int rowIndex) { return WIFI_LIST_Y + rowIndex * WIFI_ROW_STEP; }

static bool isWifiScanButtonEnabled() {
  return wifiIsEnabled() && !wifiIsScanning() && wifiConnectionState() != WIFI_CONNECTION_CONNECTING;
}

// Prints an SSID at (left, top), cut with two dots so it fits maxWidth at textSize
static void drawSsidCutToWidth(const char *ssid, int textSize, int left, int top, int maxWidth) {
  char fittedText[WIFI_SSID_MAX_LENGTH + 3];  // the SSID plus ".."
  size_t fittedLength = strnlen(ssid, WIFI_SSID_MAX_LENGTH);
  memcpy(fittedText, ssid, fittedLength);
  fittedText[fittedLength] = 0;
  setSize(textSize);
  if (textW(fittedText, textSize) > maxWidth) {
    do {
      fittedLength--;
      memcpy(fittedText + fittedLength, "..", 3);
    } while (fittedLength > 0 && textW(fittedText, textSize) > maxWidth);
  }
  setCur(left, top);
  printT(fittedText);
}

static void drawWifiNetworkRow(const WifiScanResult &network, int rowTop) {
  bool connected = wifiIsConnectedToSsid(network.ssid);
  uint16_t rowInk = connected ? UI_BG_DAY : UI_INK;
  drawBorderedBox(WIFI_LIST_X, rowTop, WIFI_LIST_W, WIFI_ROW_H, WIFI_ROW_R, connected ? UI_BAR_OK : UI_WHITE, UI_INK);

  #pragma region Signal strength: WiFi fan lit by the RSSI, vertically centered
  const int iconWidth = WIFI_ROW_ICON_SIZE * 3 / 2;
  drawWifiIcon(gfx, WIFI_LIST_X + WIFI_ROW_PAD + iconWidth / 2, rowTop + (WIFI_ROW_H + WIFI_ROW_ICON_SIZE) / 2,
               WIFI_ROW_ICON_SIZE, wifiSignalBarsForRssi(network.rssi), rowInk, UI_TRACK);
  #pragma endregion

  #pragma region Check mark at the right edge (connected network only)
  int textRight = WIFI_LIST_X + WIFI_LIST_W - WIFI_ROW_PAD;
  if (connected) {
    int checkLeft = textRight - WIFI_ROW_CHECK_SIZE;
    drawCheckIcon(gfx, checkLeft, rowTop + (WIFI_ROW_H - WIFI_ROW_CHECK_SIZE) / 2, WIFI_ROW_CHECK_SIZE, rowInk);
    textRight = checkLeft - WIFI_ROW_PAD;
  }
  #pragma endregion

  #pragma region SSID, then band, security and signal in dBm
  const int textLeft = WIFI_LIST_X + 2 * WIFI_ROW_PAD + iconWidth;
  const int textWidth = textRight - textLeft;
  gfx->setTextColor(rowInk);
  drawSsidCutToWidth(network.ssid, WIFI_ROW_SSID_SIZE, textLeft, rowTop + WIFI_ROW_SSID_DY, textWidth);
  const char *securityText = wifiSecurityLabel(network.authMode);
  if (securityText[0] == 0) securityText = T(S_WIFI_OPEN);
  char detailText[48];
  snprintf(detailText, sizeof(detailText), "%s  %s  %ld dBm", wifiIsChannelOn5GHz(network.channel) ? "5 GHz" : "2.4 GHz",
           securityText, (long)network.rssi);
  shrinkTextSizeToWidth(detailText, WIFI_ROW_DETAIL_SIZE, textWidth);  // leaves the fitted size applied
  gfx->setTextColor(connected ? UI_BG_DAY : UI_LABEL);
  setCur(textLeft, rowTop + WIFI_ROW_DETAIL_DY);
  printT(detailText);
  #pragma endregion
}

static void renderWifiSettings() {
  beginSettingsScreen(T(S_MENU_WIFI));
  bool wifiOn = wifiIsEnabled();

  #pragma region On/Off pill (green when on) and the Scan button (grayed when it cannot scan)
  drawOnOffPill(WIFI_TOGGLE_X, WIFI_BUTTON_Y, WIFI_TOGGLE_W, WIFI_BUTTON_H, WIFI_BUTTON_R, wifiOn, TS(3));

  bool scanEnabled = isWifiScanButtonEnabled();
  drawBorderedBox(WIFI_SCAN_X, WIFI_BUTTON_Y, WIFI_SCAN_W, WIFI_BUTTON_H, WIFI_BUTTON_R, UI_WHITE, scanEnabled ? UI_INK : UI_TRACK);
  gfx->setTextColor(scanEnabled ? UI_INK : UI_TRACK);
  drawTextCenteredInBox(T(S_WIFI_SCAN), TS(3), WIFI_SCAN_X, WIFI_BUTTON_Y, WIFI_SCAN_W, WIFI_BUTTON_H);
  #pragma endregion

  #pragma region Status line: connecting / failed first, then scanning, the connected network, nothing found
  char statusText[64] = "";
  uint16_t statusColor = UI_LABEL;
  WifiConnectionState connectionState = wifiConnectionState();
  if (wifiOn) {
    if (connectionState == WIFI_CONNECTION_CONNECTING) {
      snprintf(statusText, sizeof(statusText), "%s", T(S_WIFI_CONNECTING));
    } else if (connectionState == WIFI_CONNECTION_FAILED) {
      snprintf(statusText, sizeof(statusText), "%s", T(S_WIFI_FAILED));
      statusColor = UI_BAR_BAD;
    } else if (wifiIsScanning()) {
      snprintf(statusText, sizeof(statusText), "%s", T(S_WIFI_SCANNING));
    } else if (connectionState == WIFI_CONNECTION_CONNECTED) {
      snprintf(statusText, sizeof(statusText), "%s: %s", T(S_WIFI_CONNECTED), wifiConnectedSsid());
    } else if (wifiHasScanned() && wifiScanResultCount() == 0) {
      snprintf(statusText, sizeof(statusText), "%s", T(S_WIFI_NO_NETWORKS));
    }
  }
  if (statusText[0]) {
    gfx->setTextColor(statusColor);
    int statusTextSize = shrinkTextSizeToWidth(statusText, WIFI_STATUS_SIZE, WIFI_STATUS_MAX_W);
    setCur(centerX(statusText, statusTextSize), WIFI_STATUS_Y);
    printT(statusText);
  }
  #pragma endregion

  #pragma region Network rows of the current page
  int pageCount = wifiListPageCount();
  if (wifiListPage >= pageCount) wifiListPage = pageCount - 1;
  for (int rowIndex = 0; rowIndex < WIFI_ROWS_PER_PAGE; rowIndex++) {
    int resultIndex = wifiListPage * WIFI_ROWS_PER_PAGE + rowIndex;
    if (resultIndex >= wifiScanResultCount()) break;
    drawWifiNetworkRow(wifiScanResultAt(resultIndex), wifiListRowTop(rowIndex));
  }
  #pragma endregion

  #pragma region Page arrows and page number (only with more than one page; a hidden arrow ignores taps)
  if (pageCount > 1) {
    if (wifiListPage > 0) drawVerticalPageArrow(gfx, WIFI_ARROW_CX, WIFI_ARROW_UP_CY, WIFI_ARROW_SIZE, -1, UI_INK);
    if (wifiListPage < pageCount - 1) drawVerticalPageArrow(gfx, WIFI_ARROW_CX, WIFI_ARROW_DOWN_CY, WIFI_ARROW_SIZE, 1, UI_INK);
    char pageText[8];
    snprintf(pageText, sizeof(pageText), "%d/%d", wifiListPage + 1, pageCount);
    gfx->setTextColor(UI_LABEL);
    setSize(WIFI_PAGE_TEXT_SIZE);
    setCur(WIFI_ARROW_CX - (int)textW(pageText, WIFI_PAGE_TEXT_SIZE) / 2, WIFI_PAGE_TEXT_Y);
    printT(pageText);
  }
  #pragma endregion

  drawScreenCloseButton();
  gfx->flush();
}

static void handleWifiSettingsTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnScreenCloseButton(touchX, touchY)) {  // back to the menu
    settingsScreen = SETTINGS_MENU;
    return;
  }

  #pragma region On/Off pill (persists) and Scan
  if (isTouchInsideRect(touchX, touchY, WIFI_TOGGLE_X, WIFI_BUTTON_Y, WIFI_TOGGLE_W, WIFI_BUTTON_H)) {
    bool turningOn = !wifiIsEnabled();
    wifiSetEnabled(turningOn);
    wifiListPage = 0;
    sfxPlay(turningOn && !wifiIsEnabled() ? SFX_DENY : SFX_TAP);  // the WiFi stack could not start
    return;
  }
  if (isTouchInsideRect(touchX, touchY, WIFI_SCAN_X, WIFI_BUTTON_Y, WIFI_SCAN_W, WIFI_BUTTON_H)) {
    if (isWifiScanButtonEnabled() && wifiStartScan()) {
      wifiListPage = 0;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  #pragma endregion

  #pragma region Page arrows
  int pageCount = wifiListPageCount();
  const int arrowHitLeft = WIFI_ARROW_CX - WIFI_ARROW_HIT_W / 2;
  if (wifiListPage > 0 &&
      isTouchInsideRect(touchX, touchY, arrowHitLeft, WIFI_ARROW_UP_CY - WIFI_ARROW_HIT_H / 2, WIFI_ARROW_HIT_W, WIFI_ARROW_HIT_H)) {
    wifiListPage--;
    return;
  }
  if (wifiListPage < pageCount - 1 &&
      isTouchInsideRect(touchX, touchY, arrowHitLeft, WIFI_ARROW_DOWN_CY - WIFI_ARROW_HIT_H / 2, WIFI_ARROW_HIT_W, WIFI_ARROW_HIT_H)) {
    wifiListPage++;
    return;
  }
  #pragma endregion

  #pragma region Network rows: password prompt, or connect right away to an open network
  if (wifiConnectionState() == WIFI_CONNECTION_CONNECTING) return;  // one attempt at a time
  for (int rowIndex = 0; rowIndex < WIFI_ROWS_PER_PAGE; rowIndex++) {
    int resultIndex = wifiListPage * WIFI_ROWS_PER_PAGE + rowIndex;
    if (resultIndex >= wifiScanResultCount()) break;
    if (!isTouchInsideRect(touchX, touchY, WIFI_LIST_X, wifiListRowTop(rowIndex), WIFI_LIST_W, WIFI_ROW_H)) continue;
    const WifiScanResult &network = wifiScanResultAt(resultIndex);
    if (wifiIsConnectedToSsid(network.ssid)) return;  // already connected
    WifiSecurityKind securityKind = wifiSecurityKindOf(network.authMode);
    if (securityKind == WIFI_SECURITY_ENTERPRISE) {  // needs a user name too: not supported
      sfxPlay(SFX_DENY);
      return;
    }
    sfxPlay(SFX_TAP);
    if (securityKind == WIFI_SECURITY_OPEN) wifiConnectToNetwork(network.ssid, "");
    else openKeyboardForWifiPassword(network.ssid);
    return;
  }
  #pragma endregion
}
#pragma endregion
#endif  // BOARD_HAS_WIFI

#pragma region Settings dispatch (render and tap for whichever settings screen is open)
void renderSettingsScreen() {
  switch (settingsScreen) {
    case SETTINGS_AUDIO_VIDEO: renderAudioVideoSettings(); break;
    case SETTINGS_TIME: renderClock(); break;
    case SETTINGS_LANGUAGE: renderLanguageSettings(); break;
#if BOARD_HAS_WIFI
    case SETTINGS_WIFI: renderWifiSettings(); break;
#endif
    default: renderSettingsMenu(); break;
  }
}

void handleSettingsTap(int16_t touchX, int16_t touchY) {
  switch (settingsScreen) {
    case SETTINGS_AUDIO_VIDEO: handleAudioVideoSettingsTap(touchX, touchY); break;
    case SETTINGS_TIME: clockTap(touchX, touchY); break;
    case SETTINGS_LANGUAGE: handleLanguageSettingsTap(touchX, touchY); break;
#if BOARD_HAS_WIFI
    case SETTINGS_WIFI: handleWifiSettingsTap(touchX, touchY); break;
#endif
    default: handleSettingsMenuTap(touchX, touchY); break;
  }
}
#pragma endregion

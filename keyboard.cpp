// On-screen keyboard: renames the creature, or asks for a WiFi password
// (BOARD_HAS_WIFI boards only). See keyboard.h.

#pragma region Includes
#include "keyboard.h"
#include <string.h>
#include "app_state.h"      // keyboard state, kbOpen, pet, wifiPasswordTargetSsid
#include "audio.h"          // sfxPlay
#include "board_display.h"  // gfx, clearScreenToDayBackground
#include "boards/layout.h"  // KB_*
#include "colors.h"
#include "i18n.h"           // T()
#include "ui_text.h"
#include "ui_widgets.h"     // drawBorderedBox, drawScreenCloseButton, isTouchOnScreenCloseButton
#if BOARD_HAS_WIFI
#include "wifi_manager.h"   // wifiConnectToNetwork
#endif
#pragma endregion

#pragma region Keyboard layout
// 7 x 5 keys. The bottom row is the same on every page: Shift, 123/ABC, a
// 3-key-wide space bar, delete and OK. Shift switches the letters case, or to
// the second symbols page while the 123 page is up (the two symbol pages hold
// every printable ASCII symbol). Control codes mark the special keys.
// KB_X/Y/W/H and the rest: boards/layout.h
#define KB_COLS 7
#define KB_ROWS 5
#define KB_KEY_COUNT (KB_COLS * KB_ROWS)
#define KB_KEY_GAP 6     // px between neighboring keys (subtracted from the key pitch)
#define KB_KEY_RADIUS 6
#define KB_KEY_TEXT_SIZE 2
#define KB_KEY_SHIFT '\x01'
#define KB_KEY_MODE '\x02'
#define KB_KEY_DELETE '\x03'
#define KB_KEY_OK '\x04'
#define KB_BOTTOM_ROW "\x01\x02   \x03\x04"
static const char KB_LETTER_KEYS[] = "ABCDEFG" "HIJKLMN" "OPQRSTU" "VWXYZ.-" KB_BOTTOM_ROW;
static const char KB_SYMBOL_KEYS[] = "1234567" "890@#$%" "&*()-_+" "=!?.,:;" KB_BOTTOM_ROW;
static const char KB_MORE_SYMBOL_KEYS[] = "1234567" "890\"'/\\" "|~`<>[]" "{}^-_.," KB_BOTTOM_ROW;
static_assert(sizeof(KB_LETTER_KEYS) == KB_KEY_COUNT + 1, "letters page must fill the 7 x 5 grid");
static_assert(sizeof(KB_SYMBOL_KEYS) == KB_KEY_COUNT + 1, "symbols page must fill the 7 x 5 grid");
static_assert(sizeof(KB_MORE_SYMBOL_KEYS) == KB_KEY_COUNT + 1, "second symbols page must fill the 7 x 5 grid");

// key at keyIndex (row-major) on the page and case currently shown
static char keyboardKeyAt(int keyIndex) {
  if (keyboardSymbolsOn) return (keyboardShiftOn ? KB_MORE_SYMBOL_KEYS : KB_SYMBOL_KEYS)[keyIndex];
  char letterKey = KB_LETTER_KEYS[keyIndex];
  if (!keyboardShiftOn && letterKey >= 'A' && letterKey <= 'Z') return (char)(letterKey - 'A' + 'a');
  return letterKey;
}
#pragma endregion

#pragma region Keyboard open / close
// Names start in upper case (as before), passwords in lower case
static void openKeyboardWithText(KeyboardPurpose purpose, const char *initialText, uint8_t maxLength) {
  keyboardPurpose = purpose;
  keyboardTextMaxLength = maxLength;
  strncpy(keyboardText, initialText, maxLength);
  keyboardText[maxLength] = 0;
  keyboardTextLength = strlen(keyboardText);
  keyboardSymbolsOn = false;
  keyboardShiftOn = (purpose == KEYBOARD_FOR_NAME);
  kbOpen = true;
}

void openKeyboard() { openKeyboardWithText(KEYBOARD_FOR_NAME, pet.nick, sizeof(pet.nick) - 1); }

#if BOARD_HAS_WIFI
void openKeyboardForWifiPassword(const char *ssid) {
  strncpy(wifiPasswordTargetSsid, ssid, WIFI_SSID_MAX_LENGTH);
  wifiPasswordTargetSsid[WIFI_SSID_MAX_LENGTH] = 0;
  openKeyboardWithText(KEYBOARD_FOR_WIFI_PASSWORD, "", WIFI_PASSWORD_MAX_LENGTH);
}
#endif

// The text is wiped so a typed password does not linger in RAM
static void closeKeyboard() {
  memset(keyboardText, 0, sizeof(keyboardText));
  keyboardTextLength = 0;
  kbOpen = false;
}

// OK: renames the creature, or starts connecting with the password (the WiFi
// screen underneath shows the result)
static void submitKeyboardText() {
#if BOARD_HAS_WIFI
  if (keyboardPurpose == KEYBOARD_FOR_WIFI_PASSWORD) {
    if (keyboardTextLength == 0) {
      sfxPlay(SFX_DENY);
      return;
    }
    wifiConnectToNetwork(wifiPasswordTargetSsid, keyboardText);
    closeKeyboard();
    return;
  }
#endif
  pet.rename(keyboardText);
  closeKeyboard();
}
#pragma endregion

#pragma region Keyboard drawing
// keyWidth spans the key columns (the space bar is three), minus KB_KEY_GAP
static void drawKeyboardKey(char key, int keyLeft, int keyTop, int keyWidth) {
  const int keyHeight = KB_H - KB_KEY_GAP;
  bool isSpecialKey = key == KB_KEY_SHIFT || key == KB_KEY_MODE || key == KB_KEY_DELETE || key == KB_KEY_OK;
  bool isActiveShift = key == KB_KEY_SHIFT && keyboardShiftOn;
  drawBorderedBox(keyLeft, keyTop, keyWidth, keyHeight, KB_KEY_RADIUS,
                  isActiveShift ? UI_BAR_OK : isSpecialKey ? UI_BAR_WARN : UI_WHITE, UI_INK);

  #pragma region Space bar: an open bracket under the middle of the key
  if (key == ' ') {
    int bracketWidth = keyWidth / 2, bracketLeft = keyLeft + (keyWidth - bracketWidth) / 2;
    int bracketBottom = keyTop + keyHeight * 2 / 3, bracketHeight = keyHeight / 6;
    gfx->drawFastHLine(bracketLeft, bracketBottom, bracketWidth, UI_INK);
    gfx->drawFastVLine(bracketLeft, bracketBottom - bracketHeight, bracketHeight, UI_INK);
    gfx->drawFastVLine(bracketLeft + bracketWidth - 1, bracketBottom - bracketHeight, bracketHeight, UI_INK);
    return;
  }
  #pragma endregion

  #pragma region Label: the character, or the name of the special key
  char characterLabel[2] = { key, 0 };
  const char *label = characterLabel;
  if (key == KB_KEY_SHIFT) label = keyboardSymbolsOn ? (keyboardShiftOn ? "2/2" : "1/2") : "aA";
  else if (key == KB_KEY_MODE) label = keyboardSymbolsOn ? "ABC" : "123";
  else if (key == KB_KEY_DELETE) label = "<-";
  else if (key == KB_KEY_OK) label = "OK";
  gfx->setTextColor(UI_INK);
  drawTextCenteredInBox(label, KB_KEY_TEXT_SIZE, keyLeft, keyTop, keyWidth, keyHeight);
  #pragma endregion
}

void renderKeyboard() {
  clearScreenToDayBackground();
  gfx->setTextColor(UI_INK);
  ctext(keyboardPurpose == KEYBOARD_FOR_WIFI_PASSWORD ? T(S_PASSWORD) : T(S_NAME), 2, KB_TITLE_Y);

  #pragma region Text box: the nickname, or one star per password character (the end stays visible when it overflows)
  drawBorderedBox(KB_BOX_X, KB_BOX_Y, KB_BOX_W, KB_BOX_H, KB_BOX_R, UI_WHITE, UI_INK);
  char shownText[sizeof(keyboardText)];
  if (keyboardPurpose == KEYBOARD_FOR_WIFI_PASSWORD) {
    memset(shownText, '*', keyboardTextLength);
    shownText[keyboardTextLength] = 0;
  } else {
    memcpy(shownText, keyboardText, keyboardTextLength + 1);
  }
  const char *visibleText = keyboardTextLength ? shownText : "_";
  setSize(TS(3));
  const int maxTextWidth = KB_BOX_W - 2 * (KB_BOX_TX - KB_BOX_X);
  while (visibleText[1] && (int)textW(visibleText, TS(3)) > maxTextWidth) visibleText++;
  setCur(KB_BOX_TX, KB_BOX_TY);
  printT(visibleText);
  #pragma endregion

  #pragma region Keys (consecutive spaces in a row are one wide space bar)
  for (int keyIndex = 0; keyIndex < KB_KEY_COUNT; keyIndex++) {
    int column = keyIndex % KB_COLS;
    char key = keyboardKeyAt(keyIndex);
    if (key == ' ' && column > 0 && keyboardKeyAt(keyIndex - 1) == ' ') continue;  // part of the bar on its left
    int spanColumns = 1;
    while (key == ' ' && column + spanColumns < KB_COLS && keyboardKeyAt(keyIndex + spanColumns) == ' ') spanColumns++;
    drawKeyboardKey(key, KB_X + column * KB_W, KB_Y + (keyIndex / KB_COLS) * KB_H, spanColumns * KB_W - KB_KEY_GAP);
  }
  #pragma endregion

  drawScreenCloseButton();  // closes without saving
  gfx->flush();
}
#pragma endregion

#pragma region Keyboard touch
void keyboardTap(int16_t touchX, int16_t touchY) {
  if (isTouchOnScreenCloseButton(touchX, touchY)) {  // cancel: nothing is renamed or connected
    closeKeyboard();
    return;
  }
  // filter BEFORE dividing: with x/y below KB_X/KB_Y the subtraction is negative
  // and integer division truncates toward zero (not toward -inf), so column/row
  // would come out 0 instead of negative and the filter below would not catch it
  if (touchX < KB_X || touchY < KB_Y) return;
  int column = (touchX - KB_X) / KB_W, row = (touchY - KB_Y) / KB_H;
  if (column >= KB_COLS || row >= KB_ROWS) return;
  char key = keyboardKeyAt(row * KB_COLS + column);
  if (key == KB_KEY_SHIFT) {
    keyboardShiftOn = !keyboardShiftOn;
  } else if (key == KB_KEY_MODE) {  // the 123 page opens on its first page; back to letters in the purpose case
    keyboardSymbolsOn = !keyboardSymbolsOn;
    keyboardShiftOn = !keyboardSymbolsOn && keyboardPurpose == KEYBOARD_FOR_NAME;
  } else if (key == KB_KEY_DELETE) {
    if (keyboardTextLength) keyboardText[--keyboardTextLength] = 0;
  } else if (key == KB_KEY_OK) {
    submitKeyboardText();
  } else if (keyboardTextLength < keyboardTextMaxLength) {
    keyboardText[keyboardTextLength++] = key;
    keyboardText[keyboardTextLength] = 0;
  }
}
#pragma endregion

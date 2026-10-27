#pragma region Includes
#include "rtcbat.h"
#include "boards/board_select.h"
#pragma endregion

// Fallbacks for boards WITHOUT a PCF85063 (RTC), an AXP2101 (PMU) or a battery,
// such as the CYD (ESP32-2432S028R): USB-only board, no backup cell for the
// clock. Each part is compiled from the capability flags, so it only fills in
// what rtcbat.cpp does not provide on the current board.
//
// TODO(addon): for a real clock, add a DS3231/PCF8563 over I2C with a CR2032
// cell and port this file following rtcbat.cpp (Waveshare). For a real
// battery, an 18650 + TP4056 + divider into an ADC would do for a rough voltage.

#if !BOARD_HAS_RTC_CHIP
#pragma region Soft clock: NVS-backed epoch advanced with millis()
// This is a real hardware limitation:
//   - the epoch is saved to NVS on every rtcSetEpoch() and advances with
//     millis() while the board is powered. It survives a reboot (it is reread
//     in rtcBegin()), but time with the board OFF is not counted: without a
//     crystal/cell, each boot resumes from the last saved value.
//   - effect: offline progress (pet.syncClock) does not count the time it was
//     unplugged. So that the session time survives a reboot, rtcPersistIfDue()
//     rewrites the epoch every RTC_PERSIST_INTERVAL_MS (minimal flash wear);
//     the caller runs it from the idle path, rtcEpoch() itself never writes.

#pragma region Includes (NVS)
#include <Preferences.h>
#pragma endregion

#pragma region Soft clock configuration and state
#define RTC_NVS_NAMESPACE "tamapoke_rtc"
#define RTC_NVS_EPOCH_KEY "epoch"
#define RTC_PERSIST_INTERVAL_MS 600000UL  // 10 min: a reboot goes back at most this much

static Preferences rtcPreferences;
static uint32_t baseEpoch = 0;     // epoch at the instant baseMillis; 0 = unknown
static uint32_t baseMillis = 0;
static uint32_t lastPersistMillis = 0;
#pragma endregion

bool rtcBegin() {
  rtcPreferences.begin(RTC_NVS_NAMESPACE, false);
  baseEpoch = rtcPreferences.getUInt(RTC_NVS_EPOCH_KEY, 0);
  baseMillis = millis();
  lastPersistMillis = baseMillis;
  if (baseEpoch == 0) Serial.println("CYD: no saved time (will be seeded in setup)");
  return baseEpoch != 0;
}

uint32_t rtcEpoch() {
  if (baseEpoch == 0) return 0;
  return baseEpoch + (millis() - baseMillis) / 1000;
}

void rtcSetEpoch(uint32_t e) {
  baseEpoch = e;
  baseMillis = millis();
  lastPersistMillis = baseMillis;
  rtcPreferences.putUInt(RTC_NVS_EPOCH_KEY, e);
}

void rtcPersistIfDue() {
  if (baseEpoch == 0) return;
  uint32_t nowMs = millis();
  if (nowMs - lastPersistMillis <= RTC_PERSIST_INTERVAL_MS) return;
  lastPersistMillis = nowMs;
  rtcPreferences.putUInt(RTC_NVS_EPOCH_KEY, rtcEpoch());
}
#pragma endregion
#endif  // !BOARD_HAS_RTC_CHIP

#if !BOARD_HAS_PMU
#pragma region Battery and panel power stubs (no PMU)
// There is no battery: the stubs report "no battery, USB connected".
bool batBegin() { return false; }
void pmuEnablePanel() {}          // no rail to switch on: the panel hangs off 3V3
int batPercent() { return -1; }   // -1 = "no battery connected" (existing contract)
bool batCharging() { return false; }
int batMillivolts() { return 0; }
bool usbPresent() { return true; }  // this board only runs plugged into USB
#pragma endregion
#endif  // !BOARD_HAS_PMU

#if BOARD_HAS_BOOT_POWER_KEY
#pragma region BOOT button (GPIO0) as the screen on/off key
// No PMU power key on this board: the BOOT button stands in for it. GPIO0 is a
// strapping pin, but after boot it is a normal input (active low, with the
// board pull-up). The loop polls only every 250 ms, so a falling-edge
// interrupt latches the press instead of hoping a short tap is still held.
#define BOOT_BUTTON_DEBOUNCE_MS 200
static volatile bool bootButtonPressLatched = false;
static volatile uint32_t bootButtonLastPressMs = 0;

static void IRAM_ATTR onBootButtonFalling() {
  uint32_t nowMs = millis();
  if (nowMs - bootButtonLastPressMs < BOOT_BUTTON_DEBOUNCE_MS) return;  // contact bounce
  bootButtonLastPressMs = nowMs;
  bootButtonPressLatched = true;
}

void pwrSetup() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON_PIN), onBootButtonFalling, FALLING);
}

bool pwrShortPressed() {
  if (!bootButtonPressLatched) return false;
  bootButtonPressLatched = false;
  return true;
}
#pragma endregion
#elif !BOARD_HAS_PMU
#pragma region Power key stubs (neither a PMU nor a BOOT power key)
void pwrSetup() {}
bool pwrShortPressed() { return false; }
#pragma endregion
#endif  // BOARD_HAS_BOOT_POWER_KEY

#include "rtcbat.h"
#include "boards/board_select.h"

#if BOARD_CYD_RECT

#include <Preferences.h>

// The CYD (ESP32-2432S028R) has NO PCF85063 (RTC), no AXP2101 (PMU) and no
// battery: USB-only board, no backup cell for the clock. This is a real
// hardware limitation:
//
//   - Clock: the epoch is saved to NVS on every rtcSetEpoch() and advances
//     with millis() while the board is powered. It survives a reboot (it is
//     reread in rtcBegin()), but time with the board OFF is not counted:
//     without a crystal/cell, each boot resumes from the last saved value.
//     Effect: offline progress (pet.syncClock) will not count the time it
//     was unplugged. So that rtcSetEpoch() persists across the session it is
//     rewritten every 10 min (minimal flash wear).
//   - Battery: there is none; the stubs report "no battery, USB connected".
//
// TODO(addon): for a real clock, add a DS3231/PCF8563 over I2C with a CR2032
// cell and port this file following rtcbat.cpp (Waveshare). For a real
// battery, an 18650 + TP4056 + divider into an ADC would do for a rough voltage.

static Preferences prefs;
static uint32_t baseEpoch = 0;     // epoch at the instant baseMillis; 0 = unknown
static uint32_t baseMillis = 0;
static uint32_t lastPersist = 0;

bool rtcBegin() {
  prefs.begin("tamapoke_rtc", false);
  baseEpoch = prefs.getUInt("epoch", 0);
  baseMillis = millis();
  lastPersist = baseMillis;
  if (baseEpoch == 0) Serial.println("CYD: sin hora guardada (se sembrara en setup)");
  return baseEpoch != 0;
}

uint32_t rtcEpoch() {
  if (baseEpoch == 0) return 0;
  uint32_t now = millis();
  uint32_t e = baseEpoch + (now - baseMillis) / 1000;
  // persist periodically so a reboot does not go back more than ~10 min
  if (now - lastPersist > 600000UL) {
    lastPersist = now;
    prefs.putUInt("epoch", e);
  }
  return e;
}

void rtcSetEpoch(uint32_t e) {
  baseEpoch = e;
  baseMillis = millis();
  lastPersist = baseMillis;
  prefs.putUInt("epoch", e);
}

bool batBegin() { return false; }
void pmuEnablePanel() {}          // no rail to switch on: the panel hangs off 3V3
int batPercent() { return -1; }   // -1 = "no battery connected" (existing contract)
bool batCharging() { return false; }
int batMillivolts() { return 0; }
bool usbPresent() { return true; }  // this board only runs plugged into USB

void pwrSetup() {}
bool pwrShortPressed() { return false; }

#endif  // BOARD_CYD_RECT

#pragma region Includes
#include "rtcbat.h"
#include "boards/board_select.h"  // board header defines XPOWERS_CHIP_AXP2101 before XPowersLib.h
#pragma endregion

// PCF85063 (RTC) + AXP2101 (PMU/battery/power key) over the shared I2C bus,
// as on the Waveshare. Boards without them use rtcbat_cyd.cpp instead.

#if BOARD_HAS_RTC_CHIP
#pragma region PCF85063 real-time clock

#pragma region Includes (RTC driver)
#include <Wire.h>
#include <time.h>
#include <SensorPCF85063.hpp>
#pragma endregion

#pragma region RTC validity window and state
// a year outside this window means the RTC lost its time (no backup cell)
#define RTC_VALID_YEAR_MIN 2025
#define RTC_VALID_YEAR_MAX 2120

static SensorPCF85063 rtc;
static bool rtcOk = false;
#pragma endregion

bool rtcBegin() {
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("PCF85063 not detected");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  RTC_DateTime rtcDateTime = rtc.getDateTime();
  if (rtcDateTime.getYear() < RTC_VALID_YEAR_MIN || rtcDateTime.getYear() > RTC_VALID_YEAR_MAX) return 0;  // no valid time
  struct tm calendarTime = {};
  calendarTime.tm_year = rtcDateTime.getYear() - 1900;
  calendarTime.tm_mon = rtcDateTime.getMonth() - 1;
  calendarTime.tm_mday = rtcDateTime.getDay();
  calendarTime.tm_hour = rtcDateTime.getHour();
  calendarTime.tm_min = rtcDateTime.getMinute();
  calendarTime.tm_sec = rtcDateTime.getSecond();
  time_t epochSeconds = mktime(&calendarTime);  // default TZ = UTC, consistent with gmtime_r
  return epochSeconds > 0 ? (uint32_t)epochSeconds : 0;
}

void rtcSetEpoch(uint32_t e) {
  if (!rtcOk) return;
  time_t epochSeconds = e;
  struct tm calendarTime;
  gmtime_r(&epochSeconds, &calendarTime);
  rtc.setDateTime(RTC_DateTime(calendarTime.tm_year + 1900, calendarTime.tm_mon + 1, calendarTime.tm_mday,
                               calendarTime.tm_hour, calendarTime.tm_min, calendarTime.tm_sec));
}

// the RTC chip keeps the time itself: nothing to persist
void rtcPersistIfDue() {}

#pragma endregion
#endif  // BOARD_HAS_RTC_CHIP

#if BOARD_HAS_PMU
#pragma region AXP2101 power management: panel rail, battery, power key

#pragma region Includes (PMU driver)
#include <Wire.h>
#include <XPowersLib.h>
#pragma endregion

#pragma region PMU state and power cache
// the power state (I2C) is cached for ~2 s: reading it every loop frame added
// pointless I2C traffic and could oscillate (brightness flicker)
#define POWER_CACHE_INTERVAL_MS 2000
#define PANEL_RAIL_MILLIVOLTS 3300  // BLDO1 = OLED VDD

static XPowersPMU pmu;
static bool pmuOk = false;
static uint32_t powerCacheTimestampMs = 0;  // 0 = never read
static int cachedPercent = -1;
static bool cachedCharging = false, cachedUsbPresent = true;
static int cachedMillivolts = 0;
#pragma endregion

static bool beginPmuOnSharedBus() {
  return pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
}

// Turns on the AMOLED power supply. On the Waveshare 1.75 the panel (OLED VDD)
// hangs off the AXP2101's 3.3V BLDO1 rail. The firmware assumed it was on; if
// the PMU resets (full drain), BLDO1 stays OFF and the screen looks black even
// though everything else works. It must be called BEFORE gfx->begin().
// It is the first PMU call in setup(), so it also starts the driver (pmuOk).
void pmuEnablePanel() {
  pmuOk = beginPmuOnSharedBus();
  if (!pmuOk) {
    Serial.println("AXP2101 not detected (pmuEnablePanel)");
    return;
  }
  pmu.setBLDO1Voltage(PANEL_RAIL_MILLIVOLTS);
  pmu.enableBLDO1();
}

bool batBegin() {
  if (!pmuOk) pmuOk = beginPmuOnSharedBus();  // retry only if pmuEnablePanel() did not find it
  if (!pmuOk) Serial.println("AXP2101 not detected");
  return pmuOk;
}

static void refreshPower() {
  uint32_t nowMs = millis();
  if (powerCacheTimestampMs && nowMs - powerCacheTimestampMs < POWER_CACHE_INTERVAL_MS) return;
  powerCacheTimestampMs = nowMs ? nowMs : 1;
  if (!pmuOk) {
    cachedPercent = -1;
    cachedCharging = false;
    cachedUsbPresent = true;
    cachedMillivolts = 0;
    return;
  }
  cachedPercent = pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1;
  cachedCharging = pmu.isCharging();
  cachedUsbPresent = pmu.isVbusIn();
  cachedMillivolts = (int)pmu.getBattVoltage();
}

int batPercent() { refreshPower(); return cachedPercent; }
bool batCharging() { refreshPower(); return cachedCharging; }
int batMillivolts() { refreshPower(); return cachedMillivolts; }
bool usbPresent() { refreshPower(); return cachedUsbPresent; }

void pwrSetup() {
  if (!pmuOk) return;
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.clearIrqStatus();
}

bool pwrShortPressed() {
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool shortPressSeen = pmu.isPekeyShortPressIrq();
  if (shortPressSeen) pmu.clearIrqStatus();
  return shortPressSeen;
}

#pragma endregion
#endif  // BOARD_HAS_PMU

#pragma once

// WiFi station: on/off switch (persisted), asynchronous network scan sorted by
// signal strength, and connection to one saved network. The saved password is
// stored in NVS encrypted with AES-256-GCM (see wifi_manager.cpp for the key).
// Everything is non-blocking: wifiUpdate() advances the scan and the connection
// from loop(). On boards without WiFi (BOARD_HAS_WIFI 0, the CYD) every call is
// a stub: WiFi reads as off and the WiFi library is not linked.

#pragma region Includes
#include <stdint.h>
#pragma endregion

#pragma region Limits
#define WIFI_SSID_MAX_LENGTH 32       // 802.11 limit
#define WIFI_PASSWORD_MAX_LENGTH 63   // WPA passphrase limit
#define WIFI_SCAN_MAX_RESULTS 24      // strongest networks kept after a scan
#pragma endregion

#pragma region Data structures
struct WifiScanResult {
  char ssid[WIFI_SSID_MAX_LENGTH + 1];
  int32_t rssi;      // dBm
  uint8_t channel;
  uint8_t authMode;  // wifi_auth_mode_t
};

enum WifiConnectionState : uint8_t {
  WIFI_CONNECTION_IDLE,        // not connected and not trying
  WIFI_CONNECTION_CONNECTING,  // wifiConnectToNetwork() is waiting for the result
  WIFI_CONNECTION_CONNECTED,
  WIFI_CONNECTION_FAILED       // the last attempt failed (shown for a few seconds)
};

enum WifiSecurityKind : uint8_t {
  WIFI_SECURITY_OPEN,        // no password (open or OWE)
  WIFI_SECURITY_PASSWORD,    // WEP / WPA / WPA2 / WPA3 personal / WAPI
  WIFI_SECURITY_ENTERPRISE   // needs a user name too: not supported
};
#pragma endregion

#pragma region Public API
void wifiBegin();                    // setup(): loads the switch and connects to the saved network
void wifiUpdate(uint32_t now);       // loop(): scan completion, connection result and timeout

bool wifiIsEnabled();
void wifiSetEnabled(bool enabled);   // turns the radio on/off and persists the choice (stays off if it cannot start)

bool wifiStartScan();                // false if WiFi is off or a scan is already running
bool wifiIsScanning();
bool wifiHasScanned();               // a scan finished since WiFi was turned on
uint8_t wifiScanResultCount();
const WifiScanResult &wifiScanResultAt(uint8_t resultIndex);  // sorted, strongest first

// Starts connecting (password may be "" for open networks). The credentials are
// saved, password encrypted, only once the connection succeeds.
void wifiConnectToNetwork(const char *ssid, const char *password);
WifiConnectionState wifiConnectionState();
const char *wifiConnectedSsid();     // "" when not connected
bool wifiIsConnectedToSsid(const char *ssid);

#pragma region Display helpers
const char *wifiSecurityLabel(uint8_t authMode);  // "WPA2", "WPA2/WPA3"... ("" for open)
WifiSecurityKind wifiSecurityKindOf(uint8_t authMode);
bool wifiIsChannelOn5GHz(uint8_t channel);
uint8_t wifiSignalBarsForRssi(int32_t rssi);  // 1..4 lit parts of the WiFi icon (the dot is always lit)
#pragma endregion
#pragma endregion

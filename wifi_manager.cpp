#pragma region Includes
#include "wifi_manager.h"
#include "boards/board_select.h"  // BOARD_HAS_WIFI
#pragma endregion

#if BOARD_HAS_WIFI

#pragma region WiFi includes
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <string.h>
#include "esp_random.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"  // mbedtls_platform_zeroize: a wipe the compiler cannot drop
#pragma endregion

#pragma region Configuration
#define WIFI_NVS_NAMESPACE "tamapoke"
#define WIFI_NVS_KEY_ENABLED "wifiOn"
#define WIFI_NVS_KEY_SSID "wifiSsid"
#define WIFI_NVS_KEY_PASSWORD "wifiPass"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_CONNECT_FAILURE_GRACE_MS 2000  // ignore a stale failure status right after begin()
#define WIFI_FAILURE_SHOWN_MS 5000
#pragma endregion

#pragma region Password encryption format
// Saved password blob: [version][IV 12][GCM tag 16][ciphertext, same length as the password].
// The SSID is the additional authenticated data, so a blob only decrypts for its own network.
//
// Key = SHA-256(label + this chip's eFuse MAC). The password is never in flash in
// plain text and an NVS dump does not decrypt on another board, but whoever holds
// THIS board can derive the key too. Real protection against that needs the
// ESP32's flash encryption + NVS encryption (eFuse burn, not done here).
#define WIFI_PASSWORD_BLOB_VERSION 1
#define WIFI_PASSWORD_IV_LENGTH 12
#define WIFI_PASSWORD_TAG_LENGTH 16
#define WIFI_PASSWORD_KEY_LENGTH 32
#define WIFI_PASSWORD_BLOB_HEADER_LENGTH (1 + WIFI_PASSWORD_IV_LENGTH + WIFI_PASSWORD_TAG_LENGTH)
#define WIFI_PASSWORD_BLOB_MAX_LENGTH (WIFI_PASSWORD_BLOB_HEADER_LENGTH + WIFI_PASSWORD_MAX_LENGTH)
static const char WIFI_KEY_DERIVATION_LABEL[] = "TamaPoke wifi password key v1";
#pragma endregion

#pragma region State
static bool wifiEnabled = false;
static bool wifiScanRunning = false;
static bool wifiScanFinished = false;
static WifiScanResult wifiScanResults[WIFI_SCAN_MAX_RESULTS];
static uint8_t wifiScanResultTotal = 0;
static WifiConnectionState wifiState = WIFI_CONNECTION_IDLE;
static uint32_t wifiConnectStartedAt = 0;
static uint32_t wifiFailedAt = 0;
static bool wifiWasConnected = false;
static char wifiCurrentSsid[WIFI_SSID_MAX_LENGTH + 1] = "";  // network we are connected to
// attempt in progress: kept in RAM until the result, then saved (success) and wiped either way
static char wifiPendingSsid[WIFI_SSID_MAX_LENGTH + 1] = "";
static char wifiPendingPassword[WIFI_PASSWORD_MAX_LENGTH + 1] = "";
#pragma endregion

#pragma region Internal helpers: password encryption
static void deriveWifiPasswordKey(uint8_t key[WIFI_PASSWORD_KEY_LENGTH]) {
  const size_t labelLength = sizeof(WIFI_KEY_DERIVATION_LABEL) - 1;
  uint8_t keyMaterial[sizeof(WIFI_KEY_DERIVATION_LABEL) - 1 + 6];
  memcpy(keyMaterial, WIFI_KEY_DERIVATION_LABEL, labelLength);
  uint64_t chipMac = ESP.getEfuseMac();
  for (int byteIndex = 0; byteIndex < 6; byteIndex++) keyMaterial[labelLength + byteIndex] = (uint8_t)(chipMac >> (8 * byteIndex));
  mbedtls_sha256(keyMaterial, sizeof(keyMaterial), key, 0);
  mbedtls_platform_zeroize(keyMaterial, sizeof(keyMaterial));
}

// Returns the blob length, 0 on failure. blob must hold WIFI_PASSWORD_BLOB_MAX_LENGTH bytes.
static size_t encryptWifiPassword(const char *ssid, const char *password, uint8_t *blob) {
  size_t passwordLength = strlen(password);
  if (passwordLength > WIFI_PASSWORD_MAX_LENGTH) return 0;
  uint8_t *initializationVector = blob + 1;
  uint8_t *authenticationTag = initializationVector + WIFI_PASSWORD_IV_LENGTH;
  uint8_t *ciphertext = authenticationTag + WIFI_PASSWORD_TAG_LENGTH;
  blob[0] = WIFI_PASSWORD_BLOB_VERSION;
  esp_fill_random(initializationVector, WIFI_PASSWORD_IV_LENGTH);  // fresh IV on every save

  uint8_t key[WIFI_PASSWORD_KEY_LENGTH];
  deriveWifiPasswordKey(key);
  mbedtls_gcm_context gcmContext;
  mbedtls_gcm_init(&gcmContext);
  int encryptResult = mbedtls_gcm_setkey(&gcmContext, MBEDTLS_CIPHER_ID_AES, key, WIFI_PASSWORD_KEY_LENGTH * 8);
  if (encryptResult == 0) {
    encryptResult = mbedtls_gcm_crypt_and_tag(&gcmContext, MBEDTLS_GCM_ENCRYPT, passwordLength,
                                              initializationVector, WIFI_PASSWORD_IV_LENGTH,
                                              (const uint8_t *)ssid, strlen(ssid),
                                              (const uint8_t *)password, ciphertext,
                                              WIFI_PASSWORD_TAG_LENGTH, authenticationTag);
  }
  mbedtls_gcm_free(&gcmContext);
  mbedtls_platform_zeroize(key, sizeof(key));
  return encryptResult == 0 ? WIFI_PASSWORD_BLOB_HEADER_LENGTH + passwordLength : 0;
}

// password must hold WIFI_PASSWORD_MAX_LENGTH + 1 bytes. False if the blob is
// damaged, from another board or for another SSID.
static bool decryptWifiPassword(const char *ssid, const uint8_t *blob, size_t blobLength, char *password) {
  password[0] = 0;
  if (blobLength < WIFI_PASSWORD_BLOB_HEADER_LENGTH || blobLength > WIFI_PASSWORD_BLOB_MAX_LENGTH) return false;
  if (blob[0] != WIFI_PASSWORD_BLOB_VERSION) return false;
  size_t passwordLength = blobLength - WIFI_PASSWORD_BLOB_HEADER_LENGTH;
  const uint8_t *initializationVector = blob + 1;
  const uint8_t *authenticationTag = initializationVector + WIFI_PASSWORD_IV_LENGTH;
  const uint8_t *ciphertext = authenticationTag + WIFI_PASSWORD_TAG_LENGTH;

  uint8_t key[WIFI_PASSWORD_KEY_LENGTH];
  deriveWifiPasswordKey(key);
  mbedtls_gcm_context gcmContext;
  mbedtls_gcm_init(&gcmContext);
  int decryptResult = mbedtls_gcm_setkey(&gcmContext, MBEDTLS_CIPHER_ID_AES, key, WIFI_PASSWORD_KEY_LENGTH * 8);
  if (decryptResult == 0) {
    decryptResult = mbedtls_gcm_auth_decrypt(&gcmContext, passwordLength,
                                             initializationVector, WIFI_PASSWORD_IV_LENGTH,
                                             (const uint8_t *)ssid, strlen(ssid),
                                             authenticationTag, WIFI_PASSWORD_TAG_LENGTH,
                                             ciphertext, (uint8_t *)password);
  }
  mbedtls_gcm_free(&gcmContext);
  mbedtls_platform_zeroize(key, sizeof(key));
  if (decryptResult != 0) {
    mbedtls_platform_zeroize(password, WIFI_PASSWORD_MAX_LENGTH + 1);
    return false;
  }
  password[passwordLength] = 0;
  return true;
}
#pragma endregion

#pragma region Internal helpers: saved network
static void saveWifiCredentials(const char *ssid, const char *password) {
  uint8_t passwordBlob[WIFI_PASSWORD_BLOB_MAX_LENGTH];
  size_t blobLength = encryptWifiPassword(ssid, password, passwordBlob);
  if (blobLength == 0) {
    Serial.println("WiFi: password encryption failed, network not saved");
    return;
  }
  Preferences preferences;
  preferences.begin(WIFI_NVS_NAMESPACE, false);
  preferences.putString(WIFI_NVS_KEY_SSID, ssid);
  preferences.putBytes(WIFI_NVS_KEY_PASSWORD, passwordBlob, blobLength);
  preferences.end();
}

// ssid: WIFI_SSID_MAX_LENGTH + 1 bytes, password: WIFI_PASSWORD_MAX_LENGTH + 1 bytes
static bool loadWifiCredentials(char *ssid, char *password) {
  uint8_t passwordBlob[WIFI_PASSWORD_BLOB_MAX_LENGTH];
  Preferences preferences;
  preferences.begin(WIFI_NVS_NAMESPACE, true);  // read-only
  bool hasSavedNetwork = preferences.isKey(WIFI_NVS_KEY_SSID) && preferences.isKey(WIFI_NVS_KEY_PASSWORD);
  size_t blobLength = 0;
  if (hasSavedNetwork) {
    preferences.getString(WIFI_NVS_KEY_SSID, ssid, WIFI_SSID_MAX_LENGTH + 1);
    blobLength = preferences.getBytes(WIFI_NVS_KEY_PASSWORD, passwordBlob, sizeof(passwordBlob));
  }
  preferences.end();
  if (!hasSavedNetwork || ssid[0] == 0) return false;
  return decryptWifiPassword(ssid, passwordBlob, blobLength, password);
}

// Starts connecting to the saved network in the background (no status shown)
static void connectToSavedWifiNetwork() {
  char savedSsid[WIFI_SSID_MAX_LENGTH + 1] = "";
  char savedPassword[WIFI_PASSWORD_MAX_LENGTH + 1] = "";
  if (loadWifiCredentials(savedSsid, savedPassword)) WiFi.begin(savedSsid, savedPassword);
  mbedtls_platform_zeroize(savedPassword, sizeof(savedPassword));
}

static void wipePendingWifiAttempt() {
  mbedtls_platform_zeroize(wifiPendingPassword, sizeof(wifiPendingPassword));
  wifiPendingSsid[0] = 0;
}
#pragma endregion

#pragma region Internal helpers: radio
// False when the WiFi stack cannot start (on the CYD: not enough internal RAM
// next to the framebuffer). Logs the heap either way, to size the budget.
static bool startWifiRadio() {
  Serial.printf("WiFi: starting, heap free=%u largest block=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  WiFi.persistent(false);  // the IDF must never keep its own plain-text copy in NVS
  if (!WiFi.mode(WIFI_STA)) {
    WiFi.mode(WIFI_OFF);  // releases whatever the failed start allocated
    Serial.printf("WiFi: start FAILED, heap free=%u largest block=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    return false;
  }
  WiFi.setAutoReconnect(true);
  Serial.printf("WiFi: started, heap free=%u largest block=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  return true;
}

static void persistWifiEnabled(bool enabled) {
  Preferences preferences;
  preferences.begin(WIFI_NVS_NAMESPACE, false);
  preferences.putBool(WIFI_NVS_KEY_ENABLED, enabled);
  preferences.end();
}

static void stopWifiRadio() {
  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // frees the WiFi stack's RAM
}
#pragma endregion

#pragma region Internal helpers: scan results
static void removeScanResultAt(uint8_t resultIndex) {
  for (uint8_t moveIndex = resultIndex; moveIndex + 1 < wifiScanResultTotal; moveIndex++)
    wifiScanResults[moveIndex] = wifiScanResults[moveIndex + 1];
  wifiScanResultTotal--;
}

// Keeps the list sorted strongest first; when it is full the weakest entry drops out
static void insertScanResultSorted(const WifiScanResult &candidate) {
  uint8_t insertIndex = 0;
  while (insertIndex < wifiScanResultTotal && wifiScanResults[insertIndex].rssi >= candidate.rssi) insertIndex++;
  if (insertIndex >= WIFI_SCAN_MAX_RESULTS) return;
  uint8_t lastIndex = wifiScanResultTotal < WIFI_SCAN_MAX_RESULTS ? wifiScanResultTotal : WIFI_SCAN_MAX_RESULTS - 1;
  for (uint8_t moveIndex = lastIndex; moveIndex > insertIndex; moveIndex--)
    wifiScanResults[moveIndex] = wifiScanResults[moveIndex - 1];
  wifiScanResults[insertIndex] = candidate;
  if (wifiScanResultTotal < WIFI_SCAN_MAX_RESULTS) wifiScanResultTotal++;
}

static void copySortedScanResults(int16_t foundCount) {
  wifiScanResultTotal = 0;
  for (int16_t networkIndex = 0; networkIndex < foundCount; networkIndex++) {
    String ssid = WiFi.SSID(networkIndex);
    if (ssid.length() == 0) continue;  // hidden network: nothing to show or type a password for

    WifiScanResult candidate;
    strncpy(candidate.ssid, ssid.c_str(), WIFI_SSID_MAX_LENGTH);
    candidate.ssid[WIFI_SSID_MAX_LENGTH] = 0;
    candidate.rssi = WiFi.RSSI(networkIndex);
    candidate.channel = (uint8_t)WiFi.channel(networkIndex);
    candidate.authMode = (uint8_t)WiFi.encryptionType(networkIndex);

    #pragma region Same name on the same band (mesh / repeaters): keep only the strongest access point
    bool weakerDuplicate = false;
    for (uint8_t resultIndex = 0; resultIndex < wifiScanResultTotal; resultIndex++) {
      const WifiScanResult &existing = wifiScanResults[resultIndex];
      if (strcmp(existing.ssid, candidate.ssid) != 0 ||
          wifiIsChannelOn5GHz(existing.channel) != wifiIsChannelOn5GHz(candidate.channel)) continue;
      if (existing.rssi >= candidate.rssi) weakerDuplicate = true;
      else removeScanResultAt(resultIndex);
      break;
    }
    if (weakerDuplicate) continue;
    #pragma endregion

    insertScanResultSorted(candidate);
  }
}
#pragma endregion

#pragma region Public API: setup and loop
void wifiBegin() {
  WiFi.persistent(false);
  Preferences preferences;
  preferences.begin(WIFI_NVS_NAMESPACE, true);  // read-only
  wifiEnabled = preferences.getBool(WIFI_NVS_KEY_ENABLED, false);
  preferences.end();
  if (!wifiEnabled) return;
  if (!startWifiRadio()) {
    wifiEnabled = false;  // stays off this session; the saved choice is kept for the next boot
    return;
  }
  connectToSavedWifiNetwork();
}

void wifiUpdate(uint32_t now) {
  if (!wifiEnabled) return;

  #pragma region Scan completion
  if (wifiScanRunning) {
    int16_t scanStatus = WiFi.scanComplete();
    if (scanStatus >= 0) {
      copySortedScanResults(scanStatus);
      WiFi.scanDelete();  // the driver's copy is no longer needed
      wifiScanRunning = false;
      wifiScanFinished = true;
    } else if (scanStatus == WIFI_SCAN_FAILED) {
      wifiScanResultTotal = 0;
      wifiScanRunning = false;
      wifiScanFinished = true;
    }
  }
  #pragma endregion

  #pragma region Connection attempt: success saves the network, failure falls back to the saved one
  wl_status_t stationStatus = WiFi.status();
  bool connected = (stationStatus == WL_CONNECTED);
  if (wifiState == WIFI_CONNECTION_CONNECTING) {
    uint32_t elapsed = now - wifiConnectStartedAt;
    bool reportedFailure = elapsed > WIFI_CONNECT_FAILURE_GRACE_MS &&
                           (stationStatus == WL_CONNECT_FAILED || stationStatus == WL_NO_SSID_AVAIL);
    if (connected) {
      saveWifiCredentials(wifiPendingSsid, wifiPendingPassword);
      wipePendingWifiAttempt();
      wifiState = WIFI_CONNECTION_CONNECTED;
    } else if (reportedFailure || elapsed > WIFI_CONNECT_TIMEOUT_MS) {
      wipePendingWifiAttempt();
      WiFi.disconnect();
      wifiState = WIFI_CONNECTION_FAILED;
      wifiFailedAt = now;
      connectToSavedWifiNetwork();
    }
  } else if (wifiState != WIFI_CONNECTION_FAILED || now - wifiFailedAt > WIFI_FAILURE_SHOWN_MS) {
    wifiState = connected ? WIFI_CONNECTION_CONNECTED : WIFI_CONNECTION_IDLE;
  }
  #pragma endregion

  #pragma region Name of the connected network (read once per change, WiFi.SSID() allocates)
  if (connected != wifiWasConnected || (connected && wifiCurrentSsid[0] == 0)) {
    wifiWasConnected = connected;
    wifiCurrentSsid[0] = 0;
    if (connected) {
      strncpy(wifiCurrentSsid, WiFi.SSID().c_str(), WIFI_SSID_MAX_LENGTH);
      wifiCurrentSsid[WIFI_SSID_MAX_LENGTH] = 0;
    }
  }
  #pragma endregion
}
#pragma endregion

#pragma region Public API: on / off
bool wifiIsEnabled() { return wifiEnabled; }

void wifiSetEnabled(bool enabled) {
  if (enabled == wifiEnabled) return;

  #pragma region Reset the scan and connection state
  wifiScanRunning = false;
  wifiScanFinished = false;
  wifiScanResultTotal = 0;
  wifiState = WIFI_CONNECTION_IDLE;
  wifiWasConnected = false;
  wifiCurrentSsid[0] = 0;
  wipePendingWifiAttempt();
  #pragma endregion

  #pragma region Switch the radio; a failed start leaves WiFi off and is not persisted as on
  if (enabled) {
    if (!startWifiRadio()) return;
    connectToSavedWifiNetwork();
  } else {
    stopWifiRadio();
  }
  wifiEnabled = enabled;
  persistWifiEnabled(enabled);
  #pragma endregion
}
#pragma endregion

#pragma region Public API: scan
bool wifiStartScan() {
  if (!wifiEnabled || wifiScanRunning) return false;
  WiFi.scanDelete();
  wifiScanResultTotal = 0;
  wifiScanFinished = false;
  wifiScanRunning = (WiFi.scanNetworks(true /*async*/, false /*show hidden*/) == WIFI_SCAN_RUNNING);
  if (!wifiScanRunning) wifiScanFinished = true;  // failed to start: shows as an empty result
  return wifiScanRunning;
}

bool wifiIsScanning() { return wifiScanRunning; }
bool wifiHasScanned() { return wifiScanFinished; }
uint8_t wifiScanResultCount() { return wifiScanResultTotal; }
const WifiScanResult &wifiScanResultAt(uint8_t resultIndex) { return wifiScanResults[resultIndex]; }
#pragma endregion

#pragma region Public API: connection
void wifiConnectToNetwork(const char *ssid, const char *password) {
  if (!wifiEnabled || wifiScanRunning) return;
  strncpy(wifiPendingSsid, ssid, WIFI_SSID_MAX_LENGTH);
  wifiPendingSsid[WIFI_SSID_MAX_LENGTH] = 0;
  strncpy(wifiPendingPassword, password, WIFI_PASSWORD_MAX_LENGTH);
  wifiPendingPassword[WIFI_PASSWORD_MAX_LENGTH] = 0;
  WiFi.disconnect();
  WiFi.begin(wifiPendingSsid, wifiPendingPassword);
  wifiState = WIFI_CONNECTION_CONNECTING;
  wifiConnectStartedAt = millis();
}

WifiConnectionState wifiConnectionState() { return wifiState; }
const char *wifiConnectedSsid() { return wifiCurrentSsid; }

bool wifiIsConnectedToSsid(const char *ssid) {
  return wifiState == WIFI_CONNECTION_CONNECTED && wifiCurrentSsid[0] != 0 && strcmp(wifiCurrentSsid, ssid) == 0;
}
#pragma endregion

#pragma region Public API: display helpers
const char *wifiSecurityLabel(uint8_t authMode) {
  switch ((wifi_auth_mode_t)authMode) {
    case WIFI_AUTH_OPEN: return "";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA3_EXT_PSK:
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    case WIFI_AUTH_OWE: return "OWE";
    case WIFI_AUTH_DPP: return "DPP";
    case WIFI_AUTH_WPA_ENTERPRISE: return "WPA-EAP";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_ENT_192:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA3-EAP";
    default: return "?";
  }
}

WifiSecurityKind wifiSecurityKindOf(uint8_t authMode) {
  switch ((wifi_auth_mode_t)authMode) {
    case WIFI_AUTH_OPEN:
    case WIFI_AUTH_OWE: return WIFI_SECURITY_OPEN;
    case WIFI_AUTH_WPA_ENTERPRISE:
    case WIFI_AUTH_WPA2_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENT_192:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return WIFI_SECURITY_ENTERPRISE;
    default: return WIFI_SECURITY_PASSWORD;
  }
}

// 2.4 GHz channels are 1..14; anything above is 5 GHz. The ESP32 and ESP32-S3
// radios are 2.4 GHz only, so on these boards every network shows 2.4 GHz.
bool wifiIsChannelOn5GHz(uint8_t channel) { return channel > 14; }

uint8_t wifiSignalBarsForRssi(int32_t rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -78) return 2;
  return 1;
}
#pragma endregion

#else  // !BOARD_HAS_WIFI

#pragma region Stubs for boards without WiFi (the CYD): WiFi stays off and the WiFi library is never linked
static const WifiScanResult EMPTY_WIFI_SCAN_RESULT = {};

void wifiBegin() {}
void wifiUpdate(uint32_t now) { (void)now; }
bool wifiIsEnabled() { return false; }
void wifiSetEnabled(bool enabled) { (void)enabled; }
bool wifiStartScan() { return false; }
bool wifiIsScanning() { return false; }
bool wifiHasScanned() { return false; }
uint8_t wifiScanResultCount() { return 0; }
const WifiScanResult &wifiScanResultAt(uint8_t resultIndex) { (void)resultIndex; return EMPTY_WIFI_SCAN_RESULT; }
void wifiConnectToNetwork(const char *ssid, const char *password) { (void)ssid; (void)password; }
WifiConnectionState wifiConnectionState() { return WIFI_CONNECTION_IDLE; }
const char *wifiConnectedSsid() { return ""; }
bool wifiIsConnectedToSsid(const char *ssid) { (void)ssid; return false; }
const char *wifiSecurityLabel(uint8_t authMode) { (void)authMode; return ""; }
WifiSecurityKind wifiSecurityKindOf(uint8_t authMode) { (void)authMode; return WIFI_SECURITY_OPEN; }
bool wifiIsChannelOn5GHz(uint8_t channel) { return channel > 14; }
uint8_t wifiSignalBarsForRssi(int32_t rssi) { (void)rssi; return 1; }
#pragma endregion

#endif  // BOARD_HAS_WIFI

// USB upload protocol (to fill the SD without removing it from the board):
//   PUT <path> <bytes>\n  + raw data       -> "OK" ... "DONE"
//   LS\n                                   -> listing of /mons
//   SDINFO\n                               -> total and used bytes
// Use with tools/send_sd.py (or the web installer).

#pragma region Includes
#include "sdmon.h"
#include "sd_host.h"
#pragma endregion

#pragma region Configuration
#define PUT_MAX_FILE_BYTES (4UL * 1024 * 1024)
#define PUT_BLOCK_BYTES 2048        // one acknowledged block of the transfer
#define PUT_READ_TIMEOUT_MS 5000
#define SERIAL_DEFAULT_TIMEOUT_MS 1000
#pragma endregion

#pragma region Command handlers
static void handlePutCommand(const String &line) {
  #pragma region Parse and validate the request
  int lastSpace = line.lastIndexOf(' ');
  String path = line.substring(4, lastSpace);
  uint32_t size = line.substring(lastSpace + 1).toInt();
  if (!sdReady || size == 0 || size > PUT_MAX_FILE_BYTES) {
    Serial.println("ERR");
    return;
  }
  if (!path.startsWith("/")) path = "/" + path;
  // restricts writes to /mons/: the path arrives as-is from the serial line,
  // unsanitized, so a crafted PUT could write anywhere on the card. The two
  // real clients (tools/send_sd.py and web/index.html) already send names
  // with the mons/ prefix.
  if (!path.startsWith("/mons/") || path.indexOf("..") >= 0) {
    Serial.println("ERR");
    return;
  }
  #pragma endregion

  #pragma region Transfer buffer (heap only while a PUT runs, not a static in .bss)
  uint8_t *blockBuffer = (uint8_t *)malloc(PUT_BLOCK_BYTES);
  if (!blockBuffer) {
    Serial.println("ERR");
    return;
  }
  #pragma endregion

  #pragma region Replace the file
  // FILE_WRITE APPENDS if the file already exists, so retrying a half-written
  // one made it longer instead of replacing it: it left a corrupt sprite larger
  // than the original. This matters more now that the installer resumes
  // interrupted transfers, because it retries exactly the ones that failed.
  if (SDHOST.exists(path)) SDHOST.remove(path);
  File outputFile = SDHOST.open(path, FILE_WRITE);
  if (!outputFile) {
    free(blockBuffer);
    Serial.println("ERR");
    return;
  }
  #pragma endregion

  #pragma region Receive the data block by block
  Serial.println("OK");
  uint32_t remaining = size;
  Serial.setTimeout(PUT_READ_TIMEOUT_MS);
  while (remaining > 0) {
    size_t wantedBytes = remaining > PUT_BLOCK_BYTES ? PUT_BLOCK_BYTES : remaining;
    size_t receivedBytes = Serial.readBytes(blockBuffer, wantedBytes);
    if (receivedBytes == 0) break;  // timeout
    // without checking the return value, a full card or a write failure still
    // reported DONE and left a truncated file on the SD: exactly the corrupt
    // input we then have to guard against when loading the sprite
    if (outputFile.write(blockBuffer, receivedBytes) != receivedBytes) break;
    remaining -= receivedBytes;
    Serial.println("#");  // ack: ready for the next block
  }
  outputFile.close();
  free(blockBuffer);
  Serial.setTimeout(SERIAL_DEFAULT_TIMEOUT_MS);
  sdDirty = (remaining == 0);
  Serial.println(remaining == 0 ? "DONE" : "ERR");
  #pragma endregion
}

static void handleListCommand() {
  File directory = SDHOST.open("/mons");
  if (directory) {
    File entry;
    while ((entry = directory.openNextFile())) {
      Serial.printf("%s %u\n", entry.name(), (unsigned)entry.size());
      entry.close();
    }
    directory.close();
  }
  Serial.println("DONE");
}
#pragma endregion

#pragma region Public API
bool sdSerialCommand(const String &line) {
  if (line.startsWith("PUT ")) {
    handlePutCommand(line);
    return true;
  } else if (line == "SDINFO") {  // remote diagnosis of "my SD is not recognized"
    Serial.printf("total=%llu used=%llu\n", SDHOST.totalBytes(), SDHOST.usedBytes());
    Serial.println("DONE");
    return true;
  } else if (line == "LS") {
    handleListCommand();
    return true;
  }
  return false;
}
#pragma endregion

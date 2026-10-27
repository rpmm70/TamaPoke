// Sprites and gallery thumbnails read from the SD card: PMD/TPK2 multi-action
// sprites (blob or streaming), legacy TPK1 sprites and thumbs.bin. The USB
// upload protocol (sdSerialCommand) is in sd_upload.cpp.

#pragma region Includes
#include "sdmon.h"
#include "sd_host.h"
#pragma endregion

#pragma region File format constants
// TPK2 (PMD) file: "TPK2", action count (1 byte), palette count (2 bytes),
// palette, then per action a 4-byte header (id, w, h, frames), ms[] and the frames
#define PMD_FILE_HEADER_BYTES 7
#define PMD_ACTION_HEADER_BYTES 4
#define PMD_MAX_FILE_BYTES (3UL * 1024 * 1024)
#define PMD_MAX_FRAMES ((int)(sizeof(PmdAct::ms) / sizeof(PmdAct::ms[0])))  // 24

// thumbs.bin: "TPTH", count (2 bytes), offset table (4 bytes per species), then
// per thumbnail w, h, palette count (3 bytes), palette, indices
#define THUMBS_FILE_PATH "/mons/thumbs.bin"
#define THUMBS_HEADER_BYTES 6
#define THUMB_HEADER_BYTES 3
#define THUMBS_MIN_FILE_BYTES 10
#define THUMBS_MAX_FILE_BYTES (2UL * 1024 * 1024)

#define TRANSPARENT_INDEX 0xFF
#define DEFAULT_FRAME_MS 100  // a 0 ms frame becomes this: pmdFrameAt() would spin without advancing
#pragma endregion

#pragma region Shared state
bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;
#pragma endregion

#pragma region Internal helpers: memory and files
// big buffers go to PSRAM when the board has it; without PSRAM (the CYD)
// ps_malloc would return NULL, so internal RAM is used instead. CAREFUL: PMD
// sprite blobs average ~260 KB (up to 3 MB) and internal RAM only has
// ~100-200 KB contiguous, which is why PmdMon and SdThumbs stream without PSRAM.
static inline void *bigAlloc(size_t byteCount) {
  return psramFound() ? ps_malloc(byteCount) : malloc(byteCount);
}

// opens /mons/<prefix>[s]NNN.bin; a missing shiny variant falls back to the
// normal one. pathOut receives the path that was tried last.
static File openMonFile(const char *prefix, uint8_t dexNum, bool shiny, char *pathOut, size_t pathOutSize) {
  snprintf(pathOut, pathOutSize, "/mons/%s%s%03u.bin", prefix, shiny ? "s" : "", dexNum);
  File monFile = SDHOST.open(pathOut, FILE_READ);
  if (!monFile && shiny) {  // no shiny variant: use the normal one
    snprintf(pathOut, pathOutSize, "/mons/%s%03u.bin", prefix, dexNum);
    monFile = SDHOST.open(pathOut, FILE_READ);
  }
  return monFile;
}
#pragma endregion

#pragma region Internal helpers: PMD format
struct PmdActionHeader {
  uint8_t id, w, h, frames;
  uint32_t frameBytes;    // w * h
  uint32_t payloadBytes;  // ms[] plus every frame
};

// action count and palette size from the 7-byte file header (magic already checked)
static bool parsePmdFileHeader(const uint8_t *header, uint32_t fileSize, uint8_t &actionCount,
                               uint16_t &paletteCount) {
  actionCount = header[4];
  memcpy(&paletteCount, header + 5, 2);
  return paletteCount <= SPRITE_PALETTE_MAX_COLORS &&
         (uint32_t)PMD_FILE_HEADER_BYTES + paletteCount * 2 <= fileSize;
}

// payloadPosition: file offset right after the 4-byte action header. False if the
// action is invalid or its ms[] and frames do not fit in the file (truncated).
static bool parsePmdActionHeader(const uint8_t *rawHeader, uint32_t payloadPosition, uint32_t fileSize,
                                 PmdActionHeader &header) {
  header.id = rawHeader[0];
  header.w = rawHeader[1];
  header.h = rawHeader[2];
  header.frames = rawHeader[3];
  if (header.id >= PMD_NACTS || header.frames > PMD_MAX_FRAMES) return false;
  header.frameBytes = (uint32_t)header.w * header.h;
  header.payloadBytes = (uint32_t)header.frames * 2 + header.frameBytes * header.frames;
  return header.w != 0 && header.h != 0 && header.frames != 0 &&
         payloadPosition + header.payloadBytes <= fileSize;
}

static void applyPmdActionHeader(PmdAct &action, const PmdActionHeader &header) {
  action.w = header.w;
  action.h = header.h;
  action.frames = header.frames;
}

// little-endian ms[] of one action
static void decodeFrameDurations(PmdAct &action, const uint8_t *rawDurations) {
  for (uint8_t frameIndex = 0; frameIndex < action.frames; frameIndex++) {
    action.ms[frameIndex] = rawDurations[frameIndex * 2] | (rawDurations[frameIndex * 2 + 1] << 8);
    if (action.ms[frameIndex] == 0) action.ms[frameIndex] = DEFAULT_FRAME_MS;
  }
}

// row+1 of the lowest row with a visible pixel, 0 if the frame is empty
static uint8_t findLowestOpaqueRow(const uint8_t *frame, uint8_t w, uint8_t h) {
  for (int row = h - 1; row >= 0; row--)
    for (int column = 0; column < w; column++)
      if (frame[row * w + column] != TRANSPARENT_INDEX) return (uint8_t)(row + 1);
  return 0;
}

// ensures fbuf can hold at least `need` bytes (grows, never shrinks)
static bool pmdEnsureBuf(PmdMon &mon, uint32_t need) {
  if (mon.fbufCap >= need) return true;
  uint8_t *grownBuffer = (uint8_t *)realloc(mon.fbuf, need);
  if (!grownBuffer) return false;
  mon.fbuf = grownBuffer;
  mon.fbufCap = need;
  return true;
}
#pragma endregion

#pragma region PMD streaming mode (boards without PSRAM: see the PmdMon comment in sdmon.h)
// Reads ONLY the TPK2 headers (palette, and per action: dimensions, ms[], and
// the position of its data in the file). The pixel data is walked once, frame
// by frame, only to compute `base` (lowest row with content); afterwards it is
// read on demand in PmdMon::frame().
// Cost: one sequential pass over the file when loading a species (~0.3 s on
// average over SPI-SD; up to a few seconds for the largest files).
// Returns false if the file is truncated or invalid (the caller does unload()).
static bool pmdLoadStreaming(PmdMon &mon, File &monFile, uint32_t size, const char *path) {
  #pragma region File header and palette
  uint8_t fileHeader[PMD_FILE_HEADER_BYTES];
  if (!monFile.seek(0) || monFile.read(fileHeader, PMD_FILE_HEADER_BYTES) != PMD_FILE_HEADER_BYTES ||
      memcmp(fileHeader, "TPK2", 4) != 0) return false;
  uint8_t actionCount;
  if (!parsePmdFileHeader(fileHeader, size, actionCount, mon.palCount)) return false;
  if (monFile.read((uint8_t *)mon.pal, mon.palCount * 2) != (int)(mon.palCount * 2)) return false;
  #pragma endregion

  #pragma region Actions
  uint32_t position = PMD_FILE_HEADER_BYTES + (uint32_t)mon.palCount * 2;
  for (uint8_t actionIndex = 0; actionIndex < actionCount && position + PMD_ACTION_HEADER_BYTES <= size; actionIndex++) {
    uint8_t rawHeader[PMD_ACTION_HEADER_BYTES];
    if (monFile.read(rawHeader, PMD_ACTION_HEADER_BYTES) != PMD_ACTION_HEADER_BYTES) return false;
    position += PMD_ACTION_HEADER_BYTES;
    PmdActionHeader header;
    if (!parsePmdActionHeader(rawHeader, position, size, header)) return false;

    PmdAct &action = mon.acts[header.id];
    applyPmdActionHeader(action, header);
    action.data = nullptr;
    uint8_t rawDurations[PMD_MAX_FRAMES * 2];
    if (monFile.read(rawDurations, header.frames * 2) != header.frames * 2) return false;
    decodeFrameDurations(action, rawDurations);
    position += (uint32_t)header.frames * 2;
    action.fileOff = position;

    #pragma region Anchor row: walk every frame once
    if (!pmdEnsureBuf(mon, header.frameBytes)) { Serial.println("no RAM for a PMD frame"); return false; }
    uint8_t base = 1;
    for (uint8_t frameIndex = 0; frameIndex < header.frames; frameIndex++) {
      if (monFile.read(mon.fbuf, header.frameBytes) != (int)header.frameBytes) return false;
      uint8_t frameBase = findLowestOpaqueRow(mon.fbuf, header.w, header.h);
      if (frameBase > base) base = frameBase;
    }
    action.base = base;
    #pragma endregion
    position += header.frameBytes * header.frames;
  }
  #pragma endregion

  #pragma region Streaming state
  strncpy(mon.path, path, sizeof(mon.path) - 1);
  mon.path[sizeof(mon.path) - 1] = 0;
  mon.streaming = true;
  mon.cacheAct = -1;
  mon.cacheFrame = -1;
  mon.loaded = true;
  return true;
  #pragma endregion
}

const uint8_t *PmdMon::frame(uint8_t actId, uint8_t fi) {
  if (!loaded || actId >= PMD_NACTS) return nullptr;
  const PmdAct &action = acts[actId];
  if (!action.frames || fi >= action.frames) return nullptr;
  uint32_t frameBytes = (uint32_t)action.w * action.h;
  if (!streaming) return action.data + (uint32_t)fi * frameBytes;  // blob mode: direct pointer
  if (cacheAct == (int8_t)actId && cacheFrame == (int16_t)fi) return fbuf;  // same frame as before

  // Opened on every read (the handle is not kept): see note in sdmon.h
  File monFile = SDHOST.open(path, FILE_READ);
  if (!monFile) return nullptr;
  bool readOk = monFile.seek(action.fileOff + (uint32_t)fi * frameBytes) &&
                monFile.read(fbuf, frameBytes) == (int)frameBytes;
  monFile.close();
  if (!readOk) { cacheAct = -1; cacheFrame = -1; return nullptr; }
  cacheAct = (int8_t)actId;
  cacheFrame = (int16_t)fi;
  return fbuf;
}
#pragma endregion

#pragma region PMD load and unload
bool PmdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  #pragma region Open the file
  char filePath[sizeof(PmdMon::path)];
  File monFile = openMonFile("p", dexNum, shiny, filePath, sizeof(filePath));
  if (!monFile) return false;
  uint32_t size = monFile.size();
  if (size < PMD_FILE_HEADER_BYTES || size > PMD_MAX_FILE_BYTES) { monFile.close(); return false; }
  #pragma endregion

  #pragma region No PSRAM: the whole blob does not fit -> streaming (see sdmon.h)
  if (!psramFound()) {
    bool loadOk = pmdLoadStreaming(*this, monFile, size, filePath);
    monFile.close();
    if (!loadOk) { unload(); return false; }
    Serial.printf("loaded %s streaming (%u KB on SD, %u B RAM)\n",
                  filePath, (unsigned)(size / 1024), (unsigned)fbufCap);
    return true;
  }
  #pragma endregion

  #pragma region Blob mode: read the whole file
  blob = (uint8_t *)bigAlloc(size);
  if (!blob || monFile.read(blob, size) != size || memcmp(blob, "TPK2", 4) != 0) {
    if (blob) { free(blob); blob = nullptr; }
    monFile.close();
    return false;
  }
  monFile.close();

  uint8_t actionCount;
  if (!parsePmdFileHeader(blob, size, actionCount, palCount)) { unload(); return false; }
  memcpy(pal, blob + PMD_FILE_HEADER_BYTES, palCount * 2);
  #pragma endregion

  #pragma region Actions (pointers into the blob)
  const uint8_t *cursor = blob + PMD_FILE_HEADER_BYTES + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t actionIndex = 0; actionIndex < actionCount && cursor + PMD_ACTION_HEADER_BYTES <= end; actionIndex++) {
    const uint8_t *rawHeader = cursor;
    cursor += PMD_ACTION_HEADER_BYTES;
    PmdActionHeader header;
    if (!parsePmdActionHeader(rawHeader, (uint32_t)(cursor - blob), size, header)) { unload(); return false; }
    PmdAct &action = acts[header.id];
    applyPmdActionHeader(action, header);
    decodeFrameDurations(action, cursor);
    cursor += (uint32_t)header.frames * 2;
    action.data = cursor;
    cursor += header.frameBytes * header.frames;

    #pragma region Lowest row with content in any frame: anchor by the feet
    uint8_t base = 1;
    for (uint8_t frameIndex = 0; frameIndex < header.frames; frameIndex++) {
      uint8_t frameBase = findLowestOpaqueRow(action.data + (uint32_t)frameIndex * header.frameBytes,
                                              header.w, header.h);
      if (frameBase > base) base = frameBase;
    }
    action.base = base;
    #pragma endregion
  }
  #pragma endregion

  loaded = true;
  Serial.printf("loaded %s (%u KB)\n", filePath, (unsigned)(size / 1024));
  return true;
}

void PmdMon::unload() {
  if (blob) {
    free(blob);
    blob = nullptr;
  }
  if (fbuf) {
    free(fbuf);
    fbuf = nullptr;
  }
  fbufCap = 0;
  streaming = false;
  cacheAct = -1;
  cacheFrame = -1;
  path[0] = 0;
  for (auto &action : acts) {
    action.w = action.h = action.frames = action.base = 0;
    action.data = nullptr;
    action.fileOff = 0;
  }
  loaded = false;
}
#pragma endregion

#pragma region Gallery thumbnails
// the w,h,palCount header of a thumbnail at `offset` must fit in the file
static bool thumbHeaderFits(uint32_t offset, uint32_t fileSize) {
  return offset <= fileSize - THUMB_HEADER_BYTES;
}

static uint32_t thumbByteCount(const uint8_t *thumbHeader) {
  return THUMB_HEADER_BYTES + (uint32_t)thumbHeader[2] * 2 + (uint32_t)thumbHeader[0] * thumbHeader[1];
}

// and the whole thumbnail too (the offset comes unchecked from the file)
static bool thumbFits(uint32_t offset, uint32_t byteCount, uint32_t fileSize) {
  return byteCount <= fileSize && offset <= fileSize - byteCount;
}

void SdThumbs::unload() {
  if (data) { free(data); data = nullptr; }
  if (offs) { free(offs); offs = nullptr; }
  if (tbuf) { free(tbuf); tbuf = nullptr; }
  tbufCap = 0;
  cacheDex = -1;
  streaming = false;
  loaded = false;
  count = 0;
  size = 0;
}

bool SdThumbs::load() {
  unload();  // reload without leaking the previous blob
  if (!sdReady) return false;

  #pragma region Open and bound the size
  File thumbsFile = SDHOST.open(THUMBS_FILE_PATH, FILE_READ);
  if (!thumbsFile) {
    Serial.println("no thumbs.bin (gallery without thumbnails)");
    return false;
  }
  uint32_t fileSize = thumbsFile.size();
  // bounds the size: avoids an absurd ps_malloc with a corrupt file
  if (fileSize < THUMBS_MIN_FILE_BYTES || fileSize > THUMBS_MAX_FILE_BYTES) {
    Serial.println("thumbs.bin invalid (size)");
    thumbsFile.close();
    return false;
  }
  #pragma endregion

  #pragma region No PSRAM: only the offset table; each thumbnail is read on request
  if (!psramFound()) {
    uint8_t fileHeader[THUMBS_HEADER_BYTES];
    if (!thumbsFile.seek(0) || thumbsFile.read(fileHeader, THUMBS_HEADER_BYTES) != THUMBS_HEADER_BYTES ||
        memcmp(fileHeader, "TPTH", 4) != 0) {
      Serial.println("thumbs.bin invalid");
      thumbsFile.close();
      return false;
    }
    memcpy(&count, fileHeader + 4, 2);
    if ((uint32_t)THUMBS_HEADER_BYTES + 4UL * count > fileSize || count == 0) {
      Serial.println("thumbs.bin invalid (offset table)");
      thumbsFile.close();
      unload();
      return false;
    }
    offs = (uint32_t *)malloc(4UL * count);
    if (!offs || thumbsFile.read((uint8_t *)offs, 4 * count) != (int)(4 * count)) {
      Serial.println("thumbs.bin: no RAM/read for the table");
      thumbsFile.close();
      unload();
      return false;
    }
    thumbsFile.close();
    size = fileSize;
    streaming = true;
    loaded = true;
    Serial.printf("thumbnails streaming: %u (%u KB on SD)\n", (unsigned)count, (unsigned)(fileSize / 1024));
    return true;
  }
  #pragma endregion

  #pragma region PSRAM: the whole file in memory
  data = (uint8_t *)bigAlloc(fileSize);
  if (!data || thumbsFile.read(data, fileSize) != fileSize || memcmp(data, "TPTH", 4) != 0) {
    Serial.println("thumbs.bin invalid");
    if (data) { free(data); data = nullptr; }
    thumbsFile.close();
    return false;
  }
  thumbsFile.close();
  memcpy(&count, data + 4, 2);
  // the offset table (one per species) must fit entirely in what was read
  if ((uint32_t)THUMBS_HEADER_BYTES + 4UL * count > fileSize) {
    Serial.println("thumbs.bin invalid (offset table)");
    unload();
    return false;
  }
  size = fileSize;
  loaded = true;
  Serial.printf("thumbnails loaded: %u (%u KB)\n", (unsigned)count, (unsigned)(fileSize / 1024));
  return true;
  #pragma endregion
}

const uint8_t *SdThumbs::get(int16_t dex) {
  if (!loaded || dex < 1 || dex > count) return nullptr;

  #pragma region Streaming: read one thumbnail from the SD
  if (streaming) {
    if (cacheDex == dex) return tbuf;  // same thumbnail as the last one requested
    uint32_t thumbOffset = offs[dex - 1];
    if (!thumbHeaderFits(thumbOffset, size)) return nullptr;
    File thumbsFile = SDHOST.open(THUMBS_FILE_PATH, FILE_READ);
    if (!thumbsFile) return nullptr;
    uint8_t thumbHeader[THUMB_HEADER_BYTES];
    if (!thumbsFile.seek(thumbOffset) || thumbsFile.read(thumbHeader, THUMB_HEADER_BYTES) != THUMB_HEADER_BYTES) {
      thumbsFile.close();
      return nullptr;
    }
    uint32_t need = thumbByteCount(thumbHeader);
    if (!thumbFits(thumbOffset, need, size)) { thumbsFile.close(); return nullptr; }  // corrupt offset/size
    if (tbufCap < need) {
      uint8_t *grownBuffer = (uint8_t *)realloc(tbuf, need);
      if (!grownBuffer) { thumbsFile.close(); return nullptr; }
      tbuf = grownBuffer;
      tbufCap = need;
    }
    memcpy(tbuf, thumbHeader, THUMB_HEADER_BYTES);
    bool readOk = thumbsFile.read(tbuf + THUMB_HEADER_BYTES, need - THUMB_HEADER_BYTES) ==
                  (int)(need - THUMB_HEADER_BYTES);
    thumbsFile.close();
    if (!readOk) { cacheDex = -1; return nullptr; }
    cacheDex = dex;
    return tbuf;
  }
  #pragma endregion

  #pragma region In memory: pointer into the blob
  uint32_t thumbOffset;
  memcpy(&thumbOffset, data + THUMBS_HEADER_BYTES + 4 * (dex - 1), 4);
  // without these checks, a truncated or corrupt thumbs.bin caused reads
  // outside the PSRAM allocation (size >= THUMBS_MIN_FILE_BYTES always)
  if (!thumbHeaderFits(thumbOffset, size)) return nullptr;
  if (!thumbFits(thumbOffset, thumbByteCount(data + thumbOffset), size)) return nullptr;
  return data + thumbOffset;
  #pragma endregion
}
#pragma endregion

#pragma region SD mount
bool sdBegin() {
#if BOARD_SD_SDMMC
  SDHOST.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  sdReady = SDHOST.begin("/sdcard", true /* 1-bit mode */, true /* format if mount fails */);
#elif BOARD_SD_SPI
  // SD on its own SPI bus (pins 18/19/23, CS 5); not shared with the display or touch.
  // Not formatted if it fails to mount (unlike the Waveshare): better not to touch someone else's card.
  // At most 3 open files (default 5): each costs a 4 KB sector cache in internal
  // RAM, which the CYD needs for the framebuffer. The game opens one file at a
  // time; the LS serial command holds the directory plus one entry.
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SDHOST.begin(SD_CS, SPI, 20000000, "/sd", 3 /* max open files */);
#endif
  if (sdReady) {
    Serial.printf("SD mounted: %llu MB\n", SDHOST.cardSize() / (1024ULL * 1024ULL));
    SDHOST.mkdir("/mons");
  } else {
    Serial.println("SD not detected (the game uses the flash sprites)");
  }
  return sdReady;
}
#pragma endregion

#pragma region Legacy TPK1 sprite
bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  #pragma region Open the file
  char filePath[sizeof(PmdMon::path)];
  File monFile = openMonFile("", dexNum, shiny, filePath, sizeof(filePath));
  if (!monFile) {
    Serial.printf("%s does not exist\n", filePath);
    return false;
  }
  #pragma endregion

  #pragma region Header and palette
  char magic[4];
  uint16_t header[4];
  if (monFile.read((uint8_t *)magic, 4) != 4 || memcmp(magic, "TPK1", 4) != 0 ||
      monFile.read((uint8_t *)header, 8) != 8) {
    monFile.close();
    return false;
  }
  w = header[0];
  h = header[1];
  frames = header[2];
  frameMs = header[3];
  // bounds the dimensions: avoids an overflowed or absurd size with a corrupt file
  if (monFile.read((uint8_t *)&palCount, 2) != 2 || palCount > SPRITE_PALETTE_MAX_COLORS ||
      w == 0 || w > 256 || h == 0 || h > 256 || frames == 0 || frames > 64) {
    monFile.close();
    return false;
  }
  if (monFile.read((uint8_t *)pal, palCount * 2) != palCount * 2) {
    monFile.close();
    return false;
  }
  #pragma endregion

  #pragma region Frames
  uint32_t frameDataBytes = (uint32_t)w * h * frames;
  data = (uint8_t *)bigAlloc(frameDataBytes);
  if (!data) {
    Serial.println("no PSRAM for the sprite");
    monFile.close();
    return false;
  }
  uint32_t bytesRead = monFile.read(data, frameDataBytes);
  monFile.close();
  if (bytesRead != frameDataBytes) {
    Serial.printf("%s truncated (%u of %u)\n", filePath, (unsigned)bytesRead, (unsigned)frameDataBytes);
    unload();
    return false;
  }
  #pragma endregion

  // integer zoom so the creature is ~200 px tall on screen
  scale = 200 / h;
  if (scale < 2) scale = 2;
  if (scale > 5) scale = 5;

  Serial.printf("loaded %s: %ux%u x%u frames @%ums, scale %u\n",
                filePath, (unsigned)w, (unsigned)h, (unsigned)frames, (unsigned)frameMs, (unsigned)scale);
  loaded = true;
  return true;
}

void SdMon::unload() {
  if (data) {
    free(data);
    data = nullptr;
  }
  loaded = false;
}
#pragma endregion

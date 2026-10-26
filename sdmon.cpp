#include "sdmon.h"
#include "boards/board_select.h"
#include <FS.h>
#if BOARD_IS_ROUND
  #include <SD_MMC.h>
  #define SDHOST SD_MMC
#else
  // The CYD has no SDMMC wired: the microSD goes over SPI (VSPI by default)
  #include <SD.h>
  #include <SPI.h>
  #define SDHOST SD
#endif

// ps_malloc returns NULL on a board WITHOUT PSRAM (the CYD): fall back to internal RAM.
// CAREFUL: PMD sprite blobs average ~260 KB (up to 3 MB) and the CYD only has
// ~100-200 KB contiguous, so on the CYD many loads will fail until streaming
// reads are implemented (see CYD_PORT_NOTES.md).
static inline void *bigAlloc(size_t n) {
#if BOARD_IS_ROUND
  return ps_malloc(n);
#else
  return psramFound() ? ps_malloc(n) : malloc(n);
#endif
}

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

// ---------------------------------------------------------------------------
// STREAMING mode (boards without PSRAM): see the PmdMon comment in sdmon.h.
// ---------------------------------------------------------------------------

// ensures fbuf can hold at least `need` bytes (grows, never shrinks)
static bool pmdEnsureBuf(PmdMon &m, uint32_t need) {
  if (m.fbufCap >= need) return true;
  uint8_t *nb = (uint8_t *)realloc(m.fbuf, need);
  if (!nb) return false;
  m.fbuf = nb;
  m.fbufCap = need;
  return true;
}

// Reads ONLY the TPK2 headers (palette, and per action: dimensions, ms[], and
// the position of its data in the file). The pixel data is walked once, frame
// by frame, only to compute `base` (lowest row with content); afterwards it is
// read on demand in PmdMon::frame().
// Cost: one sequential pass over the file when loading a species (~0.3 s on
// average over SPI-SD; up to a few seconds for the largest files).
// Returns false if the file is truncated or invalid (the caller does unload()).
static bool pmdLoadStreaming(PmdMon &m, File &f, uint32_t size, const char *path) {
  uint8_t hdr[7];
  if (!f.seek(0) || f.read(hdr, 7) != 7 || memcmp(hdr, "TPK2", 4) != 0) return false;
  uint8_t nActs = hdr[4];
  memcpy(&m.palCount, hdr + 5, 2);
  if (m.palCount > 256 || (uint32_t)7 + m.palCount * 2 > size) return false;
  if (f.read((uint8_t *)m.pal, m.palCount * 2) != (int)(m.palCount * 2)) return false;

  uint32_t pos = 7 + (uint32_t)m.palCount * 2;
  for (uint8_t i = 0; i < nActs && pos + 4 <= size; i++) {
    uint8_t ah[4];
    if (f.read(ah, 4) != 4) return false;
    pos += 4;
    uint8_t id = ah[0], w = ah[1], h = ah[2], nf = ah[3];
    if (id >= PMD_NACTS || nf > 24) return false;
    uint32_t frameBytes = (uint32_t)w * h;
    uint32_t bytes = (uint32_t)nf * 2 + frameBytes * nf;
    // ms[] and the data for all frames must fit in the file (truncated)
    if (w == 0 || h == 0 || nf == 0 || pos + bytes > size) return false;

    PmdAct &a = m.acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    a.data = nullptr;
    uint8_t msb[48];
    if (f.read(msb, nf * 2) != nf * 2) return false;
    for (uint8_t k = 0; k < nf; k++) {
      a.ms[k] = msb[k * 2] | (msb[k * 2 + 1] << 8);
      if (a.ms[k] == 0) a.ms[k] = 100;  // never 0: pmdFrameAt() would spin without advancing
    }
    pos += (uint32_t)nf * 2;
    a.fileOff = pos;

    if (!pmdEnsureBuf(m, frameBytes)) { Serial.println("sin RAM para un frame PMD"); return false; }
    uint8_t base = 1;
    for (uint8_t fr = 0; fr < nf; fr++) {
      if (f.read(m.fbuf, frameBytes) != (int)frameBytes) return false;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++)
          if (m.fbuf[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
    pos += frameBytes * nf;
  }
  strncpy(m.path, path, sizeof(m.path) - 1);
  m.path[sizeof(m.path) - 1] = 0;
  m.streaming = true;
  m.cacheAct = -1;
  m.cacheFrame = -1;
  m.loaded = true;
  return true;
}

const uint8_t *PmdMon::frame(uint8_t actId, uint8_t fi) {
  if (!loaded || actId >= PMD_NACTS) return nullptr;
  const PmdAct &a = acts[actId];
  if (!a.frames || fi >= a.frames) return nullptr;
  uint32_t fb = (uint32_t)a.w * a.h;
  if (!streaming) return a.data + (uint32_t)fi * fb;  // blob mode: direct pointer
  if (cacheAct == (int8_t)actId && cacheFrame == (int16_t)fi) return fbuf;  // same frame as before

  // Opened on every read (the handle is not kept): see note in sdmon.h
  File f = SDHOST.open(path, FILE_READ);
  if (!f) return nullptr;
  bool ok = f.seek(a.fileOff + (uint32_t)fi * fb) && f.read(fbuf, fb) == (int)fb;
  f.close();
  if (!ok) { cacheAct = -1; cacheFrame = -1; return nullptr; }
  cacheAct = (int8_t)actId;
  cacheFrame = (int16_t)fi;
  return fbuf;
}

bool PmdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SDHOST.open(path, FILE_READ);
  if (!f && shiny) {  // no shiny PMD: use the normal one
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dexNum);
    f = SDHOST.open(path, FILE_READ);
  }
  if (!f) return false;

  uint32_t size = f.size();
  if (size < 7 || size > 3UL * 1024 * 1024) { f.close(); return false; }
  if (!psramFound()) {  // no PSRAM: the whole blob does not fit -> streaming (see sdmon.h)
    bool ok = pmdLoadStreaming(*this, f, size, path);
    f.close();
    if (!ok) { unload(); return false; }
    Serial.printf("cargado %s en streaming (%u KB en la SD, %u B de RAM)\n",
                  path, (unsigned)(size / 1024), (unsigned)fbufCap);
    return true;
  }
  blob = (uint8_t *)bigAlloc(size);
  if (!blob || f.read(blob, size) != size || memcmp(blob, "TPK2", 4) != 0) {
    if (blob) { free(blob); blob = nullptr; }
    f.close();
    return false;
  }
  f.close();

  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *p = blob + 7 + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t i = 0; i < nActs && p + 4 <= end; i++) {
    uint8_t id = p[0], w = p[1], h = p[2], nf = p[3];
    p += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    // validates that ms[] and the frame data fit in the blob (truncated file)
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || p + bytes > end) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    for (uint8_t k = 0; k < nf; k++) {
      a.ms[k] = p[0] | (p[1] << 8);
      if (a.ms[k] == 0) a.ms[k] = 100;  // never 0: pmdFrameAt() would spin without advancing
      p += 2;
    }
    a.data = p;
    p += (uint32_t)w * h * nf;
    // lowest row with content in any frame: anchor by the feet
    uint8_t base = 1;
    for (uint8_t f = 0; f < nf; f++) {
      const uint8_t *fr = a.data + (uint32_t)f * w * h;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++)
          if (fr[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  loaded = true;
  Serial.printf("cargado %s (%u KB)\n", path, size / 1024);
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
  for (auto &a : acts) {
    a.w = a.h = a.frames = a.base = 0;
    a.data = nullptr;
    a.fileOff = 0;
  }
  loaded = false;
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
  File f = SDHOST.open("/mons/thumbs.bin", FILE_READ);
  if (!f) {
    Serial.println("sin thumbs.bin (galeria sin miniaturas)");
    return false;
  }
  uint32_t sz = f.size();
  // bounds the size: avoids an absurd ps_malloc with a corrupt file
  if (sz < 10 || sz > 2UL * 1024 * 1024) {
    Serial.println("thumbs.bin invalido (tamano)");
    f.close();
    return false;
  }
  if (!psramFound()) {  // no PSRAM: only the offset table; each thumbnail is read on request
    uint8_t hdr[6];
    if (!f.seek(0) || f.read(hdr, 6) != 6 || memcmp(hdr, "TPTH", 4) != 0) {
      Serial.println("thumbs.bin invalido");
      f.close();
      return false;
    }
    memcpy(&count, hdr + 4, 2);
    if ((uint32_t)6 + 4UL * count > sz || count == 0) {
      Serial.println("thumbs.bin invalido (tabla de offsets)");
      f.close();
      unload();
      return false;
    }
    offs = (uint32_t *)malloc(4UL * count);
    if (!offs || f.read((uint8_t *)offs, 4 * count) != (int)(4 * count)) {
      Serial.println("thumbs.bin: sin RAM/lectura para la tabla");
      f.close();
      unload();
      return false;
    }
    f.close();
    size = sz;
    streaming = true;
    loaded = true;
    Serial.printf("miniaturas en streaming: %u (%u KB en la SD)\n", count, (unsigned)(sz / 1024));
    return true;
  }
  data = (uint8_t *)bigAlloc(sz);
  if (!data || f.read(data, sz) != sz || memcmp(data, "TPTH", 4) != 0) {
    Serial.println("thumbs.bin invalido");
    if (data) { free(data); data = nullptr; }
    f.close();
    return false;
  }
  f.close();
  memcpy(&count, data + 4, 2);
  // the offset table (one per species) must fit entirely in what was read
  if ((uint32_t)6 + 4UL * count > sz) {
    Serial.println("thumbs.bin invalido (tabla de offsets)");
    unload();
    return false;
  }
  size = sz;
  loaded = true;
  Serial.printf("miniaturas cargadas: %u (%u KB)\n", count, sz / 1024);
  return true;
}

const uint8_t *SdThumbs::get(int16_t dex) {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  if (streaming) {
    if (cacheDex == dex) return tbuf;  // same thumbnail as the last one requested
    uint32_t soff = offs[dex - 1];
    if (soff > size - 3) return nullptr;  // the w,h,palCount header must fit
    File f = SDHOST.open("/mons/thumbs.bin", FILE_READ);
    if (!f) return nullptr;
    uint8_t h3[3];
    if (!f.seek(soff) || f.read(h3, 3) != 3) { f.close(); return nullptr; }
    uint32_t need = 3 + (uint32_t)h3[2] * 2 + (uint32_t)h3[0] * h3[1];
    if (need > size || soff > size - need) { f.close(); return nullptr; }  // corrupt offset/size
    if (tbufCap < need) {
      uint8_t *nb = (uint8_t *)realloc(tbuf, need);
      if (!nb) { f.close(); return nullptr; }
      tbuf = nb;
      tbufCap = need;
    }
    memcpy(tbuf, h3, 3);
    bool ok = f.read(tbuf + 3, need - 3) == (int)(need - 3);
    f.close();
    if (!ok) { cacheDex = -1; return nullptr; }
    cacheDex = dex;
    return tbuf;
  }
  uint32_t off;
  memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  // the offset comes unchecked from the file: without this, a truncated or
  // corrupt thumbs.bin caused reads outside the PSRAM allocation
  if (off > size - 3) return nullptr;  // the w,h,palCount header must fit (size >= 10 always)
  uint32_t need = 3 + (uint32_t)data[off + 2] * 2 + (uint32_t)data[off] * data[off + 1];
  if (need > size || off > size - need) return nullptr;  // and the whole blob too
  return data + off;
}

bool sdBegin() {
#if BOARD_IS_ROUND
  SDHOST.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  sdReady = SDHOST.begin("/sdcard", true /* 1-bit mode */, true /* format if mount fails */);
#else
  // SD on its own SPI bus (pins 18/19/23, CS 5); not shared with the display or touch.
  // Not formatted if it fails to mount (unlike the Waveshare): better not to touch someone else's card.
  // At most 3 open files (default 5): each costs a 4 KB sector cache in internal
  // RAM, which the CYD needs for the framebuffer and WiFi. The game opens one
  // file at a time; the LS serial command holds the directory plus one entry.
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SDHOST.begin(SD_CS, SPI, 20000000, "/sd", 3 /* max open files */);
#endif
  if (sdReady) {
    Serial.printf("SD montada: %llu MB\n", SDHOST.cardSize() / (1024ULL * 1024ULL));
    SDHOST.mkdir("/mons");
  } else {
    Serial.println("SD no detectada (el juego usa los sprites de flash)");
  }
  return sdReady;
}

bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  char path[24];
  snprintf(path, sizeof(path), "/mons/%s%03u.bin", shiny ? "s" : "", dexNum);
  File f = SDHOST.open(path, FILE_READ);
  if (!f && shiny) {  // no shiny variant: use the normal one
    snprintf(path, sizeof(path), "/mons/%03u.bin", dexNum);
    f = SDHOST.open(path, FILE_READ);
  }
  if (!f) {
    Serial.printf("no existe %s\n", path);
    return false;
  }

  char magic[4];
  uint16_t header[4];
  if (f.read((uint8_t *)magic, 4) != 4 || memcmp(magic, "TPK1", 4) != 0 ||
      f.read((uint8_t *)header, 8) != 8) {
    f.close();
    return false;
  }
  w = header[0];
  h = header[1];
  frames = header[2];
  frameMs = header[3];
  // bounds the dimensions: avoids an overflowed or absurd size with a corrupt file
  if (f.read((uint8_t *)&palCount, 2) != 2 || palCount > 256 ||
      w == 0 || w > 256 || h == 0 || h > 256 || frames == 0 || frames > 64) {
    f.close();
    return false;
  }
  if (f.read((uint8_t *)pal, palCount * 2) != palCount * 2) {
    f.close();
    return false;
  }

  uint32_t size = (uint32_t)w * h * frames;
  data = (uint8_t *)bigAlloc(size);
  if (!data) {
    Serial.println("sin PSRAM para el sprite");
    f.close();
    return false;
  }
  uint32_t got = f.read(data, size);
  f.close();
  if (got != size) {
    Serial.printf("%s truncado (%u de %u)\n", path, got, size);
    unload();
    return false;
  }

  // integer zoom so the creature is ~200 px tall on screen
  scale = 200 / h;
  if (scale < 2) scale = 2;
  if (scale > 5) scale = 5;

  Serial.printf("cargado %s: %ux%u x%u frames @%ums, escala %u\n",
                path, w, h, frames, frameMs, scale);
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

// ---------------------------------------------------------------------------
// USB upload protocol (to fill the SD without removing it from the board):
//   PUT <path> <bytes>\n  + raw data       -> "OK" ... "DONE"
//   LS\n                                   -> listing of /mons
// Use with tools/send_sd.py
// ---------------------------------------------------------------------------

bool sdSerialCommand(const String &line) {
  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' ');
    String path = line.substring(4, sp);
    uint32_t size = line.substring(sp + 1).toInt();
    if (!sdReady || size == 0 || size > 4 * 1024 * 1024) {
      Serial.println("ERR");
      return true;
    }
    if (!path.startsWith("/")) path = "/" + path;
    // restricts writes to /mons/: the path arrives as-is from the serial line,
    // unsanitized, so a crafted PUT could write anywhere on the card. The two
    // real clients (tools/send_sd.py and web/index.html) already send names
    // with the mons/ prefix.
    if (!path.startsWith("/mons/") || path.indexOf("..") >= 0) {
      Serial.println("ERR");
      return true;
    }
    // FILE_WRITE APPENDS if the file already exists, so retrying a half-written
    // one made it longer instead of replacing it: it left a corrupt sprite larger
    // than the original. This matters more now that the installer resumes
    // interrupted transfers, because it retries exactly the ones that failed.
    if (SDHOST.exists(path)) SDHOST.remove(path);
    File f = SDHOST.open(path, FILE_WRITE);
    if (!f) {
      Serial.println("ERR");
      return true;
    }
    Serial.println("OK");
    static uint8_t buf[2048];
    uint32_t remaining = size;
    Serial.setTimeout(5000);
    while (remaining > 0) {
      size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
      size_t n = Serial.readBytes(buf, want);
      if (n == 0) break;  // timeout
      // without checking the return value, a full card or a write failure still
      // reported DONE and left a truncated file on the SD: exactly the corrupt
      // input we then have to guard against when loading the sprite
      if (f.write(buf, n) != n) break;
      remaining -= n;
      Serial.println("#");  // ack: ready for the next block
    }
    f.close();
    Serial.setTimeout(1000);
    sdDirty = (remaining == 0);
    Serial.println(remaining == 0 ? "DONE" : "ERR");
    return true;
  } else if (line == "SDINFO") {  // remote diagnosis of "my SD is not recognized"
    Serial.printf("total=%llu used=%llu\n", SDHOST.totalBytes(), SDHOST.usedBytes());
    Serial.println("DONE");
    return true;
  } else if (line == "LS") {
    File dir = SDHOST.open("/mons");
    if (dir) {
      File e;
      while ((e = dir.openNextFile())) {
        Serial.printf("%s %u\n", e.name(), (uint32_t)e.size());
        e.close();
      }
      dir.close();
    }
    Serial.println("DONE");
    return true;
  }
  return false;
}

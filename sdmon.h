#pragma once
#include <Arduino.h>

// TPK1 animated sprite (legacy format, fallback path). The project uses
// PMD/TPK2 (PmdMon) for everything; this path stays inactive if there is no NNN.bin on the SD.
// The indexed data lives in PSRAM; the palette is RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // integer zoom factor when drawing
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indices (0xFF = transparent)

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// PMD sprite actions (TPK2 format)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // row+1 of the lowest pixel (anchor by the feet, not the canvas)
  uint16_t ms[24];
  const uint8_t *data = nullptr;  // blob mode: frames * w * h inside the blob
  uint32_t fileOff = 0;           // streaming mode: offset of that data in the file
};

// multi-action PMD sprite loaded from the SD.
// Two modes, chosen in load() depending on whether there is PSRAM:
//   - blob (with PSRAM, like the Waveshare): the whole file lives in memory and
//     frame() returns a pointer inside the blob. Original behavior.
//   - streaming (no PSRAM, e.g. the CYD): only the headers are kept; frame()
//     reads ONE frame from the SD into a small buffer (caching the last frame).
//     The file is opened on every read (the handle is not kept), so receiving
//     files over USB (PUT) never clashes with an open sprite.
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  PmdAct acts[PMD_NACTS];

  // --- streaming mode state ---
  bool streaming = false;
  char path[24] = {0};
  uint8_t *fbuf = nullptr;      // buffer for ONE frame (the largest across actions)
  uint32_t fbufCap = 0;
  int8_t cacheAct = -1;         // which frame is currently in fbuf
  int16_t cacheFrame = -1;

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
  // Data (w*h indices, 0xFF = transparent) for frame fi of action actId, or
  // nullptr if it could not be read. In streaming mode the pointer is only valid
  // until the next call to frame() on THIS PmdMon.
  const uint8_t *frame(uint8_t actId, uint8_t fi);
};

// gallery thumbnails. With PSRAM, the whole thumbs.bin is in memory; without it,
// only the offset table, and each thumbnail is read from the SD on request.
struct SdThumbs {
  bool loaded = false;
  uint8_t *data = nullptr;
  uint16_t count = 0;
  uint32_t size = 0;  // file size in bytes: bounds the offsets
  // --- streaming mode ---
  bool streaming = false;
  uint32_t *offs = nullptr;     // offset table (count entries)
  uint8_t *tbuf = nullptr;      // one thumbnail
  uint32_t tbufCap = 0;
  int16_t cacheDex = -1;
  bool load();
  void unload();  // frees the blob: reload after receiving a new thumbs.bin
  // w,h,palCount,pal[],idx[]. In streaming mode the pointer is valid until the next get().
  const uint8_t *get(int16_t dex);
};
extern SdThumbs thumbs;

bool sdBegin();                 // mounts the SD (SDMMC 1-bit), true if a card is present
bool sdSerialCommand(const String &line);  // PUT/LS over USB; true if it handles it
extern bool sdReady;
extern bool sdDirty;  // true after receiving files: reload sprite

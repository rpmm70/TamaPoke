#include "audio.h"
#include "boards/board_select.h"

#if BOARD_CYD_RECT

#include <Arduino.h>
#include <Preferences.h>

// The CYD has no ES8311 codec and no I2S: just a speaker pin (SPEAKER_PIN).
// Instead of PCM, square tones via PWM (LEDC), with the SAME notes as the I2S
// backend (table copied from audio.cpp: if it is changed there, change it here).
// Same structure as the original: sfxPlay() only queues an id and a FreeRTOS
// task plays the notes, so the game loop never blocks.
//
// Requires ESP32 Arduino core 3.x (per-pin ledcAttach/ledcWriteTone API),
// the same requirement ESP_I2S.h already imposes on the original backend.
//
// TODO(addon): if an I2S DAC/amplifier is added on the free pins, this is the
// only file to replace; audio.h does not change.

namespace {

struct Note { uint16_t f, ms; };  // f==0 -> silence of ms milliseconds
const Note N_TAP[]    = {{880, 35}};
const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
const Note N_PLAY[]   = {{784, 45}, {988, 60}};
const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
const Note N_DENY[]   = {{300, 110}, {200, 170}};
const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};

struct SfxDef { const Note *n; uint8_t len; };
const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

bool gReady = false;
bool gOn = true;
bool gSleeping = false;
QueueHandle_t gQ = nullptr;

void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(gQ, &id, portMAX_DELAY) && gOn && !gSleeping && id < SFX_COUNT) {
      const SfxDef &d = SFX[id];
      for (uint8_t i = 0; i < d.len; i++) {
        if (d.n[i].f) ledcWriteTone(SPEAKER_PIN, d.n[i].f);
        else ledcWriteTone(SPEAKER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(d.n[i].ms));
      }
      ledcWriteTone(SPEAKER_PIN, 0);  // silence when the effect ends
    }
  }
}

}  // namespace

void audioBegin() {
  if (!ledcAttach(SPEAKER_PIN, 2000, 10)) {
    Serial.println("LEDC audio fallo (audio off)");
    return;
  }
  ledcWriteTone(SPEAKER_PIN, 0);

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  gQ = xQueueCreate(8, sizeof(uint8_t));
  gReady = true;
  xTaskCreatePinnedToCore(audioTask, "audio", 3072, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_HATCH);  // startup jingle (confirms audio works)
}

void sfxPlay(uint8_t id) {
  if (gReady && gOn && !gSleeping && gQ) xQueueSend(gQ, &id, 0);  // dropped if the queue is full
}

void audioSetEnabled(bool on) {
  gOn = on;
  if (!on && gReady) ledcWriteTone(SPEAKER_PIN, 0);
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  if (gSleeping == sleeping) return;
  gSleeping = sleeping;
  if (sleeping && gReady) ledcWriteTone(SPEAKER_PIN, 0);
}

#endif  // BOARD_CYD_RECT

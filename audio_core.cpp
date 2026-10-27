// Board-independent half of the sound engine: the effect note tables, the
// queue and task that play them without blocking the game loop, and the
// persisted sound switch. The hardware side lives behind audio_backend.h.

#pragma region Includes
#include <Arduino.h>
#include <Preferences.h>
#include "audio.h"
#include "audio_backend.h"
#include "countof.h"
#include "nvs_settings.h"
#pragma endregion

#pragma region Configuration
#define AUDIO_QUEUE_LENGTH 8        // pending effects; more are dropped
#define AUDIO_TASK_PRIORITY 1
#define AUDIO_TASK_CORE 0
#define AUDIO_NVS_KEY_SOUND "snd"
#pragma endregion

namespace {

#pragma region Note tables
// Square tones (Game Boy style). frequencyHz == 0 is a silence.
struct Note { uint16_t frequencyHz, durationMs; };

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
#pragma region Wild battle and ball game notes
// Kept under 400 ms each so a battle turn or a bounce never queues up a backlog.
const Note N_WILD_APPEAR[] = {{392, 50}, {523, 50}, {0, 15}, {784, 60}, {1047, 110}};
const Note N_BATTLE_HIT[]  = {{1175, 35}, {1568, 45}};
const Note N_BATTLE_HURT[] = {{300, 60}, {190, 80}};
const Note N_BATTLE_WIN[]  = {{659, 60}, {784, 60}, {988, 80}, {1319, 170}};
const Note N_BATTLE_LOSS[] = {{392, 110}, {330, 110}, {196, 170}};
const Note N_CATCH_OK[]    = {{784, 55}, {988, 65}, {1319, 180}};
const Note N_CATCH_FAIL[]  = {{523, 80}, {392, 80}, {0, 15}, {247, 180}};
const Note N_BALL_BOUNCE[] = {{820, 20}, {620, 25}};
const Note N_BALL_MISS[]   = {{360, 60}, {210, 100}};
#pragma endregion
#pragma endregion

#pragma region Effect table
struct SfxDef { const Note *notes; uint8_t noteCount; };
#define SFX_ENTRY(noteTable) { noteTable, (uint8_t)countof(noteTable) }

// same order as the Sfx enum in audio.h
const SfxDef SFX[] = {
  SFX_ENTRY(N_TAP), SFX_ENTRY(N_EAT), SFX_ENTRY(N_PLAY), SFX_ENTRY(N_HEART),
  SFX_ENTRY(N_HATCH), SFX_ENTRY(N_EVOLVE), SFX_ENTRY(N_MEDAL), SFX_ENTRY(N_DENY),
  SFX_ENTRY(N_BYE), SFX_ENTRY(N_LEVEL),
  SFX_ENTRY(N_WILD_APPEAR), SFX_ENTRY(N_BATTLE_HIT), SFX_ENTRY(N_BATTLE_HURT),
  SFX_ENTRY(N_BATTLE_WIN), SFX_ENTRY(N_BATTLE_LOSS), SFX_ENTRY(N_CATCH_OK),
  SFX_ENTRY(N_CATCH_FAIL), SFX_ENTRY(N_BALL_BOUNCE), SFX_ENTRY(N_BALL_MISS),
};
static_assert(countof(SFX) == SFX_COUNT, "SFX[] must have one entry per Sfx id");
#pragma endregion

#pragma region State
bool audioReady = false;     // the backend started: the queue and task exist
bool soundEnabled = true;    // the persisted switch
bool petSleeping = false;
QueueHandle_t sfxQueue = nullptr;
#pragma endregion

#pragma region Internal helpers
void notifyBackendPower() {
  audioBackendPowerChanged(audioReady && soundEnabled && !petSleeping);
}

void audioTask(void *) {
  uint8_t sfxId;
  for (;;) {
    if (xQueueReceive(sfxQueue, &sfxId, portMAX_DELAY) && soundEnabled && !petSleeping && audioReady &&
        sfxId < SFX_COUNT) {
      const SfxDef &effect = SFX[sfxId];
      for (uint8_t noteIndex = 0; noteIndex < effect.noteCount; noteIndex++)
        audioBackendPlayNote(effect.notes[noteIndex].frequencyHz, effect.notes[noteIndex].durationMs);
      audioBackendSilence();
    }
  }
}
#pragma endregion

}  // namespace

#pragma region Public API
void audioBegin() {
  if (!audioBackendBegin()) return;

  #pragma region Persisted sound switch
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, true);
  soundEnabled = preferences.getBool(AUDIO_NVS_KEY_SOUND, true);
  preferences.end();
  #pragma endregion

  audioReady = true;
  notifyBackendPower();
  sfxQueue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", AUDIO_TASK_STACK_BYTES, nullptr, AUDIO_TASK_PRIORITY, nullptr,
                          AUDIO_TASK_CORE);
  sfxPlay(SFX_HATCH);  // startup jingle (confirms audio works)
}

void sfxPlay(uint8_t id) {
  if (!audioReady || !soundEnabled || !sfxQueue) return;
#if AUDIO_DROP_SFX_WHILE_SLEEPING
  if (petSleeping) return;
#endif
  xQueueSend(sfxQueue, &id, 0);  // dropped if the queue is full
}

void audioSetEnabled(bool on) {
  soundEnabled = on;
  notifyBackendPower();
  Preferences preferences;
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBool(AUDIO_NVS_KEY_SOUND, on);
  preferences.end();
}

bool audioEnabled() { return soundEnabled; }

void audioSetSleeping(bool sleeping) {
  if (petSleeping == sleeping) return;
  petSleeping = sleeping;
  notifyBackendPower();  // no effects while asleep: the backend can power down
}
#pragma endregion
